EXECPATH='/home/pi/utilities/getj4bmsg'

VOLTS12=`sudo $EXECPATH/getj4bmsg -d /dev/j4bv12`
VOLTS33=`sudo $EXECPATH/getj4bmsg -d /dev/j4bv33pi`
VOLTS5J=`sudo $EXECPATH/getj4bmsg -d /dev/j4bv5`
VOLTS5P=`sudo $EXECPATH/getj4bmsg -d /dev/j4bv5pi`
echo "Voltages:"
echo "PI 5V     = $VOLTS5P"
echo "PJ 5V     = $VOLTS5J"
echo "PJ 3.3V   = $VOLTS33"
echo "PJ 12V    = $VOLTS12"
