/*  timer_driver.c - The simplest kernel module.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include <linux/io.h>
#include <asm/uaccess.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include <linux/cdev.h>
#include <linux/kfifo.h>

#include <linux/interrupt.h>
#include <asm/irq.h>

#include <linux/signal.h> //for SIGIO and POLL_IN

#include "timer_ioctl.h"

/* Standard module information, edit as appropriate */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason Liu & Michael Chyziak");
MODULE_DESCRIPTION("Zedboard AXI Timer Driver");

#define DRIVER_NAME "timer_driver"

static struct file_operations timer_fops;
struct timer_driver_local *timer;
static struct fasync_struct *timer_async_queue = NULL;

/*
 * Main driver data structure
 */
struct timer_driver_local {
	int irq;
	int major;
	int minor;
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;
	struct cdev *cdev;
};

// INTERRUPT HANDLER:
// this clears the interrupt status bit in the control register, and then
// sends the signal to the user program
static irqreturn_t timer_driver_irq(int irq, void *timer_driver)
{
	__u32 read_data;
	__u32 target_addr;
	//printk("timer interrupt\n");
	
	target_addr = (__u32 __force )timer->base_addr;
	target_addr += CONTROL_REG;
	
	// read value from CONTROL_REG
	read_data = ioread32(target_addr);
	
	// this gets the value of T0INT in the control stat reg : read_data & T0INT;

	// add in the T0INT
	read_data |= T0INT;
	// write it to CONTROL_REG
	iowrite32(read_data, target_addr);
	
	
	kill_fasync(&timer_async_queue, SIGIO, POLL_IN);
	return IRQ_HANDLED;
}


/* 
 * Open function, called when userspace program calls open()
 */
static int timer_open(struct inode *inode, struct file *file)
{
	return 0;
}


static int timer_fasync(int fd, struct file *file, int mode)
{
    printk("timer_fasync() called\n");
    return fasync_helper(fd, file, mode, &timer_async_queue);
}


/*
 * Close function, called when userspace program calls close()
 */
static int timer_release(struct inode *inode, struct file *file)
{
	timer_fasync(-1, file, 0);
	return 0;
}

/*
 * ioctl function, called when userspace program calls ioctl()
 * arg is a "struct timer_ioctl_data" however because of compiler warnings,
 * it had to be declared as an unsigned long
 */
static long timer_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {

	// this is always necessary
	struct timer_ioctl_data timer_data;
	__u32 target_addr;
	
	// these are for TIMER_READ_REG
	__u32 read_data;
	struct timer_ioctl_data * user_data;
	
	/* printf (along with other C library functions) is not available in
	 * kernel code, but printk is almost identical */
	printk(KERN_ALERT "Starting IOCTL... \n");
	
	if (copy_from_user(&timer_data, (void *)arg, sizeof(struct timer_ioctl_data))) {
		printk(KERN_ALERT "***Unsuccessful transfer of ioctl argument...\n");
		return -EFAULT;
	}
	
	// protecting read-only register (TCR0)
	// we don't have a define for TCR1 (the second timer)
	if ( cmd == TIMER_WRITE_REG && timer_data.offset == TIMER_REG ) {
		printk(KERN_ALERT "***Attempting to write to read-only register TIMER_REG...\n");
		return -EFAULT;
	}
	
	target_addr = (__u32 __force )timer->base_addr;
	target_addr += timer_data.offset;
	
	//if ( target_addr < timer->mem_start || target_addr >= timer->mem_end ) {
	//	printk(KERN_ALERT "***Invalid target address of ioctl argument...\n");
	//	return -EFAULT;
	//}

	switch (cmd) {
		case TIMER_READ_REG:
			
			user_data = (struct timer_ioctl_data *)arg;
			read_data = ioread32(target_addr);

			if (copy_to_user((void *)&user_data->data, (void *)&read_data, sizeof(__u32))) {
				printk(KERN_ALERT "***Unsuccessful transfer of ioctl argument...\n");
				return -EFAULT;
			}
			break;
		
		case TIMER_WRITE_REG:		
			iowrite32(timer_data.data, target_addr);
			break;	
		
		default:
			printk(KERN_ALERT "***Invalid ioctl command...\n");
			return -EINVAL;
	}

	return 0;

}

/*
 * File operations struct 
 * - informs kernel which functions should be called when userspace prorgams
 *   call different functions (ie. open, close, ioctl etc.)
 */
static struct file_operations timer_fops = {
	.open           = timer_open,
	.release        = timer_release,
	.unlocked_ioctl = timer_ioctl,
	.fasync 	= timer_fasync,
};

/*
 * Probe function (part of platform driver API)
 */
