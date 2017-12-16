#include <linux/init.h>    // magic
#include <linux/module.h>  // magic #2
#include <linux/kernel.h>  // magic #3
#include <linux/gfp.h>     // kzalloc flags
#include <linux/slab.h>    // kzalloc
#include <linux/uaccess.h> // copy_from/to_user
#include <linux/wait.h>    // wait_queue
#include <linux/fs.h>      // file_operations
#include <linux/kdev_t.h>  // mkdev
#include <linux/cdev.h>    // cdev_init
#include <linux/interrupt.h>
#include <linux/timex.h>

#define INT_NUM 12
#define DEVICES_COUNT 1
#define MICEY_MAJOR 210
#define MICEY_MINOR 0
MODULE_LICENSE("GPL");

static int is_eof;
static wait_queue_head_t queue;
static int opened_descriptors;
struct cdev micey_cdev;
static dev_t dev;
const struct file_operations micey_file_ops;
static int int_flag;
static unsigned int t;


//int request_irq(INT_NUM, micey_irq_handler, IRQF_SHARED, "micey", &micey_cdev)
//void free_irq(INT_NUM, &micey_cdev)

static irqreturn_t micey_irq_handler(int irq, void *dev) 
{
	//printk("Got an interrupt\n");
	int_flag = 1;
	t = (unsigned char)get_cycles();
	wake_up_interruptible(&queue);
	return IRQ_HANDLED;
}


static int micey_open(struct inode *i, struct file *f) 
{
	if (opened_descriptors == 0) {
		int_flag = 0;
		request_irq(INT_NUM, micey_irq_handler, IRQF_SHARED, "micey", &micey_cdev);
		is_eof = 0;
	}
	opened_descriptors++;
	return 0;
}


static int micey_release(struct inode *i, struct file *f)
{
	opened_descriptors--;
	if (opened_descriptors == 0) {
		is_eof = 1;
		printk(KERN_INFO "released\n");
		wake_up_interruptible(&queue);
		free_irq(INT_NUM, &micey_cdev);
	}
	return 0;
}


static ssize_t micey_read(struct file *f, char __user *buf, size_t sz, loff_t *off)
{
	// Writes into buf! (user space app READS us)
	sz = 16;
	while (true) {
		//printk("Waaaat I'm not sleepng\n");
		t &= 0x01;
		if (t)
			t = '1';
		else
			t = '0';
		if (copy_to_user(buf+sz-1, &t, 1)) {
			printk(KERN_ERR "Error! Cannot copy to user.\n");
			return -1;
		}
		sz--;
		if (sz == 0 || is_eof == 1) {
			return 16;
		}
		//printk("I before sleeping\n");
		wait_event_interruptible(queue, (int_flag == 1));
		int_flag = 0;
	}
}


static int __init micey_init(void)
{	
	dev = MKDEV(MICEY_MAJOR, MICEY_MINOR);
	if (register_chrdev_region(dev, DEVICES_COUNT, "micey")) {
		printk(KERN_CRIT "ERROR!! Failed to register micey device.\n");
		return -2;
	}
	cdev_init(&micey_cdev, &micey_file_ops);
	if (cdev_add(&micey_cdev, dev, DEVICES_COUNT)) {
		printk(KERN_CRIT "ERROR!! Failed to add micey device as char device.\n");
		return -3;
	}
	init_waitqueue_head(&queue);
	opened_descriptors = 0;
	//request_irq(INT_NUM, micey_irq_handler, IRQF_SHARED, "micey", &micey_cdev);
	return 0;
} 


static void __exit micey_exit(void)
{
	//free_irq(INT_NUM, &micey_cdev);
	dev = MKDEV(MICEY_MAJOR, MICEY_MINOR);
	unregister_chrdev_region(dev, DEVICES_COUNT);
	cdev_del(&micey_cdev);
}


const struct file_operations micey_file_ops = {
	.owner = THIS_MODULE,
	.open = micey_open,
	.release = micey_release,
	.read = micey_read
};

module_init(micey_init);
module_exit(micey_exit);
