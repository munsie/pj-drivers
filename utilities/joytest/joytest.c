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
#include "pjjoy.h"
#include "j4b.h"

/*
struct j4b_msg {
        unsigned char device:4;
        unsigned char msg:4;
        unsigned short val:16;
};
*/


int main(int argc, char *argv[]){
	int ch;
	int val;
	int wrote;

	int serr, maxsfd;
        struct timeval timeout;
        fd_set readfds, writefds;

	FILE *fp;

	struct j4b_msg msg;
	char *cmsg;

	if (argc < 2) {
		printf("joytest <queue_device> where queue_device is /dev/j4binput0 or /dev/j4binput1\n");
		exit(1);
	}
 
	printf("Using queue: %s\n", argv[1]);
	printf("Press ~ to quit\n");
	printf("Use W,S,A,D for UP,DOWN,LEFT,RIGHT\n");
	printf("Use I,O,P,J,K,L for BUTTON 1,2,3,4,5,6\n");
	printf("Use Z,X,C,V for SELECT,START,EXTRA,MODE\n");

	fp = fopen(argv[1], "w");
	if (fp == NULL){
		printf("Unable to open device %s, %s\n", argv[1], strerror(errno));
		exit(1);
	}

	system("stty -echo"); // supress echo
	system("stty cbreak"); // go to RAW mode

	while(1){

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_SET(STDIN_FILENO, &readfds);
		maxsfd=0;

		timeout.tv_sec = 0;
		timeout.tv_usec = 300000;

		serr = select(maxsfd+1, &readfds, &writefds, NULL, &timeout);

		if (serr > 0 && FD_ISSET(0, &readfds)){
			ch = getchar(); 
		}
                else{
			ch = ' ';
			val = 0;
		}

		if (ch == '~') break;
		else if (ch == 'w') val |= JAMMA_UP;
		else if (ch == 's') val |= JAMMA_DOWN;
		else if (ch == 'a') val |= JAMMA_LEFT;
		else if (ch == 'd') val |= JAMMA_RIGHT;
		else if (ch == 'i') val |= JAMMA_B1;
		else if (ch == 'o') val |= JAMMA_B2;
		else if (ch == 'p') val |= JAMMA_B3;
		else if (ch == 'j') val |= JAMMA_B4;
		else if (ch == 'k') val |= JAMMA_B5;
		else if (ch == 'l') val |= JAMMA_B6;
		else if (ch == 'z') val |= JAMMA_SELECT;
		else if (ch == 'x') val |= JAMMA_START;
		else if (ch == 'c') val |= JAMMA_EXTRA;
		else if (ch == 'v') val |= JAMMA_MODE;

		memset(&msg, 0, sizeof(struct j4b_msg));
		msg.cmd = J4B_CMD_INPUT_0;
		msg.val = val;

		cmsg = (char *)&msg;
		
		wrote = fwrite(&msg, sizeof(struct j4b_msg), 1, fp);
		printf("ch = %04d '%1c', val = %04x, msg = %x.%x.%x.%x, wrote = %d       \r", ch, ch, val, 
			cmsg[0], cmsg[1], cmsg[2], cmsg[3], wrote); 
		fflush(stdout);

		if (errno || !wrote){
			printf("Unable to write to device %s, %s\n", argv[1], strerror(errno));
			exit(1);
		}
		fflush(fp);
	}
	system ("stty echo"); // Make echo work
	system("stty -cbreak");// go to COOKED mode
}
