/*
 * teeplc.c
 *
 *  Created on: 27 Feb 2023
 *      Author: dianfeng
 */
#define _GNU_SOURCE
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#include "teeplc.h"

//#include <linux/moduleparam.h>

#define DEVICE_NAME "TEEPLC"
#define DEVICE_CLASS_NAME "TEEPLC_CLASS"
#define DEVICE_NODE_NAME "TEEPLC_DEVICE"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alvin");
MODULE_DESCRIPTION("TEE-based PLC");

static dev_t teeplc_dev_t;
static struct cdev teeplc_cdev;
struct class *teeplc_class;
struct device *teeplc_device;





static int teeplc_open(struct inode *inode, struct file *filp)
{
	return 0;
}


static int teeplc_close(struct inode *inode, struct file *filp)
{
  return 0;
}

//struct mutex shadow_stack_init_mutex;
//DEFINE_MUTEX(shadow_stack_init_mutex);


static long teeplc_ioctl(struct file *file, unsigned int cmd, \
                        unsigned long arg)
{

    int ret = 0;
    int tid = current->pid;
    cmd = _IOC_NR(cmd);
    uint64_t *cmd_args = (uint64_t *)vmalloc(0x100);
    memset(cmd_args,0x0,0x100);
    //static __thread uint64_t *ss_base;
    switch(cmd) {
    	case 0x0: //for test
        		ret = copy_from_user(cmd_args, (void*)arg, 0x100);
    	    	uint b = *cmd_args;
    		uint c = *(cmd_args+1);
    		uint d = *(cmd_args+2);
    		uint e = *(cmd_args+3);
    		printk(KERN_INFO "Hello, teeplc\n");
    		printk(KERN_INFO "b is %u\n",b);
    		printk(KERN_INFO "c is %u\n",c);
    		asm volatile(
    				"mov x1, #0\n"
    				"mov x2, %0\n"
    				"mov x3, %1\n"
    				"ldr w0, =0xc7000001\n"
    				"smc #0\n"
    				::"r"(b),"r"(c):"x1","x2","x3","w0"
    				);
    		asm volatile(
    				"mov %0, x0\n"
    				"mov %1, x1\n"
    				"mov %2, x2\n"
    				"mov %3, x3\n"
    				:"=r"(b),"=r"(c),"=r"(d),"=r"(e)::"x0","x1","x2","x3"
    		);
		printk(KERN_INFO "new b is %u\n",b);
		printk(KERN_INFO "new c is %u\n",c);
		printk(KERN_INFO "new d is %u\n",d);
		printk(KERN_INFO "new e is %u\n",e);
		
		*cmd_args = b;
		*(cmd_args+1)=c;
		*(cmd_args+2)=d;
		*(cmd_args+3)=e;
		
		ret = copy_to_user((void*)arg, cmd_args, 0x100);
		break;
    	case 0x1:
    		printk("enter step.\n");

    		asm volatile(
    			"mov x1, #1\n"
    			"mov x2, %0\n"
    			"ldr w0, =0xc7000001\n"
    			"smc #0\n"
    			::"r"(tid):"x1","x2","w0"
    		);    		
    		asm volatile(
    			"mov %0, x0\n"
    			:"=r"(ret)::"x0"
    		);
    		printk("enter step. atf success, tid is %d, ret is %d.\n",tid,ret);
    		break;
    	case 0x2:
    		printk("save step.\n");
    		ret = copy_from_user(cmd_args, (void*)arg, 0x100);
    		if(ret) printk("copy from user error");
    		
    		asm volatile(
    			"mov x1, #2\n"
    			"mov x2, %0\n"
    			"mov x3, %1\n"
    			"ldr w0, =0xc7000001\n"
    			"smc #0\n"
    			::"r"(tid),"r"((unsigned long)(*cmd_args)):"x1","x2","x3","w0"
    		);
    		asm volatile(
    			"mov %0, x0\n"
    			:"=r"(ret)::"x0"
    		);

    		printk("save step. tid is %d,  ret is %d, cmd_args is %ld.\n", tid, ret, (unsigned long)(*cmd_args));
    		//printk("save lr in ATF.\n");
    		break;
    	case 0x3:
    		printk("restore step.\n");
    		//uint64_t *tmp_ss_base = get_thread_area(ss_base);
    		unsigned long return_address;
    		//printk("restore step. get tls success.\n");
    		asm volatile(
    			"mov x1, #3\n"
    			"mov x2, %0\n"
    			"ldr w0, =0xc7000001\n"
    			"smc #0\n"
    			::"r"(tid):"x1","x2","w0"
    		);
    		asm volatile(
    			"mov %0, x0\n"
    			"mov %1, x1\n"
    			:"=r"(ret),"=r"(return_address)::"x0","x1"
    		);
    		printk("restore step. atf return success. tid is %d, ret is %d, return_address is %ld\n", tid, ret, return_address);
    		ret = copy_to_user((void*)arg, &return_address, sizeof(return_address));
    		if(ret) printk("copy to user error.\n");
    		printk("restore step. copy to user success.\n");
    		break;
    	case 0x4:
    		printk("leave step.\n");
    		ret = copy_from_user(cmd_args, (void*)arg, 0x100);
    		
    		
    		//uint64_t *tmp_ss_base = get_thread_area(ss_base);
    		
    		printk("leave step. subtid is %d.\n",(int)(*cmd_args));
    		asm volatile(
    			"mov x1, #4\n"
    			"mov x2, %0\n"
    			"ldr w0, =0xc7000001\n"
    			"smc #0\n"
    			::"r"((int)(*cmd_args)):"x1","x2","w0"
    		);
    		asm volatile(
    			"mov %0, x0\n"
    			:"=r"(ret)::"x0"
    		);
    		printk("free shadow stack in ATF, tid is %d, subtid is %d, ret is %d.\n", tid, (int)(*cmd_args), ret);
    		break;
    	
	case 0x5:
		asm volatile(
    			"mov x1, #5\n"
    			"ldr w0, =0xc7000001\n"
    			"smc #0\n"
    			:::"x1","w0"
    		);
    		asm volatile(
    			"mov %0, x0\n"
    			:"=r"(ret)::"x0"
    		);
    		printk("free whole shadow stack in ATF, tid is %d, ret is %d.\n", tid, ret);
    		break;
  case 0x6:
    printk("log writing.");
    
    break;




    default:
        return -ENOTTY;
    }
    vfree(cmd_args);

    return ret;
}

