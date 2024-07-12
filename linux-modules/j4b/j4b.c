/**
 * @file   j4b.c
 * @author pcbjunkie 
 */
 
#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <asm/uaccess.h>          // Required for the copy to user function
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/delay.h> 
#include <linux/timex.h>
#include <linux/timekeeping.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "j4b.h"

// #define  J4B_DEBUG

MODULE_DESCRIPTION("JAMMA 4-bit bus interface driver for PJ board");  ///< The description -- see modinfo
MODULE_LICENSE("GPL");            // The license type -- this affects available functionality
MODULE_AUTHOR("pcbjunkie");       // The author -- visible when you use modinfo
MODULE_VERSION("1.0");            ///< A version number to inform users

#define  DEVICE_NAME "j4b"        // The device will appear at /dev/j4b using this value
#define  CLASS_NAME  "j4b"        // The device class -- this is a character device driver
 
static int majorNumber;           // Stores the device number -- determined automatically


// GPIO pin function defines for the J4B bus
// changed from the old pi version
#define D0  9 
#define D1  10
#define D2  11
#define D3  19
#define RDY 25
#define RW  26
#define CLK 27

static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  j4bClass  = NULL; ///< The device-driver class struct pointer
static struct device* j4bDevice = NULL; ///< The device-driver device struct pointer

static struct timer_list read_timer;

DEFINE_SPINLOCK(mLock);
DEFINE_SPINLOCK(cLock);
unsigned long lockflags;

#define J4B_REFRESH_TIME HZ/240 
 
// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
 
/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

int j4b_values[J4B_MAX_DEVICES];

#define J4B_READ_QUEUE_SIZE 5
static unsigned int j4b_read_nibble_queue[J4B_READ_QUEUE_SIZE];      // queue of all received nibbles  
static int j4b_read_queue_len = 0;
static int j4b_read_irqnum = 0;

static char *j4b_device_names[J4B_MAX_DEVICES] = {"j4binput0", "j4binput1", "j4bsnd", "j4boutput", "j4bv5", "j4bv12", "j4bv5pi", "j4bv33pi"};
static int j4b_device_nibbles[J4B_MAX_DEVICES] = {4, 4, 0, 0, 2, 2, 2, 2};

// initialize gpio and request pins for bus
void j4b_gpio_init(void){
  printk(KERN_DEBUG "J4B: starting gpio...");
  gpio_request(D0, "D0");
  gpio_request(D1, "D1");
  gpio_request(D2, "D2");
  gpio_request(D3, "D3");
  gpio_request(CLK, "CLK");
  gpio_request(RDY, "RDY");
  gpio_request(RW, "RW");
}

// set bus mode to write (rpi tells pj) 
void j4b_gpio_set_write(void){
  gpio_direction_output(D0, 0);
  gpio_direction_output(D2, 0);
  gpio_direction_output(D3, 0);
  gpio_direction_output(D3, 0);
  gpio_direction_output(CLK, 0);
  gpio_direction_input(RDY);
  gpio_direction_output(RW, 0);
}

// set bus mode to read (pj tells rpi) 
void j4b_gpio_set_read(void){
  gpio_direction_input(D0);
  gpio_direction_input(D2);
  gpio_direction_input(D3);
  gpio_direction_input(D3);
  gpio_direction_input(CLK);
  gpio_direction_output(RDY, 0);
  gpio_direction_input(RW);
}

