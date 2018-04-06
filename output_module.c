// COP 4600 - HW 3
// Christopher Padilla, Richard Tsai, Matthew Winchester
// output_module.c

#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#define DEVICE_NAME "output_module"
#define CLASS_NAME "output"
#define BUFF_LEN 2048

extern char * fifo_buffer_ptr;
extern short fifo_buffer_size;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christofer Padilla, Richard Tsai, Matthew Winchester");
MODULE_DESCRIPTION("COP4600 - Programming Assignment 3 - Output_Module");
MODULE_VERSION("2.0");

static DEFINE_MUTEX(output_mutex);

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);

static struct file_operations fops = {
  .open = dev_open,
  .read = dev_read,
  .release = dev_release
};

static int majorNum;
static int numberOpens = 0;
static struct class* charClass = NULL;
static struct device* charDevice = NULL;

static char * temp;

static int __init char_init(void)
{
  fifo_buffer_ptr = (char *) vmalloc(sizeof(char) * BUFF_LEN);

  printk(KERN_INFO "FIFODev: Initializing FIFODev\n");

  majorNum = register_chrdev(0, DEVICE_NAME, &fops);

  if(majorNum < 0)
  {
    printk(KERN_ALERT "FIFODev: Failed to register a major number\n");
    return majorNum;
  }

  printk(KERN_INFO "FIFODev: Registered correctly with major number %d\n", majorNum);

  charClass = class_create(THIS_MODULE, CLASS_NAME);
  if(IS_ERR(charClass))
  {
    unregister_chrdev(majorNum, DEVICE_NAME);
    printk(KERN_ALERT "FIFODev: Failed to register device class.\n");
    return PTR_ERR(charClass);
  }

  printk(KERN_INFO "FIFODev: device class registered correctly\n");

  charDevice = device_create(charClass, NULL, MKDEV(majorNum, 0), NULL, DEVICE_NAME);
  if(IS_ERR(charDevice))
  {
    class_destroy(charClass);
    unregister_chrdev(majorNum, DEVICE_NAME);
    printk(KERN_ALERT "FIFODev: Failed to create the device.\n");
    return PTR_ERR(charDevice);
  }

  mutex_init(&output_mutex);
  printk(KERN_INFO "FIFODev: Device class created\n");

  return 0;
}

static void __exit char_exit(void)
{
  mutex_destroy(&output_mutex);
  device_destroy(charClass, MKDEV(majorNum, 0));
  class_unregister(charClass);
  class_destroy(charClass);
  unregister_chrdev(majorNum, DEVICE_NAME);
  printk(KERN_INFO "FIFODev: Exiting");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
  if(!mutex_trylock(&output_mutex))
  {
    printk(KERN_INFO "FIFODev: Device in use by another process");
    return -EBUSY;
  }
  printk(KERN_INFO "FIFODev: Device has been opened %d time(s)\n", ++numberOpens);
  return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
  mutex_unlock(&output_mutex);
  printk(KERN_INFO "FIFODev: Device closed\n");
  return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
  int error_count = 0;

  if(fifo_buffer_size > 0)
  {

    if (len > BUFF_LEN)
    {
        printk(KERN_INFO "FIFODev: User requested %zu bytes, more bytes than maximum buffer size: %d. Only %d bytes will be returned.\n", len, BUFF_LEN, fifo_buffer_size);

        error_count = copy_to_user(buffer, fifo_buffer_ptr, fifo_buffer_size);
        temp = (char *) vmalloc(sizeof(char) * BUFF_LEN);
	temp[0] = '\0';
	vfree(fifo_buffer_ptr);
	fifo_buffer_ptr=temp;
	if(error_count == 0)
        {
	  printk(KERN_INFO "FIFODev: %d characters sent to user\n", fifo_buffer_size);
	  fifo_buffer_size = 0;

          return 0;

        }
    }

    else
    {
	printk(KERN_INFO "FIFODev: User wants %zu bytes, in our buffer there are %d\n",len,fifo_buffer_size);

        error_count = copy_to_user(buffer, fifo_buffer_ptr, len);


	temp = (char *) vmalloc(sizeof(char) * BUFF_LEN);

	strncpy(temp, fifo_buffer_ptr+len, fifo_buffer_size-len);

	vfree(fifo_buffer_ptr);

	fifo_buffer_ptr = temp;

	//fifo_buffer_ptr[fifo_buffer_size-len] = '\0';

        if(error_count == 0)
        {
           printk(KERN_INFO "FIFODev: %d characters sent to user\n", fifo_buffer_size);
	   fifo_buffer_size = fifo_buffer_size - len;
           printk(KERN_INFO "FIFODev: After send fifo_buffer_size: %d\n",fifo_buffer_size);
           return 0;

        }


	printk(KERN_INFO "Errors ocurred \n");

    }

      printk(KERN_INFO "FIFODev: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;
  }

  printk(KERN_INFO "FIFODev: User tried to read the empty buffer\n");
  copy_to_user(buffer, fifo_buffer_ptr, 0);
  return 0;
}


module_init(char_init);
module_exit(char_exit);
