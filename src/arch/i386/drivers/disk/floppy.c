#if	(TRY_FLOPPY_FIRST > 0)

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#include "etherboot.h"

#define BOOTSECT ((char *)0x7C00)
#define BOOTSIG  ((unsigned short *)BOOTSECT)[0xFF]
#define PARTTAB  ((partentry *)(BOOTSECT+0x1BE))

typedef struct partentry {
	unsigned char flags;
	unsigned char start_head;
	unsigned char start_sector;
	unsigned char start_cylinder;
	unsigned char type;
	unsigned char end_head;
	unsigned char end_sector;
	unsigned char end_cylinder;
	unsigned long offset;
	unsigned long length;
} partentry;

static unsigned int disk_read_retry(int dev,int c,int h,int s)
{
	int retry = 3,rc;

	while (retry-- && (rc = pcbios_disk_read(dev,c,h,s,BOOTSECT)) != 0);
	if (BOOTSIG != 0xAA55) {
		printf("not a boot sector");
		return(0xFFFF); }
	return(rc);
}

int bootdisk(int dev,int part)
{
	int rc;

	disk_init();
	if ((rc = disk_read_retry(dev,0,0,1)) != 0) {
	readerr:
		if (rc != 0xFFFF)
			printf("read error (%#hhX)",rc/256);
		return(0); }
	if (part) {
		partentry *ptr;

		if (part >= 5) {
			int i;
			for (i = 0, ptr = PARTTAB; ptr->type != 5; ptr++)
				if (++i == 4) {
					printf("partition not found");
					return(0); }
			if ((rc = disk_read_retry(dev,
						  ptr->start_cylinder,
						  ptr->start_head,
						  ptr->start_sector)) != 0)
				goto readerr;
			part -= 4; }
		if (!(ptr = PARTTAB-1+part)->type) {
			printf("empty partition");
			return(0); }
		if ((rc = disk_read_retry(dev,
					  ptr->start_cylinder,
					  ptr->start_head,
					  ptr->start_sector)) != 0)
			goto readerr; }
	cleanup();
	gateA20_unset();
	/* Set %edx to device number to emulate BIOS
	   Fortunately %edx is not used after this */
	__asm__("movl %0,%%edx" : : "g" (dev));
	xstart((unsigned long)BOOTSECT, 0, 0);
	return(0);
}

#endif

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
