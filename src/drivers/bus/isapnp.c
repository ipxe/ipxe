#ifdef CONFIG_ISA
/**************************************************************************
*
*    isapnp.c -- Etherboot isapnp support for the 3Com 3c515
*    Written 2002-2003 by Timothy Legge <tlegge@rogers.com>
*
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*    Portions of this code:
*	Copyright (C) 2001  P.J.H.Fox (fox@roestock.demon.co.uk)
*
*
*    REVISION HISTORY:
*    ================
*    Version 0.1 April 26, 2002 	TJL
*    Version 0.2 01/08/2003		TJL Moved outside the 3c515.c driver file
*    Version 0.3 Sept 23, 2003	timlegge Change delay to currticks
*		
*
*    Indent Options: indent -kr -i8
***************************************************************************/

/* to get some global routines like printf */
#include "etherboot.h"
#include "timer.h"
#include "isapnp.h"

static int pnp_card_csn = 0;

void isapnp_wait(unsigned int nticks)
{
	unsigned int to = currticks() + nticks;
	while (currticks() < to)
		/* Wait */ ;
}

/* The following code is the ISA PNP logic required to activate the 3c515 */
/* PNP Defines */
#define IDENT_LEN 9
#define NUM_CARDS 128

/* PNP declares */
static unsigned char serial_identifier[NUM_CARDS + 1][IDENT_LEN];
static unsigned char isapnp_checksum_value;
static char initdata[INIT_LENGTH] = INITDATA;
int read_port = 0;


/* PNP Prototypes */
static int Isolate(void);
static int do_isapnp_isolate(void);
static int isapnp_build_device_list(void);
static int isapnp_isolate_rdp_select(void);
static int isapnp_next_rdp(void);
static void isapnp_peek(unsigned char *data, int bytes);
static void send_key(void);
static unsigned char isapnp_checksum(unsigned char *data);
int Config(int csn);

void config_pnp_device(void)
{
	/* PNP Configuration */
	printf("Probing/Configuring ISAPNP devices\n");
	if (!read_port) {
		Isolate();
		if (pnp_card_csn)
			Config(pnp_card_csn);
	}
}

/* Isolate all the PNP Boards on the ISA BUS */
static int Isolate(void)
{
	int cards = 0;
	if (read_port < 0x203 || read_port > 0x3ff) {
		cards = do_isapnp_isolate();
		if (cards < 0 || (read_port < 0x203 || read_port > 0x3ff)) {
			printf("No Plug & Play device found\n");
			return 0;
		}
	}
	isapnp_build_device_list();
#ifdef EDEBUG
	printf("%d Plug & Play device found\n", cards);
#endif
	return 0;
}

static int do_isapnp_isolate(void)
{
	unsigned char checksum = 0x6a;
	unsigned char chksum = 0x00;
	unsigned char bit = 0x00;
	unsigned char c1, c2;
	int csn = 0;
	int i;
	int iteration = 1;

	read_port = 0x213;
	if (isapnp_isolate_rdp_select() < 0)
		return -1;

	while (1) {
		for (i = 1; i <= 64; i++) {
			c1 = READ_DATA;
			isapnp_wait(1);
			c2 = READ_DATA;
			isapnp_wait(1);
			if (c1 == 0x55) {
				if (c2 == 0xAA) {
					bit = 0x01;
				}
			}
			checksum =
			    ((((checksum ^ (checksum >> 1)) & 0x01) ^ bit)
			     << 7) | (checksum >> 1);
			bit = 0x00;
		}
#ifdef EDEBUG
		printf("Calc checksum %d", checksum);
#endif
		for (i = 65; i <= 72; i++) {
			c1 = READ_DATA;
			udelay(250);
			c2 = READ_DATA;
			udelay(250);
			if (c1 == 0x55) {
				if (c2 == 0xAA)
					chksum |= (1 << (i - 65));
			}
		}
#ifdef EDEBUG
		printf("Actual checksum %d", chksum);
#endif
		if (checksum != 0x00 && checksum == chksum) {
			csn++;
			serial_identifier[csn][iteration] >>= 1;
			serial_identifier[csn][iteration] |= bit;
			CARDSELECTNUMBER;
#ifdef EDEBUG
			printf("Writing csn: %d", csn);
#endif
			WRITE_DATA(csn);
			udelay(250);
			iteration++;
			/* Force all cards without a CSN into Isolation state */
			Wake(0);
			SetRdPort(read_port);
			udelay(1000);
			SERIALISOLATION;
			udelay(1000);
			goto __next;
		}
		if (iteration == 1) {
			read_port += READ_ADDR_STEP;
			if (isapnp_isolate_rdp_select() < 0)
				return -1;
		} else if (iteration > 1) {
			break;
		}
	      __next:
		checksum = 0x6a;
		chksum = 0x00;
		bit = 0x00;
	}
	return csn;
}

/*
 *  Build device list for all present ISA PnP devices.
 */
