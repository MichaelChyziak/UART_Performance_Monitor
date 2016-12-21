/*  vga_driver.c
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

#include "vga_ioctl.h"

#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>

/* Standard module information, edit as appropriate */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason Liu, Michael Chyziak, Reese Erickson, Tahsin Alam, & in memory of David");
MODULE_DESCRIPTION("Zedboard AXI VGA Driver");

#define DRIVER_NAME "vga_driver"
#define FB_SIZE (640*480*4)

static struct file_operations vga_fops;
struct vga_driver_local *vga;

/*
 * Main driver data structure
 */
struct vga_driver_local {	//define main driver data
	int major;
	int minor;
	
	unsigned long mem_start;
	unsigned long mem_end;
	void __iomem *base_addr;

	void *fb_virt;		//kernel space memeory pointer
	dma_addr_t fb_phys;	//physical adress
	struct device *dev;	//Representation of vga device
	struct cdev *cdev;
};


/*
 * Open function, called when userspace program calls open()
 */
static int vga_open(struct inode *inode, struct file *file)
{
    return 0;
}


/*
 * Close function, called when userspace program calls close()
 */
static int vga_release(struct inode *inode, struct file *file)
{
    return 0;
}

/*
 * ioctl function, called when userspace program calls ioctl()
 * arg is a "struct vga_ioctl_data" however because of compiler warnings,
 * it had to be declared as an unsigned long
 */
static long vga_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {

    // this is always necessary
    struct vga_ioctl_data vga_data;
    __u32 target_addr;

    // these are for VGA_READ_REG
    __u32 read_data;
    struct vga_ioctl_data * user_data;

    /* printf (along with other C library functions) is not available in
     * kernel code, but printk is almost identical */
    printk(KERN_ALERT "Starting IOCTL... \n");

    if (copy_from_user(&vga_data, (void *)arg, sizeof(struct vga_ioctl_data))) {
        printk(KERN_ALERT "***Unsuccessful transfer of ioctl argument...\n");
        return -EFAULT;
    }

    target_addr = (__u32 __force )vga->base_addr;
    target_addr += vga_data.offset;

    
    switch (cmd) {
        case VGA_READ_REG:

            user_data = (struct vga_ioctl_data *)arg;
            read_data = ioread32(target_addr);

            if (copy_to_user((void *)&user_data->data, (void *)&read_data, sizeof(__u32))) {
                printk(KERN_ALERT "***Unsuccessful transfer of ioctl argument...\n");
                return -EFAULT;
            }
            break;

        case VGA_WRITE_REG:
            iowrite32(vga_data.data, target_addr);
            break;

        default:
            printk(KERN_ALERT "***Invalid ioctl command...\n");
            return -EINVAL;
    }

    return 0;

}

/*
 * Mmap function, called when userspace program calls Mmap()
 */
int vga_mmap(struct file *file, struct vm_area_struct *vma) {
    unsigned long pfn, off;
    pfn = dma_to_pfn(vga->dev, vga->fb_phys);
    off = vma->vm_pgoff;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    if (remap_pfn_range(vma, vma->vm_start, pfn + off, vma->vm_end - vma->vm_start,
        vma->vm_page_prot)) {
        return -EAGAIN;
    }
    return 0;
}


/*
 * File operations struct
 * - informs kernel which functions should be called when userspace prorgams
 *   call different functions (ie. open, close, ioctl etc.)
 */
static struct file_operations vga_fops = {
    .open           = vga_open,
    .release        = vga_release,
    .unlocked_ioctl = vga_ioctl,
    .mmap = vga_mmap,
};

/*
 * Probe function (part of platform driver API)
 */
