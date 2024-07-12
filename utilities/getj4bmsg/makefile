MODSRC=/home/pi/linux-modules
INCPATH=-I$(MODSRC)/j4b -I$(MODSRC)/pjjoy

all:getj4bmsg

clean:
	rm -f getj4bmsg

getj4bmsg: getj4bmsg.c
	gcc $(INCPATH) -o getj4bmsg getj4bmsg.c
