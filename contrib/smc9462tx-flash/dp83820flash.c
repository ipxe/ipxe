/*
   Kernel module for the dp83820 flash write utility. This code was written
   by Dave Ashley for NXTV, Inc.
   Copyright 2004 by NXTV, Inc.
   Written 20040219 by Dave Ashley.

   This code is released under the terms of the GPL. No warranty.

   THEORY: The dp83820 bootrom interface is flawed in that you can't
   read or write a single byte at a time, and this is required in order
   to write to flash devices like the AT29C512. So the workaround is
   to use the chips ability to map into memory the bootrom, then the cpu
   can directly do byte accesses.

   The problem is that a "feature" of the dp83820 is that when you map
   in the bootrom, you conveniently lose access to the PCI registers.
   So we need to do this in kernel space and wrap every access to the
   bootrom within interrupt_disable/restore, in case a network interrupt
   were to come in.

   This kernel module is very simple, it just creates a proc file
   /proc/dp83820
   If you write 3 bytes to this file you are doing a write to the flashrom:

Byte 1   2    3
   ALOW AHIGH DATA

   If you write 2 bytes to this file you are doing a read from the flashrom:
Byte 1   2
   ALOW AHIGH
   Then the next read from the file will return a single byte of what
   was at that location.

   You only get one shot at accessing the proc file, you need to then
   close/open if you want to do another access. This could probably be
   cleaned up pretty easily so more accesses can be done without having
   to close/open the file.     

*/


#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/module.h>


#define PROCNAME "dp83820"

struct pci_dev *mydev=0;
unsigned long loc;
unsigned char *addr=0;

unsigned char lastread;


int my_read_proc(char *buf, char **start,off_t offset,int count, int *eof,void *data)
{
int retval=0;

	if(count>0)
	{
		buf[0]=lastread;
		retval=1;
	}

	*eof=1;

	return retval;
}

int my_write_proc(struct file *file, const char *buffer, unsigned long count,
		void *data)
{
unsigned char *msg;

unsigned long flags;

	msg=(void *)buffer;
	save_flags(flags);
	cli();
	pci_write_config_dword(mydev, 0x30, loc | 1);

	switch(count)
	{
		case 2:
			lastread=addr[msg[0] | (msg[1]<<8)];
			break;
		case 3:
			addr[msg[0] | (msg[1]<<8)] = msg[2];
			break;
	}
	pci_write_config_dword(mydev, 0x30, loc);
	restore_flags(flags);
	return count;
}


struct proc_dir_entry *de=0;

int __init init_module(void)
{
int found=0;
	mydev=0;
	pci_for_each_dev(mydev)
	{
		if(mydev->vendor==0x100b && mydev->device==0x0022)
		{
			found=1;
			break;
		}
	}
	if(!found)
	{
		printk("Could not find DP83820 network device\n");
		return ENODEV;
	}

	de=create_proc_entry(PROCNAME,0,0);
	if(!de)
		return -1;
	de->data=0;
	de->read_proc=my_read_proc;
	de->write_proc=my_write_proc;

	loc=mydev->resource[PCI_ROM_RESOURCE].start;
	addr=ioremap_nocache(loc,0x10000);


	return 0;
}

void cleanup_module(void)
{
	if(de)
	{
		remove_proc_entry(PROCNAME,0);
		de=0;
	}
	if(addr)
	{
		iounmap(addr);
		addr=0;
	}
}

