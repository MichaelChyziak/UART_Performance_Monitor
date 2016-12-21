#include "UART_Interface.h"

//file descriptors
int serial_fd;
int timer_fd;

//timings of typing start
struct timer_ioctl_data timer_data; //data structure for ioctl calls

//returns the difference of timespec A-B in nanoseconds
unsigned long long time_spec_diff(struct timespec *timeA_p, struct timespec *timeB_p)
{
  return ((timeA_p->tv_sec * BILLION) + timeA_p->tv_nsec) - ((timeB_p->tv_sec * BILLION) + timeB_p->tv_nsec);
}


//Initializes the serial port in non-cannonical mode and sets serial fd
int init_serial() {
    struct termios tio;
    int serial_flags;

    memset(&tio, 0, sizeof(tio));
    serial_fd = open("/dev/ttyPS0",O_RDWR);

    if(serial_fd == -1) {
        printf("Failed to open serial port... :( \n");
        return -1;
    }
    tcgetattr(serial_fd, &tio);
    cfsetospeed(&tio, B115200);
    cfsetispeed(&tio, B115200);
    tio.c_lflag &= (~ICANON);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    tcsetattr(serial_fd, TCSANOW, &tio);
    // make sure we turn off nonblocking file operation, since we set VMIN and VTIME already
    serial_flags = fcntl( serial_fd, F_GETFD );
    fcntl(serial_fd, F_SETFD, (serial_flags & (~O_NONBLOCK)));
    tcflush( serial_fd, TCIFLUSH );

    return 0;
}


//Blocking read from the serial port
//After any characters are read, update any possible metrics
int read_from_kermit(char *c) {
    int num_chars_read = 0;

	num_chars_read = read(serial_fd, c, MAX_CHAR_INPUT_SIZE);

    //failure to read from kermit
    if (num_chars_read < 0) {
        if(errno == EAGAIN) {
            // If this condition passes, there is no data to be read
            return 0;
        }
	    perror("READ FROM KERMIT FAILED");
	    return -1;
	}
    return num_chars_read;
}

//writes back to the serial_fd count number of characters from c
int write_to_kermit(char *c, int count) {
    //write(serial_fd, c, count);
    return 0;
}

//Function to read the value of the timer
__u32 read_timer()
{
	timer_data.offset = TIMER_REG;
	ioctl(timer_fd, TIMER_READ_REG, &timer_data);
	return timer_data.data;
}

/*
 * SIGINT Signal Handler
 */
void sigint_handler(int signum)
{
	//printf("Received Signal, signum=%d (%s)\n", signum, strsignal(signum));

	if (timer_fd) {
		// Turn off timer and reset device
		timer_data.offset = CONTROL_REG;
		timer_data.data = 0x0;
		ioctl(timer_fd, TIMER_WRITE_REG, &timer_data);

		// Close device file
		close(timer_fd);
	}

	exit(EXIT_SUCCESS);
}

//Sets the timer to send SIGIO every "msec" milliseconds
int setup_timer_interval(int msec) {

    // Load countdown timer initial value for ~1 sec
	timer_data.offset = LOAD_REG;
	timer_data.data = 100e3 * msec; // timer runs on 100 MHz clock
	ioctl(timer_fd, TIMER_WRITE_REG, &timer_data);

	// Set control bits to load value in load register into counter
	timer_data.offset = CONTROL_REG;
	timer_data.data = LOAD0;
	ioctl(timer_fd, TIMER_WRITE_REG, &timer_data);

	// Set control bits to enable timer, enable interrupt, auto reload mode,
	//  and count down
	timer_data.data = ENT0 | ENIT0 | ARHT0 | UDT0;
	ioctl(timer_fd, TIMER_WRITE_REG, &timer_data);

    return 0;
}

//This function never returns (exits)
//Closes all fd's and sets timer to 0
void exit_interface(int status) {

    //close serial fd
    if (serial_fd) {
        close(serial_fd);
    }

    //close timer_fd
    if (timer_fd) {
		// Turn off timer and reset device
		timer_data.offset = CONTROL_REG;
		timer_data.data = 0x0;
		ioctl(timer_fd, TIMER_WRITE_REG, &timer_data);

		// Close device file
		close(timer_fd);
	}

	exit(status);
}
