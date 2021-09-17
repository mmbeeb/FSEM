/* File Server Emulator   */
/* main.c                 */
/* (c) 2021 Martin Mather */

/* 17/09/21, some changes made to make it run in Cygwin64 */

#define _POSIX_C_SOURCE 1

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>	//sleep()
#include <time.h>
#include <sys/socket.h> //for Cygwin

#include "aun.h"
#include "fsem.h"

//#define LOOPSPERSEC	3000
//#define OPSPERLOOP	1000

#define OPSPERSEC	2000000

void set_no_buffer() {
	struct termios term;
	tcgetattr(0, &term);
	term.c_lflag &= ~ICANON;
	tcsetattr(0, TCSANOW, &term);
}

int charsWaiting(int fd) {
	int count;
	
	if (ioctl(fd, FIONREAD, &count) == -1)
		exit (EXIT_FAILURE);
	
	return count;
}

int main(void) {
	char c;
	int ex = 0, loops = 0;
	//double t2 = (double) 1/1000;
	clock_t timeout1 = time(0); //, timeout2 = clock();
	
	//21/09/21 new timing using clock_gettime
	double lps = 1000;
	int ops = OPSPERSEC/lps;
	long ntime = 0, otime = 0;
	struct timespec ts;

	printf("File Server Emulator\n\n");

	if (fsem_open("FS3v125", 0x0400, 254, "scsi1.dat")) {
		aun_open(254);
	
		set_no_buffer();
		do {
			if (time(0) > timeout1 && loops) {//second timer
				//printf("loop timeout %d %f\n", loops, t2);
				//t2 = t2 *((double) loops/LOOPSPERSEC);
				
				lps = (lps + loops)/2;//Average
				ops = OPSPERSEC/lps;
				//printf(":: loops=%d lps=%f ops=%d\n", loops, lps, ops);
				
				loops = 0;
				timeout1 = time(0);
			}

			//if (clock() > timeout2) {
			clock_gettime(CLOCK_MONOTONIC, &ts);
			ntime = ts.tv_nsec/1000000;//milliseconds
			if (ntime != otime) {
				otime = ntime;
				
				loops++;
				aun_poll();

				//if (!ex)
				//	ex = fsem_exec(OPSPERLOOP, 0);
				
				if (!ex)
					ex = fsem_exec(ops, 0);
				
				c = 0;
				int count = charsWaiting(fileno(stdin));
				if (count != 0) {
					c = tolower(getchar());
					switch (c) {
						case 'q':
							printf("Quit\n");
							break;
						case 'x':
							printf("Enable exec\n");
							ex = 0;
							break;
						case 'z':
							printf("Disable exec\n");
							ex = 1;
							break;
						case 'm'://monitor on/off
							fsem_sendkey('M');
							break;
						case 'r'://restart
							fsem_sendkey('Q');
							break;
					}
				}

				//timeout2 = clock() + t2 * CLOCKS_PER_SEC;
			}
		} while (c != 'q');
	
		aun_close();
		fsem_close();
	}
}