static irqreturn_t j4b_clk_read_interrupt(int irq, void* dev_id) {
   unsigned int rw;
   unsigned int val;
   unsigned int cmd;
   unsigned int dev;
   // struct j4b_msg msg;
   int nibbles;
   int nibblecount;
   int i;

   spin_lock_irqsave(&cLock, lockflags);

   rw = gpio_get_value(RW);
   val = (gpio_get_value(D3) << 3) | (gpio_get_value(D2) << 2) | (gpio_get_value(D1) << 1) | gpio_get_value(D0);
   #ifdef J4B_DEBUG
      printk(KERN_DEBUG "Data lines 0x%x (%d%d%d%d)\n", val, gpio_get_value(D3), gpio_get_value(D2), gpio_get_value(D1), gpio_get_value(D0));
   #endif

   if (j4b_read_queue_len == J4B_READ_QUEUE_SIZE){
      #ifdef J4B_DEBUG
         printk(KERN_DEBUG "Read queue is full\n");
      #endif
   }
   else {
      // store this value on the queue;
      j4b_read_nibble_queue[j4b_read_queue_len] = val;
      j4b_read_queue_len++;

      cmd = j4b_read_nibble_queue[0];

      if ((cmd & J4B_MSG_VAL_SIZE) == J4B_MSG_8BIT_VAL) nibbles= 2;
      else if ((cmd & J4B_MSG_VAL_SIZE) == J4B_MSG_16BIT_VAL) nibbles = 4;
      nibblecount = nibbles;

      // if we don't have enough data in the queue, wait for more
      if (nibblecount >= j4b_read_queue_len) {
         if (!rw){
            #ifdef J4B_DEBUG
               printk(KERN_DEBUG "R/W is not set during message, ignoring received data\n");
            #endif
            // clean up queue if failed to read
            j4b_read_queue_len = 0;
            gpio_set_value(RDY, 0);
         }
         spin_unlock_irqrestore(&cLock, lockflags);
         return(IRQ_HANDLED);
      }

      // at this point, we've got all the nibbles, ask msp to stop sending
      gpio_set_value(RDY, 0);

      i = 1;
      val = 0; 
      while (nibblecount > 0){
         val = (val << 4) | j4b_read_nibble_queue[i];
         nibblecount--;
         i++;
      }

      #ifdef J4B_DEBUG
         printk(KERN_DEBUG "Received j4b message, cmd %x val %x nibs %x left %x\n", cmd, val, nibbles, nibblecount);
      #endif

      dev = cmd & J4B_MSG_CMD;
      if (nibbles == j4b_device_nibbles[dev]){
         // at this point message should be completed and r/w should be 0
         rw = gpio_get_value(RW);
         if (rw){
            #ifdef J4B_DEBUG
               printk(KERN_DEBUG "R/W set after message, ignoring data cmd %x val %x nibs %x left %x\n", cmd, val, nibbles, nibblecount);
            #endif
         }
         else {
            #ifdef J4B_DEBUG
               printk(KERN_DEBUG "Received completed j4b message, cmd %x val %x nibs %x\n", cmd, val, nibbles);
            #endif
            j4b_values[cmd & J4B_MSG_CMD] = val; 
         }
      }
      else{
         #ifdef J4B_DEBUG
            printk(KERN_DEBUG "Got %d of expected %d nibbles for dev %d, ignoring data cmd %x val %x\n", nibbles, j4b_device_nibbles[dev], dev, cmd, val);
         #endif
      } 
   }

   // message completed, or rejected, clean queue
   j4b_read_queue_len = 0;
   gpio_set_value(RDY, 0);

   // stop reading for now 
   // free_irq(j4b_read_irqnum, NULL);       
   // j4b_read_irqnum = 0;

   spin_unlock_irqrestore(&cLock, lockflags);
   return(IRQ_HANDLED);
}