static int __devinit timer_driver_probe(struct platform_device *pdev)
{
	struct resource *r_irq; /* Interrupt resources */
	struct resource *r_mem; /* IO mem resources */
	struct device *dev = &pdev->dev;

	int rc = 0;
	dev_t devno;
	
	/* dev_info is a logging function part of the platform driver API */
	dev_info(dev, "Device Tree Probing\n");
	dev_info(dev, "Written by Michael Chyziak and Jason Liu\n");
	dev_info(dev, "Module name is %s\n", DRIVER_NAME );

	/* Get iospace for the device */
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		dev_err(dev, "invalid address\n");
		return -ENODEV;
	}
	
	/* Allocate space for driver data structure
	 * note the use of kmalloc - malloc (and all other C library functions) is
	 * unavailable in kernel code */
	timer = (struct timer_driver_local *) kmalloc(sizeof(struct timer_driver_local), GFP_KERNEL);
	if (!timer) {
		dev_err(dev, "Cound not allocate timer device\n");
		return -ENOMEM;
	}
	
	dev_set_drvdata(dev, timer);
	
	timer->mem_start = r_mem->start;
	timer->mem_end = r_mem->end;

	if (!request_mem_region(timer->mem_start,
				timer->mem_end - timer->mem_start + 1,
				DRIVER_NAME)) {
		dev_err(dev, "Couldn't lock memory region at %p\n",
			(void *)timer->mem_start);
		rc = -EBUSY;
		goto error1;
	}

	/* Allocate I/O memory */
	timer->base_addr = ioremap(timer->mem_start, timer->mem_end - timer->mem_start + 1);
	if (!timer->base_addr) {
		dev_err(dev, "timer: Could not allocate iomem\n");
		rc = -EIO;
		goto error2;
	}

	/* Get IRQ for the device */
	r_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!r_irq) {
		dev_info(dev, "no IRQ found\n");
		dev_info(dev, "timer at 0x%08x mapped to 0x%08x\n",
			(unsigned int __force)timer->mem_start,
			(unsigned int __force)timer->base_addr);
		return 0;
	} 
	timer->irq = r_irq->start;
	
	rc = request_irq(timer->irq, &timer_driver_irq, 0, DRIVER_NAME, timer);
	if (rc) {
		dev_err(dev, "testmodule: Could not allocate interrupt %d.\n",
			timer->irq);
		goto error3;
	}

	dev_info(dev, "Registering character device\n");
 
    	if ((alloc_chrdev_region(&devno, TIMER_MINOR, 1, DRIVER_NAME)) < 0)  {
        	goto error3;
	}

	// Fill in driver data structure
    	timer->major = MAJOR(devno);
    	timer->minor = MINOR(devno);
    		    
	dev_info(dev, "Initializing character device\n");
    		     	
    	timer->cdev = cdev_alloc();
	timer->cdev->owner = THIS_MODULE;
	timer->cdev->ops =  &timer_fops;
	cdev_add (timer->cdev, devno, 1);
	
	/* Print driver info (addresses, major and minor num) */
	dev_info(dev,"timer at 0x%08x mapped to 0x%08x, irq=%d\n",
		(unsigned int __force)timer->mem_start,
		(unsigned int __force)timer->base_addr,
		timer->irq);
		
	dev_info(dev,"timer at 0x%08x mapped to 0x%08x\nMajor: %d, Minor %d\n",
		(unsigned int __force)timer->mem_start,
		(unsigned int __force)timer->base_addr,
		timer->major, timer->minor);
	return 0;

/* Error handling for probe function
 * - this is one of very few cases where goto statements are a good idea
 * - when an error happens that prevents the driver from continuing to
 *   register/allocate resources, we need to undo any previous allocations
 *   that succeeded (in the reverse order)
 */
error3:
	free_irq(timer->irq, timer);
error2:
	release_mem_region(timer->mem_start, timer->mem_end - timer->mem_start + 1);
error1:
	kfree(timer);
	dev_set_drvdata(dev, NULL);
	return rc;
}

/*
 * Remove function (part of platform driver API)
 */
static int __devexit timer_driver_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct timer_driver_local *timer = dev_get_drvdata(dev);

	free_irq(timer->irq, timer);
	release_mem_region(timer->mem_start, timer->mem_end - timer->mem_start + 1);

	cdev_del(timer->cdev);
    	unregister_chrdev_region(MKDEV(timer->major, timer->minor), 1);

	kfree(timer);
	dev_set_drvdata(dev, NULL);
	return 0;
}


/*
 * Compatiblity string for matching driver to hardware
 */
#ifdef CONFIG_OF
static struct of_device_id timer_driver_of_match[] __devinitdata = {
	{ .compatible = "ensc351-timer", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, timer_driver_of_match);
#else
# define timer_driver_of_match
#endif


/*
 * Platform driver struct
 * - used by platform driver API for device tree probing
 */
static struct platform_driver timer_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= timer_driver_of_match,
	},
	.probe		= timer_driver_probe,
	.remove		= __devexit_p(timer_driver_remove),
};

/*
 * Driver initialization function
 */
static int __init timer_driver_init(void)
{
	printk("<1>Hello module world.\n");
	return platform_driver_register(&timer_driver);
}

/*
 * Driver exit function
 */
static void __exit timer_driver_exit(void)
{
	platform_driver_unregister(&timer_driver);
	printk(KERN_ALERT "Goodbye module world.\n");
}

/*
 * Register init and exit functions
 */
module_init(timer_driver_init);
module_exit(timer_driver_exit);

