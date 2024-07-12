obj-m+=j4b.o pjjoy.o

.PHONY: all
all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

.PHONY: clean
clean:
	rm -f *~
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
