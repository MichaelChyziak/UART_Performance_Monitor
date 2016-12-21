#ifndef UART_INTERFACE_H
#define UART_INTERFACE_H


#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include "../../user-modules/timer_driver/timer_ioctl.h"

#define MAX_CHAR_INPUT_SIZE 1
#define BILLION 1000000000ULL //exactly 1 billion

//file descriptors
extern int serial_fd;
extern int timer_fd;

extern struct timespec typing_start;

extern struct timer_ioctl_data timer_data; //data structure for ioctl calls


//returns the difference of timespec A-B in nanoseconds
unsigned long long time_spec_diff(struct timespec *timeA_p, struct timespec *timeB_p);


//Initializes the serial port in non-cannonical mode and sets serial fd
int init_serial();


//Blocking read from the serial port
//After any characters are read, update any possible metrics
int read_from_kermit(char *c);

//writes back to the serial_fd count number of characters from c
int write_to_kermit(char *c, int count);
//Function to read the value of the timer
__u32 read_timer();

/*
 * SIGINT Signal Handler
 */
void sigint_handler(int signum);

//Sets the timer to send SIGIO every "msec" milliseconds
int setup_timer_interval(int msec);

//This function never returns (exits)
//Closes all fd's and sets timer to 0
void exit_interface(int status);

#endif//UART_INTERFACE_H