static int isapnp_build_device_list(void)
{
	int csn, device, vendor, serial;
	unsigned char header[9], checksum;
	for (csn = 1; csn <= 10; csn++) {
		Wake(csn);
		isapnp_peek(header, 9);
		checksum = isapnp_checksum(header);
#ifdef EDEBUG
		printf
		    ("vendor: 0x%hX:0x%hX:0x%hX:0x%hX:0x%hX:0x%hX:0x%hX:0x%hX:0x%hX\n",
		     header[0], header[1], header[2], header[3], header[4],
		     header[5], header[6], header[7], header[8]);
		printf("checksum = 0xhX\n", checksum);
#endif
		/* Don't be strict on the checksum, here !
		   e.g. 'SCM SwapBox Plug and Play' has header[8]==0 (should be: b7) */
		if (header[8] == 0);
		else if (checksum == 0x00 || checksum != header[8])	/* not valid CSN */
			continue;

		vendor = (header[1] << 8) | header[0];
		device = (header[3] << 8) | header[2];
		serial =
		    (header[7] << 24) | (header[6] << 16) | (header[5] <<
							     8) |
		    header[4];
		if (vendor == 0x6D50)
			if (device == 0x5150) {
				printf
				    ("\nFound 3Com 3c515 PNP Card!\n Vendor ID: 0x%hX, Device ID: 0x%hX, Serial Num: 0x%hX\n",
				     vendor, device, serial);
				pnp_card_csn = csn;
			}
		isapnp_checksum_value = 0x00;
	}
	return 0;
}

int Config(int csn)
{
#define TIMEOUT_PNP     100
	unsigned char id[IDENT_LEN];
	int i, x;
	Wake(csn);
	udelay(1000);
	for (i = 0; i < IDENT_LEN; i++) {
		for (x = 1; x < TIMEOUT_PNP; x++) {
			if (STATUS & 1)
				break;
			udelay(1000);
		}
		id[i] = RESOURCEDATA;
#ifdef EDEBUG
		printf(" 0x%hX ", id[i]);
#endif
	}
#ifdef EDEBUG
	printf("Got The status bit\n");
#endif
	/*Set Logical Device Register active */
	LOGICALDEVICENUMBER;
	/* Specify the first logical device */
	WRITE_DATA(0);


	/* Apparently just activating the card is enough
	   for Etherboot to detect it.  Why bother with the
	   following code.  Left in place in case it is
	   later required  */
/*==========================================*/
	/* set DMA */
/*    ADDRESS(0x74 + 0);
    WRITE_DATA(7); */

	/*Set IRQ */
/*    udelay(1000);
    ADDRESS(0x70 + (0 << 1));
    WRITE_DATA(9);
    udelay(1000); */
/*=============================================*/
	/*Activate */
	ACTIVATE;
	WRITE_DATA(1);
	udelay(250);
	/* Ask for access to the Wait for Key command - ConfigControl register */
	CONFIGCONTROL;
	/* Write the Wait for Key Command to the ConfigControl Register */
	WRITE_DATA(CONFIG_WAIT_FOR_KEY);
	/* As per doc. Two Write cycles of 0x00 required befor the Initialization key is sent */
	ADDRESS(0);
	ADDRESS(0);

	return 1;
}

static void send_key(void)
{
	int i;
	/* Ask for access to the Wait for Key command - ConfigControl register */
	CONFIGCONTROL;
	/* Write the Wait for Key Command to the ConfigControl Register */
	WRITE_DATA(CONFIG_WAIT_FOR_KEY);
	/* As per doc. Two Write cycles of 0x00 required befor the Initialization key is sent */
	ADDRESS(0);
	ADDRESS(0);
	/* 32 writes of the initiation key to the card */
	for (i = 0; i < INIT_LENGTH; i++)
		ADDRESS(initdata[i]);
}

static void isapnp_peek(unsigned char *data, int bytes)
{
	int i, j;
	unsigned char d = 0;

	for (i = 1; i <= bytes; i++) {
		for (j = 0; j < 20; j++) {
			d = STATUS;
			if (d & 1)
				break;
			udelay(100);
		}
		if (!(d & 1)) {
			if (data != NULL)
				*data++ = 0xff;
			continue;
		}
		d = RESOURCEDATA;	/* PRESDI */
		isapnp_checksum_value += d;
		if (data != NULL)
			*data++ = d;
	}
}

/*
 *  Compute ISA PnP checksum for first eight bytes.
 */
static unsigned char isapnp_checksum(unsigned char *data)
{
	int i, j;
	unsigned char checksum = 0x6a, bit, b;

	for (i = 0; i < 8; i++) {
		b = data[i];
		for (j = 0; j < 8; j++) {
			bit = 0;
			if (b & (1 << j))
				bit = 1;
			checksum =
			    ((((checksum ^ (checksum >> 1)) & 0x01) ^ bit)
			     << 7) | (checksum >> 1);
		}
	}
	return checksum;
}
static int isapnp_next_rdp(void)
{
	int rdp = read_port;
	while (rdp <= 0x3ff) {
		/*
		 *      We cannot use NE2000 probe spaces for ISAPnP or we
		 *      will lock up machines.
		 */
		if ((rdp < 0x280 || rdp > 0x380)) {
			read_port = rdp;
			return 0;
		}
		rdp += READ_ADDR_STEP;
	}
	return -1;
}

static int isapnp_isolate_rdp_select(void)
{
	send_key();
	/* Control: reset CSN and conditionally everything else too */
	CONFIGCONTROL;
	WRITE_DATA((CONFIG_RESET_CSN | CONFIG_WAIT_FOR_KEY));
	mdelay(2);

	send_key();
	Wake(0);

	if (isapnp_next_rdp() < 0) {
		/* Ask for access to the Wait for Key command - ConfigControl register */
		CONFIGCONTROL;
		/* Write the Wait for Key Command to the ConfigControl Register */
		WRITE_DATA(CONFIG_WAIT_FOR_KEY);
		return -1;
	}

	SetRdPort(read_port);
	udelay(1000);
	SERIALISOLATION;
	udelay(1000);
	return 0;
}
#endif  /* CONFIG_ISA */
