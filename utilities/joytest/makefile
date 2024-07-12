MODSRC=/home/pi/linux-modules
INCPATH=-I$(MODSRC)/j4b -I$(MODSRC)/pjjoy

all: joytest

#non curses version
clean:
	rm -f joytest
joytest: joytest.c
	gcc $(INCPATH) -o joytest joytest.c
