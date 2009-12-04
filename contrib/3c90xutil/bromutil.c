/* 
 * readutil.c - perform various control ops on the 3c509b bios rom
 *
 */

#ifndef __i386__
#  error "This program can't compile or run on non-intel computers"
#else

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef __FreeBSD__

#include <fcntl.h>
#include <machine/cpufunc.h>

#define OUTB(data, port) 	outb(port, data)
#define OUTW(data, port) 	outw(port, data)
#define OUTL(data, port) 	outl(port, data)

#else

#include <sys/io.h>

#define OUTB(data, port) 	outb(data, port)
#define OUTW(data, port) 	outw(data, port)
#define OUTL(data, port) 	outl(data, port)

#endif

/*
 * write_eeprom() and enum definitions are copied from vortex-diag.c,
 * Copyright 1997-2004 by Donald Becker.
 *	This software may be used and distributed according to the terms of
 *	the GNU General Public License (GPL), incorporated herein by reference.
 *	Contact the author for use under other terms.
 */

enum vortex_cmd {
	TotalReset = 0<<11, SelectWindow = 1<<11, StartCoax = 2<<11,
	RxDisable = 3<<11, RxEnable = 4<<11, RxReset = 5<<11,
	UpStall = 6<<11, UpUnstall = (6<<11)+1,
	DownStall = (6<<11)+2, DownUnstall = (6<<11)+3,
	RxDiscard = 8<<11, TxEnable = 9<<11, TxDisable = 10<<11, TxReset = 11<<11,
	FakeIntr = 12<<11, AckIntr = 13<<11, SetIntrEnb = 14<<11,
	SetStatusEnb = 15<<11, SetRxFilter = 16<<11, SetRxThreshold = 17<<11,
	SetTxThreshold = 18<<11, SetTxStart = 19<<11,
	StartDMAUp = 20<<11, StartDMADown = (20<<11)+1, StatsEnable = 21<<11,
	StatsDisable = 22<<11, StopCoax = 23<<11, SetFilterBit = 25<<11,
};

enum Window0 {
	Wn0EepromCmd = 10,		/* Window 0: EEPROM command register. */
	Wn0EepromData = 12,		/* Window 0: EEPROM results register. */
	IntrStatus=0x0E,		/* Valid in all windows. */
};

enum Win0_EEPROM_cmds {
	EEPROM_Read = 2, EEPROM_WRITE = 1, EEPROM_ERASE = 3,
	EEPROM_EWENB = 0xC,		/* Enable erasing/writing for 10 msec. */
	EEPROM_EWDIS = 0x0,		/* Disable EWENB before 10 msec timeout. */
};

#define debug 1
static void write_eeprom(long ioaddr, int addrlen, int index, int value)
{
	int timer;

	/* Verify that the EEPROM is idle. */
	for (timer = 1620; inw(ioaddr + Wn0EepromCmd) & 0x8000;)
		if (--timer < 0)
			goto error_return;
	/* Enable writing: EEPROM_EWENB | 110000.... */
	OUTW(3 << (addrlen-2), ioaddr + Wn0EepromCmd);
	for (timer = 400; inw(ioaddr + Wn0EepromCmd) & 0x8000;) {
		if (--timer < 0)
			goto error_return;
	}
	if (debug)
		fprintf(stderr, "EEPROM write enable took %d ticks!\n", 400 - timer);
	OUTW((EEPROM_ERASE << addrlen) + index, ioaddr + Wn0EepromCmd);
	for (timer = 16000; inw(ioaddr + Wn0EepromCmd) & 0x8000;)
		if (--timer < 0) {
			fprintf(stderr, "EEPROM failed to erase index %d!\n", index);
			return;
		}
	if (debug)
		fprintf(stderr, "EEPROM erased index %d after %d ticks!\n",
				index, 16000-timer);
	OUTW(3 << (addrlen-2), ioaddr + Wn0EepromCmd);
	for (timer = 400; inw(ioaddr + Wn0EepromCmd) & 0x8000;) {
		if (--timer < 0)
			goto error_return;
	}
	if (debug)
		fprintf(stderr, "EEPROM write enable took %d ticks!\n", 400-timer);
	OUTW(value, ioaddr + Wn0EepromData);
	OUTW((EEPROM_WRITE << addrlen) + index, ioaddr + Wn0EepromCmd);
	for (timer = 16000; inw(ioaddr + Wn0EepromCmd) & 0x8000;)
		if (--timer < 0)
			goto error_return;
	if (debug)
		fprintf(stderr, "EEPROM wrote index %d with 0x%4.4x after %d ticks!\n",
				index, value, 16000-timer);
	return;
error_return:
	fprintf(stderr, "Failed to write EEPROM location %d with 0x%4.4x!\n",
			index, value);
}

