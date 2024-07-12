# Makefile for building jstest-sdl

CFLAGS  = -std=c99
INCPATH	= -I/usr/local/include/SDL2 -I/usr/include/SDL2 -D_REENTRANT
LIBS	= -lSDL2

all:
	gcc sdljoytest.c -o sdljoytest $(CFLAGS) $(INCPATH) $(LIBS)
clean:
	-rm -f sdljoytest 

