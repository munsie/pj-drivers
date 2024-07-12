EXECPATH='/home/pi/utilities/getj4bmsg'

JOY1=`sudo $EXECPATH/getj4bmsg -d /dev/j4binput0`
JOY2=`sudo $EXECPATH/getj4bmsg -d /dev/j4binput1`
echo "Joystick Values:"
echo "P1 = $JOY1"
echo "P2 = $JOY2"
