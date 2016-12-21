#ifndef _VGA_IOCTL_H
#define _VGA_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

//These should be unique for each of your Drivers
#define VGA_IOCTL_BASE 'T'

struct vga_ioctl_data {
	__u32 offset;
	__u32 data;
};

// READ/WRITE ops
#define VGA_READ_REG  _IOR(VGA_IOCTL_BASE, 0, struct vga_ioctl_data)
#define VGA_WRITE_REG _IOW(VGA_IOCTL_BASE, 1, struct vga_ioctl_data)


#endif

