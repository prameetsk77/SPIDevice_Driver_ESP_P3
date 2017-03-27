#include <linux/kthread.h>
#include <linux/module.h>    
#include <linux/kernel.h> 
#include <linux/fs.h>	   // Inode and File types
#include <linux/cdev.h>    // Character Device Types and functions.
#include <linux/types.h>
#include <asm/uaccess.h>   // Copy to/from user space   
#include <linux/init.h>  
#include <linux/slab.h>
#include <linux/device.h>  // Device Creation / Destruction functions
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/moduleparam.h> // Passing parameters to modules through insmod
#include <asm/msr.h>
#include <linux/interrupt.h>

#define gpio_1_Mux 30
#define gpio_0_Mux 31
#define trigger 14
#define echo 15
#define DEVICE_NAME "Ultrasonic_HCSR04"  // device name to be created and registered

struct timer_list my_timer;

/* per device structure */
struct udm_dev {
	struct cdev cdev;   /* The cdev structure */
    int distance; 
	
} *udm_devp;

static dev_t udm_dev_num;      /* Allotted device number */
struct class *udm_dev_class;          /* Tie with the device model */
static struct device *udm_dev_device;

/// timer handler
void UDM_Timer_Handler (unsigned long data){
	
	struct timer_list *timer = (struct timer_list*)data;
	
	gpio_set_value(trigger, 1); // trigger  = pin 14
	udelay(10);
	gpio_set_value(trigger, 0); 
	
	timer->expires = jiffies+(15); // repeat after 15 jiffies i.e. after 60ms
	add_timer(timer);
	
	return ;
}
/// IRQ Handler
unsigned long rise_time, fall_time, time_dif;
bool toggle =0;
static irq_handler_t UDM_IRQ_Handler(int irq, void *dev_id, struct pt_regs *regs)
{	
 	 if(toggle == 0){
	 	rdtscl(rise_time);
		
		toggle =1;
		irq_set_irq_type(irq,IRQF_TRIGGER_FALLING);
	}
	 else{
		rdtscl(fall_time);
		time_dif = fall_time - rise_time;
		
		udm_devp->distance = (time_dif / (58*400)); // converting to echo high time to cms
		
		toggle =0;
		irq_set_irq_type(irq,IRQF_TRIGGER_RISING);
	}
	 	
	return (irq_handler_t)IRQ_HANDLED;
}

/*
* Open usonic driver
*/

int UDM_open(struct inode *inode, struct file *file)
{
	struct udm_dev *udm_devp;
	int ret;
	unsigned int echo_irq = 0;
	/* Get the per-device structure that contains this cdev */
	udm_devp = container_of(inode->i_cdev, struct udm_dev, cdev);

	file->private_data = udm_devp;
	printk("\n device is openning \n");

	ret = gpio_request(30, "30");    // interrupt input mux for echo
	if(ret < 0)
	{
		printk("Error Requesting GPIO 30.\n");
		return -1;
	}

	ret = gpio_request(31, "31");
	if(ret < 0)
	{
		printk("Error Requesting GPIO 31.\n");
		return -1;
	}

	ret = gpio_request(0, "0"); //MUX for Trigger
	if(ret < 0)
	{
		printk("Error Requesting GPIO 0.\n");
		return -1;
	}
	
	ret = gpio_request(1, "1");  //MUX for echo
	if(ret < 0)
	{
		printk("Error Requesting GPIO 1.\n");

		return -1;
	}

	ret = gpio_request(14, "trigger"); // Trigger op
	if(ret < 0)
	{
		printk("Error Requesting IRQ 14.\n");
		return -1;
	}

	ret = gpio_request(15, "echo"); // Echo ip
	if(ret < 0)
	{
		printk("Error Requesting IRQ 15.\n");
		return -1;
	}



	ret = gpio_direction_output(gpio_0_Mux, 0); //31
	if(ret < 0)
	{
		printk("Error Setting GPIO_0_Mux output.\n");
	}
	ret = gpio_direction_output(0, 0);
	if(ret < 0)
	{
		printk("Error Setting GPIO_2_Mux output.\n");
	}
	
	ret = gpio_direction_output(trigger , 0); //14
	if(ret < 0)
	{
		printk("Error Setting trigger Pin Output.\n");
	}

	ret = gpio_direction_output(gpio_1_Mux, 0); //30
	if(ret < 0)
	{
		printk("Error Setting GPIO_2_Mux output.\n");
	}

	ret = gpio_direction_output(1, 0);
	if(ret < 0)
	{
		printk("Error Setting GPIO_2_Mux output.\n");
	}

	ret = gpio_direction_input(echo); 
	if(ret < 0)
	{
		printk("Error Setting Echo Pin Input.\n");
	}

	gpio_set_value_cansleep(gpio_0_Mux, 0); 
	gpio_set_value_cansleep(0, 0); 
	
	gpio_set_value_cansleep(gpio_1_Mux, 0); 
	gpio_set_value_cansleep(1, 0); 


	gpio_set_value(trigger, 0); 
	
	echo_irq = gpio_to_irq(echo);
	
	my_timer.function = UDM_Timer_Handler;
	my_timer.expires = jiffies + 15;
	my_timer.data = (unsigned long)&my_timer;
	init_timer(&my_timer);
	

	ret = request_irq(echo_irq, (irq_handler_t)UDM_IRQ_Handler, IRQF_TRIGGER_RISING, "UDM", (void *)(echo_irq));
	if(ret < 0)
	{
		printk("Error requesting IRQ: %d\n", ret);
	}	
	
	add_timer(&my_timer);

	return 0;
}

