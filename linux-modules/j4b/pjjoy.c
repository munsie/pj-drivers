/*
 *
 * Analog joystick driver for PJ rpi to jamma board interface 
 * Copyright (c) 2019 pcbjunkie (pcbjunkie.net) 
 *
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/gameport.h>
#include <linux/jiffies.h>
#include <linux/timex.h>
#include <linux/timekeeping.h>
#include <linux/version.h>

#include "pjjoy.h"
#include "../j4b/j4b.h"

// #define J4B_DEBUG 

#define DRIVER_DESC "PJ - Rasperry Pi to JAMMA adapter joystick driver"
#define JAMMA_JOY_NAME "PJ arcade controls"
#define JAMMA_MAX_NAME_LENGTH  128
#define JAMMA_MAX_PHYS_LENGTH	32

MODULE_AUTHOR("pcbjunkie <pcbjunkie@gmail.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#define MK_REFRESH_TIME	HZ/120

static short pjjoy_axes[ JAMMA_NUM_AXIS ] = { ABS_X, ABS_Y };
static short pjjoy_dirs[ JAMMA_NUM_AXIS ][ JAMMA_NUM_AXIS_DIRS ] = { { JAMMA_LEFT, JAMMA_RIGHT } , { JAMMA_UP, JAMMA_DOWN } };
static short pjjoy_offs[ JAMMA_NUM_AXIS ][ JAMMA_NUM_AXIS_DIRS ] = { { JAMMA_AXIS_LEFT_OFFSET, JAMMA_AXIS_RIGHT_OFFSET } , { JAMMA_AXIS_UP_OFFSET, JAMMA_AXIS_DOWN_OFFSET } }; 

/* button value map */ 
static short pjjoy_keys[] = { BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7, BTN_8, BTN_START, BTN_EXTRA, BTN_SELECT, BTN_MODE };
static short pjjoy_btns[] = { JAMMA_B1, JAMMA_B2, JAMMA_B3, JAMMA_B4, JAMMA_B5, JAMMA_B6, JAMMA_B7, JAMMA_B8, JAMMA_START, JAMMA_EXTRA, JAMMA_SELECT, JAMMA_MODE };

/* map joystick to j4b device */
static short pjjoy_j4b_devs[] = { J4B_DEV_INPUT_0, J4B_DEV_INPUT_1 };

struct pjjoy {
	struct timer_list timer;
	int idx;
        int j4b_devnum;
	struct input_dev *dev;
	int mask;
	// short *buttons;
	char *name;
	char phys[JAMMA_MAX_PHYS_LENGTH];
};

static struct pjjoy js[JAMMA_NUM_DEVS];
// struct mutex pjjoy_mutex;

DEFINE_SPINLOCK(mLock);
unsigned long lockflags;

/*
 * pjjoy_decode() decodes pjjoy joystick data and reports input events.
 */

static void pjjoy_decode(struct pjjoy *joy, int switches) {
	struct input_dev *dev;
	int i, j, k, axval;
	
	dev = joy->dev;

	// printk(KERN_INFO "%s: decoding device switches %d\n", joy->phys, switches);
        // iterate over the button map and report state for each one 

	for (i = 0; i < JAMMA_NUM_BUTTONS; i++){
		if (switches & pjjoy_btns[i]) input_report_key(dev, pjjoy_keys[i], 1);
		else input_report_key(dev, pjjoy_keys[i], 0);
	}

        // iterate over the axis map and report direction offset value 
	for (j = 0; j < JAMMA_NUM_AXIS; j++){
		axval = JAMMA_AXIS_CENTER;
		for (k = 0; k < JAMMA_NUM_AXIS_DIRS; k++){
			if (pjjoy_dirs[j][k] & switches){
				axval += pjjoy_offs[j][k];
			}
		}
		input_report_abs(dev, pjjoy_axes[j], axval);
	}
        
	input_sync(dev);
}

