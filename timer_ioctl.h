#ifndef _TIMER_IOCTL_H
#define _TIMER_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

// Note: These must be unique for each driver you create
#define TIMER_IOCTL_BASE 'T'
#define TIMER_MINOR 69

struct timer_ioctl_data {
	__u32 offset;
	__u32 data;
};

// Timer IRQ
#define TIMER_INTERRUPT_NUM 0

#define NUM_SLAVE_REGS 10

// Register offset
#define CONTROL_REG		0x00
#define LOAD_REG		0x04
#define TIMER_REG		0x08

// Register masks
#define T0INT	(1 << 8)	// Timer0 Interrupt status
#define ENT0	(1 << 7)	// Enable Timer0
#define ENIT0	(1 << 6)	// Enable Timer0 interrupt
#define LOAD0	(1 << 5)	// Load Timer0
#define ARHT0	(1 << 4)	// Auto Reload/Hold Timer0
#define UDT0	(1 << 1)	// Timer0 functions as down counter

// READ/WRITE ops
#define TIMER_READ_REG  _IOR(TIMER_IOCTL_BASE, 0, struct timer_ioctl_data)
#define TIMER_WRITE_REG _IOW(TIMER_IOCTL_BASE, 1, struct timer_ioctl_data)

#endif