// this function sets up the clk edge irq and puts pi in read mode
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
void j4b_start_reading_messages(unsigned long idx){
#else
void j4b_start_reading_messages(struct timer_list *t){
#endif
   spin_lock_irqsave(&mLock, lockflags);
   
   // sync up start of message with msp 
   // set pins to read 
   j4b_gpio_set_read();
  
   if (!j4b_read_irqnum){ 
      j4b_read_irqnum = gpio_to_irq(CLK);
	
      if (request_irq(j4b_read_irqnum, j4b_clk_read_interrupt, IRQF_TRIGGER_RISING|IRQF_ONESHOT, "gpio_rising", NULL) ) {
        printk(KERN_ERR "Trouble requesting IRQ %d for J4B reads", j4b_read_irqnum);
        return;
      }
      else{
	printk(KERN_DEBUG "Request for J4B read IRQ %d is successful\n", j4b_read_irqnum);
      }
   }

   // get ready to receive
   mod_timer(&read_timer, jiffies + J4B_REFRESH_TIME);
   gpio_set_value(RDY, 1);
   spin_unlock_irqrestore(&mLock, lockflags);

   return;
}

void j4b_gpio_close(void){
  printk(KERN_DEBUG "J4B: stopping gpio...");
  if (j4b_read_irqnum) free_irq(j4b_read_irqnum, NULL);
  gpio_free(D0);
  gpio_free(D1);
  gpio_free(D2);
  gpio_free(D3);
  gpio_free(CLK);
  gpio_free(RDY);
  gpio_free(RW);
}

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init j4b_init(void){
   int i, rc;

   #ifdef J4B_DEBUG 
      printk(KERN_DEBUG "J4B: Initializing the J4B LKM\n");
   #endif 

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "J4B failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "J4B: registered correctly with major number %d\n", majorNumber);
 
   // Register the device class
   #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
     j4bClass = class_create(CLASS_NAME);
   #else
     j4bClass = class_create(THIS_MODULE, CLASS_NAME);
   #endif
   if (IS_ERR(j4bClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(j4bClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "J4B: device class registered correctly\n");
 
   // Register the device driver
   // j4bDevice = device_create(j4bClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   rc = 0;

   for (i = 0; i < J4B_MAX_DEVICES; i++){
      printk(KERN_INFO "J4B: creating device %s\n", j4b_device_names[i]);
      if (device_create(j4bClass, NULL, MKDEV(majorNumber, i), NULL, j4b_device_names[i], i) == NULL){
         rc = -1;
         break;
      }
   }
   if (rc < 0){              
      printk(KERN_ALERT "Failed to create the device for %s\n", j4b_device_names[i]);
      class_destroy(j4bClass);
      for (; i >= 0; i--){
         unregister_chrdev(majorNumber, j4b_device_names[i]);
         printk(KERN_INFO "J4B: removing device %s\n", j4b_device_names[i]);
      }
      return PTR_ERR(j4bDevice);
   }
   #ifdef J4B_DEBUG 
      printk(KERN_INFO "J4B: devices created successfully\n"); // Made it! device was initialized
   #endif 

   // one global timer for moving data in / out of the bus
   #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
     setup_timer(&read_timer, j4b_start_reading_messages, 0);
   #else
     timer_setup(&read_timer, j4b_start_reading_messages, 0);
   #endif

   mod_timer(&read_timer, jiffies + J4B_REFRESH_TIME);
 
   // initialize the gpio module and setup pins 
   j4b_gpio_init(); 

   return 0;
}
 
/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit j4b_exit(void){
   int i;

   for (i = J4B_MAX_DEVICES - 1; i >= 0; i--){
         device_destroy(j4bClass, MKDEV(majorNumber, i));
         unregister_chrdev(majorNumber, j4b_device_names[i]);
         printk(KERN_INFO "J4B: unloading device %d: %s\n", i, j4b_device_names[i]);
   }

   // kill polling timer
   del_timer(&read_timer);

   // shutdown the gpio module and free pins 
   j4b_gpio_close(); 

   // device_destroy(j4bClass, MKDEV(majorNumber, 0));     // remove the device
   // unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   class_unregister(j4bClass);                          // unregister the device class
   class_destroy(j4bClass);                             // remove the device class

   printk(KERN_INFO "J4B: Module unloaded\n");
}
 
/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *i, struct file *f){
   #ifdef J4B_DEBUG 
      printk(KERN_DEBUG "J4B: Opening device %d. Device has been opened %d time(s)\n", MINOR(i->i_rdev), numberOpens);
   #endif
   numberOpens++;
   return 0;
}
 
/* get and put message functions are used by all other child drivers */
EXPORT_SYMBOL(j4b_getValue);
EXPORT_SYMBOL(j4b_putValue);

int j4b_getValue(int mdev, int *val){
   
   if (mdev < 0 || mdev >= J4B_MAX_DEVICES){
      #ifdef J4B_DEBUG 
         printk(KERN_DEBUG "J4B: invalid device %d getting message\n", mdev);
      #endif
      return 0;
   }

   *val = j4b_values[mdev];

   #ifdef J4B_DEBUG 
      printk(KERN_DEBUG "J4B: Get val %x, device %d\n", *val, mdev);
   #endif
   return 1;
}

int j4b_putValue(int mdev, int val){
   
   if (mdev < 0 || mdev >= J4B_MAX_DEVICES){
      #ifdef J4B_DEBUG 
         printk(KERN_DEBUG "J4B: invalid device %d putting value %x\n", mdev, val);
      #endif
      return 0;
   }
   #ifdef J4B_DEBUG 
      printk(KERN_DEBUG "J4B: Put val %x, device %d\n", val, mdev);
   #endif

   j4b_values[mdev] = val;

   return 1;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int err = 0;
   int msglen = 0;
   int m = iminor(filep->f_path.dentry->d_inode);
   int val;
   struct j4b_msg msg;

   msglen = sizeof(struct j4b_msg);

   if (len < msglen){
      printk(KERN_INFO "J4B: Insufficient buffer of size %d, need %d\n", len, msglen);
      return -EFAULT;
   }

   if (!j4b_getValue(m, &val)){
      #ifdef J4B_DEBUG 
         printk(KERN_DEBUG "J4B: Nothing to read\n");
      #endif
      return 0;
   }
   msg.cmd = m;
   msg.val = val;

   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   #ifdef J4B_DEBUG 
      printk(KERN_DEBUG "J4B: Reading %d characters from device %d\n", msglen, m);
   #endif

   err = copy_to_user(buffer, &msg, msglen);
   // printk(KERN_DEBUG "dev = %d msg = %x.%x ret = %d size = %d\n", m, msg.cmd, msg.val, err, msglen);
 
   if (err == 0){            // if true then have success
      #ifdef J4B_DEBUG
         printk(KERN_DEBUG "J4B: Sent %d characters to the user\n", sizeof(struct j4b_msg));
      #endif
      return (msglen);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "J4B: Failed to send %d characters to the user\n", err);
      return -EFAULT; // Failed -- return a bad address message (i.e. -14)
   }
}
 
/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   int m = iminor(filep->f_path.dentry->d_inode);
   struct j4b_msg *msg;

   if (len < sizeof(struct j4b_msg)){
      printk(KERN_INFO "J4B: Insufficient buffer of size %d, need %d\n", len, sizeof(struct j4b_msg));
      return -EFAULT;
   }

   msg = (struct j4b_msg *)buffer;      
   if (!j4b_putValue(m, msg->val)) return ENOSPC;      
   #ifdef J4B_DEBUG 
      printk(KERN_DEBUG "J4B: Writing %d characters to device %d\n", len, m);
   #endif
   return sizeof(struct j4b_msg);
}
 
/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   #ifdef J4B_DEBUG 
     printk(KERN_INFO "J4B: Device successfully closed\n");
   #endif
   return 0;
}
 
/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(j4b_init);
module_exit(j4b_exit);
