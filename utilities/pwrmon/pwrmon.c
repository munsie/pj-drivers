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

// shutdown command to execute
const static char* shutdown_command = "/sbin/shutdown now";


// reading power device driver files /dev/j4bv5 and /dev/j4bv5pi
const static char* j4bv5 = "/dev/j4bv5";
const static char* j4bv5pi = "/dev/j4bv5pi";

// global constants for finetuning, defaults ok
const int sleepusecs = 500000;
const int downusecs = 50000;
const int downticks = 20;
const float difftrig = .3;

FILE *fpv5pi;
FILE *fpv5;
char dev[256];

void *opendevs(){
	fpv5 = fopen(j4bv5, "rb");
	if (fpv5 == NULL){
		printf("Unable to open device %s, %s\n", j4bv5, strerror(errno));
		exit(1);
	}
	fpv5pi = fopen(j4bv5pi, "rb");
	if (fpv5pi == NULL){
		printf("Unable to open device %s, %s\n", j4bv5pi, strerror(errno));
		exit(1);
	}
}

int closedevs(){
	fclose(fpv5);
	fclose(fpv5pi);
}

float getval(FILE *fp){
	int num = 0;
	struct j4b_msg msg;
	float volts;

	memset(&msg, 0, sizeof(msg));
	while (num == 0){
		num = fread(&msg, sizeof(msg), 1, fp);
	}
        
	volts = msg.val;
	volts = volts / 10;

	if (errno){
		printf("Unable to read device %s\n", strerror(errno));
		exit(1);
	}
	return(volts);
}

int main(int argc, char *argv[]){
	int ch;
	int val;
	float trigval, diffval, valv5, valv5pi;
	int num;
	int all = 0; 
        char *voltage;
	int verbose = 0;
	int shutdown = 0;
	int downcount;

	if (argc < 2) {
		printf("pwrmon [option] <voltage>\n");
		printf("   where <voltage> device is difference between v5 and v5pi voltage required to trigger a shutdown, and\n");
		printf("   option -d is for verbose debug mode only\n");
		printf("   option -v is for verbose mode with shutdown\n");
		printf("   option -q is for quiet shutdown monitoring\n");
		exit(1);
	}

        
	voltage = argv[argc - 1];

	if (argv[1][0] == '-'){
		if (argv[1][1] == 'd'){
			verbose = 1;
		}
		else if (argv[1][1] == 'v'){
			verbose = 1;
			shutdown = 1;
		}
		else if (argv[1][1] == 'q'){
			verbose = 0;
			shutdown = 1;
		}
	}

        if (sscanf(voltage, "%f", &trigval) == 1){
		printf("Triggering reboot when input voltage drop exceeds %2.1fV\n", trigval); 
	}
	else{
		trigval = difftrig;
		printf("Triggering reboot when input voltage drop exceeds default value of %2.1fV\n", trigval); 
	}	
	opendevs(); 
	while(1){
		valv5 = getval(fpv5);
		valv5pi = getval(fpv5pi);
		diffval = valv5pi - valv5;
		if (verbose) printf("v5 = %2.1f, v5pi = %2.1f, difference = %2.1f\n", valv5, valv5pi, diffval);
		if (diffval >= trigval){
			if (verbose){
				printf("Power shutdown triger detected, v5 = %2.1f, v5pi = %2.1f, difference = %2.1f, trigger = %2.1f\n", 
					valv5, valv5pi, diffval, trigval);
			}
			else{
				printf("Power shutdown triger detected\n"); 
			}

			downcount = downticks;
			while(1){
				valv5 = getval(fpv5);
				valv5pi = getval(fpv5pi);
				diffval = valv5pi - valv5;
				if (diffval > trigval){
					if (verbose) printf("Brownout continues, difference = %2.1f, shutting down in %d...\n", diffval, downcount);
					if (downcount <= 0){
						if (shutdown){
							printf("Shutdown condition detected, shutting down now...\n");
							system(shutdown_command); 
						}
						else{
							printf("Shutdown condition detected, exiting...\n");
							exit(1);
						}
					}	
				}
				else{
					if (verbose) printf("Power restored, returning to normal monitoring\n");
					break;
				}
				downcount--;
				usleep(downusecs);
			}	
		}	
		usleep(sleepusecs);
	}
}
