/**
 * timer_test.c - AXI Timer/Counter driver test program
 *
 * This program tests the basic functionality of the AXI Timer/Counter driver.
 * It consists of two parts: 
 *
 * Part 1 is a simple up counter test, which sets the timer/counter to count
 * up, sleeps for 1 second, then stops the counter and reads the counter value.
 *
 * Part 2 is a test of the Asynchronous Notification functionality in the
 * driver. It sets of asynchronous notification on the driver device file,
 * then registers a handerl for SIGIO. Then it loads a large value into
 * the timer, enables interrupts, turns on the auto releod function, and 
 * enables countdown mode. The program then waits for signals in an infinite
 * loop. 
 * Assuming the driver has implemented the asynchronous notification correctly,
 * this program should receive a SIGIO signal when an interrupt is generated
 * (ie. when the timer rolls over upon reaching 0) Because the auto reload
 * mode is enabled, the program should receive SIGIO at regular intervals.
 *
 * A signal handler for SIGINT is registered so that a user may exit the
 * test program by entering CTRL+C at the terminal. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "../../user-modules/timer_driver/timer_ioctl.h"

/*
 * Global variables
 */
struct timer_ioctl_data data; // data structure for ioctl calls
int fd; // file descriptor for timer driver

/* 
 * Function to read the value of the timer
 */
__u32 read_timer()
{
	data.offset = TIMER_REG;
	ioctl(fd, TIMER_READ_REG, &data);
	return data.data;
}

/*
 * SIGIO Signal Handler
 */
void sigio_handler(int signum) 
{
	printf("Received Signal, signum=%d (%s)\n", signum, strsignal(signum));
}

/*
 * SIGINT Signal Handler
 */
void sigint_handler(int signum)
{
	printf("Received Signal, signum=%d (%s)\n", signum, strsignal(signum));

	if (fd) {
		// Turn off timer and reset device
		data.offset = CONTROL_REG;
		data.data = 0x0;
		ioctl(fd, TIMER_WRITE_REG, &data);

		// Close device file
		close(fd);
	}

	exit(EXIT_SUCCESS);
}

int main(void)
{
	int oflags;

	// Register handler for SIGINT, sent when pressing CTRL+C at terminal
	signal(SIGINT, &sigint_handler);

	// Open device driver file
	if (!(fd = open("/dev/timer_driver", O_RDWR))) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	/* 
	 * Part 1 - Simple up counter test
	 */
	printf("\nPart 1 - Simple up counter test:\n");

	// Rest the counter
	data.offset = LOAD_REG;
	data.data = 0x0;
	ioctl(fd, TIMER_WRITE_REG, &data);

	// Set control bits to load value in load register into counter
	data.offset = CONTROL_REG;
	data.data = LOAD0;
	ioctl(fd, TIMER_WRITE_REG, &data);

	sleep(1);

	// Read value from timer
	printf("initial timer value = %u\n", read_timer());

	// Set control bits to enable timer, count up
	data.offset = CONTROL_REG;
	data.data = ENT0;
	ioctl(fd, TIMER_WRITE_REG, &data);

	sleep(2);

	// Clear control bits to disable timer
	data.offset = CONTROL_REG;
	data.data = 0x0;
	ioctl(fd, TIMER_WRITE_REG, &data);

	// Read value from timer
	printf("timer value after 2 seconds  = %u\n", read_timer());

	printf("End of Part 1\n");

	/* 
	 * Part 2 - Asynchronous Notification test
	 */
	printf("\nPart 2 - Asynchronous Notification test:\n");

	// Set up Asynchronous Notification
	signal(SIGIO, &sigio_handler);
	fcntl(fd, F_SETOWN, getpid());
	oflags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, oflags | FASYNC);

	// Load countdown timer initial value for ~1 sec
	data.offset = LOAD_REG;
	data.data = 100e6; // timer runs on 100 MHz clock
	ioctl(fd, TIMER_WRITE_REG, &data);
	sleep(1);

	// Set control bits to load value in load register into counter
	data.offset = CONTROL_REG;
	data.data = LOAD0;
	ioctl(fd, TIMER_WRITE_REG, &data);

	// Set control bits to enable timer, enable interrupt, auto reload mode,
	//  and count down
	data.data = ENT0 | ENIT0 | ARHT0 | UDT0;
	ioctl(fd, TIMER_WRITE_REG, &data);

	// Wait for signals
	while (1);

	return 0;
}

