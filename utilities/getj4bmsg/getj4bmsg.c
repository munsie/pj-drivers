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
#include "../j4b/j4b.h"

int main(int argc, char *argv[]){
	int ch;
	int val;
	int num;
	int fmt = 0; 
	FILE *fp;

	struct j4b_msg msg;

	if (argc < 2) {
		printf("getj4bmsg [format] <queue_device> where queue_device is any of /dev/j4b*\n");
		printf("          where format -d for decimal value, or -x for hex value\n");
		exit(1);
	}
 
	fp = fopen(argv[argc - 1], "rb");
	if (fp == NULL){
		printf("Unable to open device %s, %s\n", argv[argc - 1], strerror(errno));
		exit(1);
	}

	if (argv[1][0] == '-'){
		if (argv[1][1] == 'd') fmt = 1;
		if (argv[1][1] == 'x') fmt = 0;
	}

	memset(&msg, 0, sizeof(msg));
		
	while (num == 0){
		num = fread(&msg, sizeof(msg), 1, fp);
	}
	if (fmt == 0) printf("%x %x\n", msg.cmd, msg.val); 
	else if (fmt == 1) printf("%d %d\n", msg.cmd, msg.val); 

	if (errno){
		printf("Unable to read device %s, %s\n", argv[1], strerror(errno));
		exit(1);
	}
	exit(0);
}