/*
 * pjjoy_poll() polls the joysticks from j4b queue data
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void pjjoy_poll(unsigned long idx){
#else
static void pjjoy_poll(struct timer_list *t){
	int idx;
#endif
        struct pjjoy *joy;
	struct timer_list *timer;
	int val;

	spin_lock_irqsave(&mLock, lockflags);	

        #if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	joy = &js[(int)idx]; 
	#else
	joy = from_timer(joy, t, timer);
	idx = joy->idx;
	#endif

	timer = &(joy->timer); 
	mod_timer(timer, jiffies + MK_REFRESH_TIME);

	spin_unlock_irqrestore(&mLock, lockflags);

	#ifdef J4B_DEBUG
		printk(KERN_INFO "%s: polling device %ld\n", joy->phys, idx);
	#endif	
	if (j4b_getValue(joy->j4b_devnum, &val)){
		#ifdef J4B_DEBUG
			printk(KERN_DEBUG "%s: read %d from device %ld\n", joy->phys, val, idx);
		#endif	
		pjjoy_decode(joy, val);
	}
}

/*
 * pjjoy_open() is a callback from the input open routine.
 */

static int pjjoy_open(struct input_dev *dev)
{
        struct pjjoy *joy;
	struct timer_list *timer;

	joy = input_get_drvdata(dev);
	timer = &(joy->timer); 
	mod_timer(timer, jiffies + MK_REFRESH_TIME);

	return 0;
}

/*
 * pjjoy_close() is a callback from the input close routine.
 */

static void pjjoy_close(struct input_dev *dev)
{
        struct pjjoy *joy;
	struct timer_list *timer;

	joy = input_get_drvdata(dev);
	timer = &(joy->timer); 
	del_timer(timer);
}

/*
 * pjjoy_init_device()
 */

static int pjjoy_init_device(struct pjjoy *pjjoy, int index)
{
	struct input_dev *input_dev;
	int i, error;

	pjjoy->name = JAMMA_JOY_NAME;
	pjjoy->idx = index;

	snprintf(pjjoy->phys, sizeof(pjjoy->phys), "input%d", index);
	printk(KERN_INFO "%s: Registering device %d\n", pjjoy->phys, index);

	pjjoy->j4b_devnum = pjjoy_j4b_devs[index];
	pjjoy->dev = input_dev = input_allocate_device();
	if (!input_dev){
	        printk(KERN_ERR "%s: Unable to allocate device\n", pjjoy->phys);
		return -ENOMEM;
	}

	input_dev->name = pjjoy->name;
	input_dev->phys = pjjoy->phys;
	// input_dev->id.bustype = BUS_USB;
	// input_dev->id.bustype = BUS_GAMEPORT;
	input_dev->id.bustype = BUS_VIRTUAL;
	input_dev->id.vendor = 0x4147;
	input_dev->id.product = 0x504a;
	input_dev->id.version = 0x0100;

	input_set_drvdata(input_dev, pjjoy);

	input_dev->open = pjjoy_open;
	input_dev->close = pjjoy_close;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_set_abs_params(input_dev, ABS_X, 0, 255, 4, 8);
	input_set_abs_params(input_dev, ABS_Y, 0, 255, 4, 8);

	for (i = 0; i < JAMMA_NUM_BUTTONS; i++){
		set_bit(pjjoy_keys[i], input_dev->keybit);
	}

	error = input_register_device(pjjoy->dev);
	if (error) {
	        printk(KERN_ERR "%s: Unable to register device\n", pjjoy->phys);
		input_free_device(pjjoy->dev);
		return error;
	}

	#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	setup_timer(&(pjjoy->timer), pjjoy_poll, index);
	#else
	timer_setup(&(pjjoy->timer), pjjoy_poll, 0);	
	#endif
	return 0;
}

/*
 * pjjoy_init_devices() sets up device-specific values and registers the input devices.
 */

static int __init pjjoy_init(void)
{
	int i;
	int err;

	// mutex_init(&pjjoy_mutex);
	printk(KERN_INFO "Loading driver for %s, %d devices\n", DRIVER_DESC, JAMMA_NUM_DEVS);

	for (i = 0; i < JAMMA_NUM_DEVS; i++){
		err = pjjoy_init_device(&js[i], i);
		if (err){
			input_unregister_device(js[i].dev);
			return err;
		}
	}

	return 0;
}


static void __exit pjjoy_exit(void){
	int i;

	for (i = 0; i < JAMMA_NUM_DEVS; i++){
		input_unregister_device(js[i].dev);
		printk(KERN_INFO "%s: Unregistering device %d\n", js[i].phys, i);
	}
}

module_init(pjjoy_init);
module_exit(pjjoy_exit);