/*
 * Release gmem driver
 */
int UDM_release(struct inode *inode, struct file *file)
{
	
	unsigned int echo_irq =0;
	gpio_free(gpio_0_Mux);
	gpio_free(0);
	gpio_free(trigger);

	gpio_free(gpio_1_Mux);
	gpio_free(1);
	
	
	echo_irq = gpio_to_irq(echo);
	free_irq(echo_irq, (void *)(echo_irq));
	
	gpio_free(echo);	
	del_timer(&my_timer);
	printk("\n UDM is closing\n");
	
	return 0;
}

/*
 * Read to gmem driver
 */
ssize_t UDM_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	int echo_dist = 0,ret =0;
	struct udm_dev *udm_devp = file->private_data;
	
	echo_dist = udm_devp->distance;
	printk("\nDistance :%d\n",echo_dist);
	
	ret = copy_to_user((int *)buf,&echo_dist, sizeof(echo_dist));
	if(ret != 0){	
		printk("could not copy to user, ret= %d",ret);	
		return -EINVAL;
	}
	return 0;

}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations UDM_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= UDM_open,        /* Open method */
    .release	= UDM_release,     /* Release method */
    .read		= UDM_read,        /* Read method */
};

static int __init init_UDM(void)
{
	int ret =0;

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&udm_dev_num, 0, 1, DEVICE_NAME) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	udm_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structure */
	udm_devp = kmalloc(sizeof(struct udm_dev), GFP_KERNEL);
	if (!udm_devp) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	/* Request I/O region */
	//sprintf(udm_devp->name, DEVICE_NAME);

	/* Connect the file operations with the cdev */
	cdev_init(&udm_devp->cdev, &UDM_fops);
	udm_devp->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&udm_devp->cdev, (udm_dev_num), 1);
	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	udm_dev_device = device_create(udm_dev_class, NULL, MKDEV(MAJOR(udm_dev_num), 0), NULL, DEVICE_NAME);	
	
	return 0;
}

static void __exit exit_UDM(void)
{

	/* Release the major number */
	unregister_chrdev_region((udm_dev_num), 1);

	/* Destroy device */
	device_destroy (udm_dev_class, MKDEV(MAJOR(udm_dev_num), 0));
	cdev_del(&udm_devp->cdev);
	kfree(udm_devp);
	
	/* Destroy driver_class */
	class_destroy(udm_dev_class);
}

module_init(init_UDM);
module_exit(exit_UDM);
MODULE_LICENSE("GPL v2");