int main(int argc, char **argv)
{
	unsigned int i, j, n;
	unsigned int ioaddr;
	unsigned long recvrstat;
	unsigned char buf[128];
	unsigned char b;

	if (argc != 3) {
		printf
		    ("Usage: romid ioaddr [erase|protect|unprotect|id|bootrom|read >file|prog <file]\n");
		exit(-1);
	}
#ifdef __FreeBSD__
	/* get permissions for in/out{blw} */
	open("/dev/io", O_RDONLY, 0);
#else
	setuid(0);		/* if we're setuid, do it really */
	if (iopl(3)) {
		perror("iopl()");
		exit(1);
	}
#endif

	sscanf(argv[1], "%x", &ioaddr);
	/* Set the register window to 3 for the 3c905b */
	OUTW(0x803, ioaddr + 0xe);
	recvrstat = inl(ioaddr);	/* save the receiver status */
	/* set the receiver type to MII so the full bios rom address space
	   can be accessed */
	OUTL((recvrstat & 0xf00fffff) | 0x00600000, ioaddr);

	/* Set the register window to 0 for the 3c905b */
	OUTW(0x800, ioaddr + 0xe);

	if (strcmp(argv[2], "erase") == 0) {
		/* do the funky chicken to erase the rom contents */
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xaa, ioaddr + 0x8);
		OUTL(0x2aaa, ioaddr + 0x4);
		OUTB(0x55, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0x80, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xaa, ioaddr + 0x8);
		OUTL(0x2aaa, ioaddr + 0x4);
		OUTB(0x55, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0x10, ioaddr + 0x8);
		printf("Bios ROM at %04x has been erased\n", ioaddr);
	} else if (strcmp(argv[2], "protect") == 0) {
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xaa, ioaddr + 0x8);
		OUTL(0x2aaa, ioaddr + 0x4);
		OUTB(0x55, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xa0, ioaddr + 0x8);
		printf
		    ("Software Data Protection for Bios ROM at %04x has been enabled\n",
		     ioaddr);
	} else if (strcmp(argv[2], "unprotect") == 0) {
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xaa, ioaddr + 0x8);
		OUTL(0x2aaa, ioaddr + 0x4);
		OUTB(0x55, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0x80, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xaa, ioaddr + 0x8);
		OUTL(0x2aaa, ioaddr + 0x4);
		OUTB(0x55, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0x20, ioaddr + 0x8);
		printf
		    ("Software Data Protection for Bios ROM at %04x has been disabled\n",
		     ioaddr);
	} else if (strcmp(argv[2], "id") == 0) {
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xaa, ioaddr + 0x8);
		OUTL(0x2aaa, ioaddr + 0x4);
		OUTB(0x55, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0x90, ioaddr + 0x8);
		/* 10ms delay needed */
		printf("Manufacturer ID - ");
		/* manuf. id */
		OUTL(0x0000, ioaddr + 0x4);
		printf("%02x\n", inb(ioaddr + 0x8));
		/* device id */
		OUTL(0x0001, ioaddr + 0x4);
		printf("Device ID - %02x\n", inb(ioaddr + 0x8));
		/* undo the funky chicken */
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xaa, ioaddr + 0x8);
		OUTL(0x2aaa, ioaddr + 0x4);
		OUTB(0x55, ioaddr + 0x8);
		OUTL(0x5555, ioaddr + 0x4);
		OUTB(0xf0, ioaddr + 0x8);
	} else if(strcmp(argv[2], "bootrom") == 0) {
		printf("bootrom fix\n");
		write_eeprom(ioaddr, 6, 19, 0x160);
	} else if (strcmp(argv[2], "read") == 0) {
		for (i = 0; i < 65536; i++) {
			OUTL(i, ioaddr + 0x4);
			b = inb(ioaddr + 0x8);
			write(1, &b, 1);
		}
	} else if (strcmp(argv[2], "prog") == 0) {
		/* program the rom in 128 bute chunks */
		for (i = 0, n = 0; i < 65536; i += n) {
			n = read(0, buf, 128);
			if (n == 0)
				break;
			if (n < 0) {
				perror("File Error");
				exit(-3);
			}
			/* disable SDP temporarily for programming a sector */
			OUTL(0x5555, ioaddr + 0x4);
			OUTB(0xaa, ioaddr + 0x8);
			OUTL(0x2aaa, ioaddr + 0x4);
			OUTB(0x55, ioaddr + 0x8);
			OUTL(0x5555, ioaddr + 0x4);
			OUTB(0xa0, ioaddr + 0x8);
			for (j = 0; j < n; j++) {
				OUTL(i + j, ioaddr + 0x4);
				OUTB(buf[j], ioaddr + 0x8);
			}
			/* wait for the programming of this sector to coomplete */
			while (inb(ioaddr + 0x8) != buf[j - 1]);
		}
	}

	/* Set the register window to 3 for the 3c905b */
	OUTW(0x803, ioaddr + 0xe);
	/* restore the receiver status */
	OUTL(recvrstat, ioaddr);
	return 0;
}

#endif				/* __i386__ */