static const struct file_operations teeplc_driver_fops = {
    .owner = THIS_MODULE,
    .open = teeplc_open,
    .release = teeplc_close,
    .unlocked_ioctl = teeplc_ioctl,
};


static int __init teeplc_init(void)
{
	int rc;
	rc = alloc_chrdev_region(&teeplc_dev_t,0,1,DEVICE_NAME);
	if(rc < 0){
		pr_err("failed to register device.\n");
		return rc;
	}

	printk("MAJOR is %d\n",MAJOR(teeplc_dev_t));
	printk("MINOR is %d\n",MINOR(teeplc_dev_t));

	cdev_init(&teeplc_cdev, &teeplc_driver_fops);
	rc = cdev_add(&teeplc_cdev,teeplc_dev_t,1);
	if(rc < 0){
		pr_err("cdev_add failed!");
		return rc;
	}
	teeplc_class = class_create(THIS_MODULE,DEVICE_CLASS_NAME);
	teeplc_device = device_create(teeplc_class,NULL,teeplc_dev_t,NULL,DEVICE_NODE_NAME);

	printk("teeplc driver initial successfully!\n");
  
	uint b = 1;
	uint c = 3;
	uint d = 4;
	uint e = 4;
	printk(KERN_INFO "Hello, teeplc\n");
	printk(KERN_INFO "b is %u\n",b);
	printk(KERN_INFO "c is %u\n",c);
	asm volatile(
			"mov x1, #0\n"
			"mov x2, %0\n"
			"mov x3, %1\n"
			"ldr w0, =0xc7000001\n"
			"smc #0\n"
			::"r"(b),"r"(c):"x1","x2","x3","w0"
			);
	asm volatile(
			"mov %0, x0\n"
			"mov %1, x1\n"
			"mov %2, x2\n"
			"mov %3, x3\n"
			:"=r"(b),"=r"(c),"=r"(d),"=r"(e)::"x0","x1","x2","x3"
	);
	printk(KERN_INFO "new b is %u\n",b);
	printk(KERN_INFO "new c is %u\n",c);
	printk(KERN_INFO "new d is %u\n",d);
	printk(KERN_INFO "new e is %u\n",e);
	return 0;
}

static void __exit teeplc_exit(void)
{
	printk("exit module.\n");
    cdev_del(&teeplc_cdev);
    unregister_chrdev_region(teeplc_dev_t,1);
    device_destroy(teeplc_class,1);
    class_destroy(teeplc_class);
    printk("teeplc module exit.\n");

	printk(KERN_INFO "Bye, teeplc\n");

}

module_init(teeplc_init);
module_exit(teeplc_exit);


