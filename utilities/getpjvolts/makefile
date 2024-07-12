MODSRC=/home/pi/linux-modules
INCPATH=-I$(MODSRC)/j4b -I$(MODSRC)/pjjoy

all:getpjvolts

clean:
	rm -f getpjvolts

getpjvolts: getpjvolts.c
	gcc $(INCPATH) -o getpjvolts getpjvolts.c