static int __devinit vga_driver_probe(struct platform_device *pdev)
{
    struct resource *r_mem; /* IO mem resources */
    struct device *dev = &pdev->dev;
    int rc = 0;
    dev_t devno;

    /* dev_info is a logging function part of the platform driver API */
    dev_info(dev, "Device Tree Probing\n");
    dev_info(dev, "Written by Michael Chyziak, Jason Liu, Reese Erickson, Tahsin Alam\n");
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
    vga = (struct vga_driver_local *) kmalloc(sizeof(struct vga_driver_local), GFP_KERNEL);
    if (!vga) {
        dev_err(dev, "Cound not allocate vga device\n");
        return -ENOMEM;
    }

    dev_set_drvdata(dev, vga);

    /* Allocate space for driver frame buffer physical address
     * note the use of kmalloc - malloc (and all other C library functions) is
     * unavailable in kernel code */
    vga->fb_phys = (dma_addr_t) kmalloc(FB_SIZE, GFP_KERNEL);
    if (!vga->fb_phys) {
        dev_err(dev, "Cound not allocate vga fb_phys\n");
        rc = -ENOMEM;
        goto error1;
    }

    vga->mem_start = r_mem->start;
    vga->mem_end = r_mem->end;
    vga->dev = dev;

    if (!request_mem_region(vga->mem_start, vga->mem_end - vga->mem_start + 1, DRIVER_NAME)) {
        dev_err(dev, "Couldn't lock memory region at %p\n", (void *)vga->mem_start);
        rc = -EBUSY;
        goto error2;
    }

    /* Allocate I/O memory */
    vga->base_addr = ioremap(vga->mem_start, vga->mem_end - vga->mem_start + 1);
    if (!vga->base_addr) {
        dev_err(dev, "vga: Could not allocate iomem\n");
        rc = -EIO;
        goto error3;
    }

    dev_info(dev, "Registering character device\n");

    if (!(vga->fb_virt = (void*) dma_alloc_coherent(dev, FB_SIZE, &vga->fb_phys, GFP_KERNEL))) {
        goto error3;
    }
    iowrite32(vga->fb_phys, vga->base_addr);

    if ((alloc_chrdev_region(&devno, 69, 1, DRIVER_NAME)) < 0)  {
        goto error4;
    }

    // Fill in driver data structure
    vga->major = MAJOR(devno);
    vga->minor = MINOR(devno);

    dev_info(dev, "Initializing character device\n");

    vga->cdev = cdev_alloc();
    vga->cdev->owner = THIS_MODULE;
    vga->cdev->ops =  &vga_fops;
    cdev_add (vga->cdev, devno, 1);

    /* Print driver info (addresses, major and minor num) */
    dev_info(dev,"vga at 0x%08x mapped to 0x%08x\nMajor: %d, Minor %d\n",
        (unsigned int __force)vga->mem_start,
        (unsigned int __force)vga->base_addr,
        vga->major, vga->minor);
    return 0;

/* Error handling for probe function
 * - this is one of very few cases where goto statements are a good idea
 * - when an error happens that prevents the driver from continuing to
 *   register/allocate resources, we need to undo any previous allocations
 *   that succeeded (in the reverse order)
 */
error4:
    dma_free_coherent(dev, FB_SIZE, vga->fb_virt, vga->fb_phys);
error3:
    release_mem_region(vga->mem_start, vga->mem_end - vga->mem_start + 1);
error2:
    kfree((void*) vga->fb_phys);
error1:
    kfree(vga);
    dev_set_drvdata(dev, NULL);
    return rc;
}

/*
 * Remove function (part of platform driver API)
 */
static int __devexit vga_driver_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct vga_driver_local *vga = dev_get_drvdata(dev);

    vga->dev = dev;

    dma_free_coherent(dev, FB_SIZE, vga->fb_virt, vga->fb_phys); 

    release_mem_region(vga->mem_start, vga->mem_end - vga->mem_start + 1);

    cdev_del(vga->cdev);
    unregister_chrdev_region(MKDEV(vga->major, vga->minor), 1);

    kfree(vga);
    dev_set_drvdata(dev, NULL);
    return 0;
}


/*
 * Compatiblity string for matching driver to hardware
 */
#ifdef CONFIG_OF
static struct of_device_id vga_driver_of_match[] __devinitdata = {
    { .compatible = "ensc351-vga", },
    { /* end of list */ },
};
MODULE_DEVICE_TABLE(of, vga_driver_of_match);
#else
#define vga_driver_of_match
#endif


/*
 * Platform driver struct
 * - used by platform driver API for device tree probing
 */
static struct platform_driver vga_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = vga_driver_of_match,
    },
    .probe = vga_driver_probe,
    .remove = __devexit_p(vga_driver_remove),
};

/*
 * Driver initialization function
 */
static int __init vga_driver_init(void)
{
    printk("<1>Hello module world.\n");
    return platform_driver_register(&vga_driver);
}

/*
 * Driver exit function
 */
static void __exit vga_driver_exit(void)
{
    platform_driver_unregister(&vga_driver);
    printk(KERN_ALERT "Goodbye module world.\n");
}

/*
 * Register init and exit functions
 */
module_init(vga_driver_init);
module_exit(vga_driver_exit);

