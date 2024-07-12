MODSRC=/home/pi/linux-modules
INCPATH=-I$(MODSRC)/j4b -I$(MODSRC)/pjjoy

all: pwrmon

clean:
	rm -f pwrmon

pwrmon: pwrmon.c
	gcc $(INCPATH) -o pwrmon pwrmon.c
