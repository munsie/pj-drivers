#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include "j4b.h"

//dev/j4bv12
//dev/j4bv33pi
//dev/j4bv5
//dev/j4bv5pi

FILE *fp;
char dev[256];
int verbose;

int opendev(char *devname){
	memset(dev, 0, 256);
        strcpy(dev, "/dev/j4b");
        strncat(dev, devname, 240);

	fp = fopen(dev, "rb");
	if (fp == NULL){
		printf("Unable to open device %s, %s\n", dev, strerror(errno));
		exit(1);
	}
}

int closedev(){
	fclose(fp);
}

int getval(char *devname){
	int num = 0;
	struct j4b_msg msg;
	float volts;

	memset(&msg, 0, sizeof(msg));
	while (num == 0){
		num = fread(&msg, sizeof(msg), 1, fp);
	}
        
	volts = msg.val;
	volts = volts / 10;
	if (verbose) printf("%s = %1.1fV\n", devname, volts); 
	else printf("%1.1f\n", volts); 

	if (errno){
		printf("Unable to read device %s, %s\n", dev, strerror(errno));
		exit(1);
	}
}

int readval(char *devname){
	opendev(devname);
	getval(devname);
	closedev();
}


int main(int argc, char *argv[]){
	int ch;
	int val;
	int num;
	int all = 0; 
        char *voltage;


	if (argc < 2) {
		printf("getpjvolts <voltage | all>\n");
		printf("   where <voltage> device is any of the following: \n");
		printf("   v5      : JAMMA +5V from the PSU\n");
		printf("   v12     : JAMMA 12V from the PSU\n");
		printf("   v5pi    : +5V supply at the Raspberry Pi\n");
		printf("   v33pi   : +3.3V supply at the Raspberry Pi\n");
		exit(1);
	}

        
	voltage = argv[argc - 1];
	verbose = 0;

	if (argv[1][0] == '-'){
		if (argv[1][1] == 'v') verbose = 1;
	}

        if (strcmp(voltage, "all") == 0){
		all = 1;
		verbose = 1;
	} 
        if (strcmp(voltage, "v5") == 0 || all){
		readval("v5");
	}		

        if (strcmp(voltage, "v12") == 0 || all){
		readval("v12");
	}		

        if (strcmp(voltage, "v5pi") == 0 || all){
		readval("v5pi");
	}		

        if (strcmp(voltage, "v33pi") == 0 || all){
		readval("v33pi");
	}		

}
