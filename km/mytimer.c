/* Sam Beaulieu
 * EC 535, Spring 2016
 * Lab 3, 3/18/16
 * Source code for mytimer kernel module, to be used with ktimer
 */

/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <linux/timer.h> /* timer support */
#include <linux/string.h> /* string to long conversion */
#include <linux/seq_file.h> /* for sequence files */
#include <linux/proc_fs.h>

MODULE_LICENSE("Dual BSD/GPL");

/* declaration of functions */
static int mytimer_open(struct inode *inode, struct file *filp);
static int mytimer_release(struct inode *inode, struct file *filp);
static ssize_t mytimer_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t mytimer_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
static void mytimer_exit(void);
static int mytimer_init(void);
static int mytimer_fasync(int fd, struct file *filp, int mode);
void timer_callback(unsigned long data);
static int mytimer_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);


/* structure that declares the usual file access functions */
struct file_operations mytimer_fops = {
	read	: mytimer_read,
	write	: mytimer_write,
	open	: mytimer_open,
	release : mytimer_release,
	fasync 	: mytimer_fasync
};

/* declaration of the init and exit functions */
module_init(mytimer_init);
module_exit(mytimer_exit);

/* global variables of the driver */
/* major number */
static int mytimer_major = 61;
/* pointers to two timers */
static struct timer_list *mytimer;
/* asynchronous readers */
struct fasync_struct *async_queue; 
/* active timer */
int active = 0;
/* proc entry */
static struct proc_dir_entry *mytimer_proc;
/* time (in jiffies) loaded */
int time_loaded;
/* caller's PID */
int pid = 0;
/* timer name */
char *name;
/* original delay time */
int starting_delay;
/* state when reading */
int read_state = 0;


/* declare changeable module parameters */
module_param(mytimer_major, uint, S_IRUGO);

/* initialization function for mytimer */
static int mytimer_init(void)
{
	int result;
	time_loaded = jiffies;

	/* Registering device */
	result = register_chrdev(mytimer_major, "mytimer", &mytimer_fops);
	if (result < 0)
		return result;

	/* allocate space for two timers */
	mytimer = kmalloc(sizeof(struct timer_list *), GFP_KERNEL);

	/* verify there was enough kernel memory */
	if (!mytimer)
	{
		result = -ENOMEM;
		goto fail;
	}

	/* create proc entry */
	mytimer_proc = create_proc_entry("mytimer", 0, NULL);
	if(!mytimer_proc)
		return -ENOMEM;
	else
	{
		mytimer_proc->read_proc = mytimer_proc_read;
	}
	/* success */
	return 0;

fail:
	mytimer_exit();
	return result;
}

static int mytimer_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/* get expiration time and time since module load */
	int time_since_load = jiffies_to_msecs(jiffies-time_loaded)/1000;
	int time_left = jiffies_to_msecs(mytimer->expires-jiffies)/1000;

	/* output information to proc entry page */
	if(active == 1)
		count = sprintf(page, "Module name: mytimer\nTime since loaded (sec): %d\nProcess ID of user program: %d\nCommand name: ktimer\nTime until expiration (sec): %d\n", time_since_load, pid, time_left);
	else
		count = sprintf(page, "Module name: mytimer\nTime since loaded (sec): %d\n", time_since_load);
	return count;
}

static void mytimer_exit(void)
{
	/* freeing the major number */
	unregister_chrdev(mytimer_major, "mytimer");

	/* remove proc entry */
	remove_proc_entry("mytimer", NULL);

	/* freeing timer memory */
	if (mytimer) kfree(mytimer);
	if (name) kfree(name);
}

static int mytimer_open(struct inode *inode, struct file *filp)
{
	/* Success */
	return 0;
}

static int mytimer_release(struct inode *inode, struct file *filp)
{
	/* Success */
	mytimer_fasync(-1, filp, 0);
	return 0;
}

static ssize_t mytimer_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	char *temp;
	int time_left;

	/* allocate memory for outgoing data */
	temp = kmalloc(128*sizeof(char), GFP_KERNEL);

	/* if there is a timer currently running */
	if(active != 0 && read_state == 0)
	{
		/* get time left and convert to a string */
		time_left = jiffies_to_msecs(mytimer->expires-jiffies)/1000;
		sprintf(temp, "%d", time_left);

		/* transfering data to user space */ 
		if (copy_to_user(buf, temp, count))
			return -EFAULT;
	}
	/* if there is a timer with the same name that has been updated */
	else if(active != 0 && read_state == 1)
	{
		/* prepare output string */
		sprintf(temp, "-Timer %s has now been reset to %d seconds!\n", name, starting_delay);
		
		/* transfering data to user space */ 
		if (copy_to_user(buf, temp, count))
			return -EFAULT;

		read_state = 0;
	}
	/* if a timer already exists */
	else if(active != 0 && read_state == 2)
	{
		/* prepare output message */
		sprintf(temp, "-A timer exists already\n");

		/* transfering data to user space */ 
		if (copy_to_user(buf, temp, count))
			return -EFAULT;

		read_state = 0;
	}

	/* clean up and return */
	if(temp) kfree(temp);
	return count; 
}

static ssize_t mytimer_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	
	char *temp;
	unsigned long delay;

	/* create a temp buffer for incoming data */
	temp = kmalloc(128*sizeof(char), GFP_KERNEL);
	memset(temp, 0, 128);

	/* copy data from the user to temp buffer */
	if (copy_from_user(temp, buf, count))
		return -EFAULT;

	/* get delay and PID from passed string */
	delay = simple_strtol(temp, &temp, 10);
	temp++;
	pid = simple_strtol(temp, &temp, 10);
	temp++;

	/* if no timers are active, create a new one */
	if (active == 0)
	{
		/* allocate memory for and get name from passed string */
		if(name) kfree(name);
		name = kmalloc(sizeof(char)*strlen(temp)+1, GFP_KERNEL);
		memset(name, 0, strlen(name));
		strcpy(name, temp);

		/* create timer */
		setup_timer(mytimer, timer_callback, 0);
		mod_timer(mytimer, jiffies + msecs_to_jiffies(1000*delay));

		starting_delay = delay;
		active = 1;
	}
	/* else if a timer with the same name is running, update it */
	else if(strcmp(name, temp) == 0)
	{
		/* reset starting delay */
		starting_delay = delay;

		/* update timer */
		mod_timer(mytimer, jiffies+msecs_to_jiffies(1000*delay));
		read_state = 1;
	}
	/* if a timer with a different name already exists, do nothing */
	else
	{
		read_state = 2;
	}

	/* free allocated memory and return */
	if(temp) kfree(temp);
	return count;
}

void timer_callback(unsigned long data)
{
	/* stop the calling function from sleeping */
	if (async_queue)
		kill_fasync(&async_queue, SIGIO, POLL_IN);

	/* delete timer */
	del_timer(mytimer);
	active = 0;
}

static int mytimer_fasync(int fd, struct file *filp, int mode) {
	return fasync_helper(fd, filp, mode, &async_queue);
}

