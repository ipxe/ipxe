/**************************************************************************
*
*    isapnp.h -- Etherboot isapnp support for the 3Com 3c515
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
*				Copyright (C) 2001  P.J.H.Fox (fox@roestock.demon.co.uk)
*
*
*
*    REVISION HISTORY:
*    ================
*        Version 0.1 April 26, 2002 	TJL
*		 Version 0.2 01/08/2003			TJL Renamed from 3c515_isapnp.h
*
***************************************************************************/

/*extern int read_port;*/
/*#define DEBUG*/
#define ADDRESS_ADDR 0x0279
#define WRITEDATA_ADDR 0x0a79
/* MIN and MAX READ_ADDR must have the bottom two bits set */
#define MIN_READ_ADDR 0x0203
#define START_READ_ADDR 0x203
#define MAX_READ_ADDR 0x03ff
/* READ_ADDR_STEP must be a multiple of 4 */
#ifndef READ_ADDR_STEP
#define READ_ADDR_STEP 8
#endif

#ifdef EDEBUG
static int x;
#define ADDRESS(x) (outb(x, ADDRESS_ADDR), printf("\nAddress: %hX", x))
#define WRITE_DATA(x) (outb(x, WRITEDATA_ADDR), printf(" WR(%hX)", x & 0xff))
#define READ_DATA (x = inb(read_port), printf(" RD(%hX)", x & 0xff), x)
#define READ_IOPORT(p) (x = inb(p), printf(" [%hX](%hX)", p, x & 0xff), x)
#else				/* !DEBUG */
#define ADDRESS(x) outb(x, ADDRESS_ADDR)
#define WRITE_DATA(x) outb(x, WRITEDATA_ADDR)
#define READ_DATA inb(read_port)
#define READ_IOPORT(p) inb(p)
#endif				/* !DEBUG */



#define INIT_LENGTH 32

#define INITDATA { 0x6a, 0xb5, 0xda, 0xed, 0xf6, 0xfb, 0x7d, 0xbe,\
                   0xdf, 0x6f, 0x37, 0x1b, 0x0d, 0x86, 0xc3, 0x61,\
                   0xb0, 0x58, 0x2c, 0x16, 0x8b, 0x45, 0xa2, 0xd1,\
                   0xe8, 0x74, 0x3a, 0x9d, 0xce, 0xe7, 0x73, 0x39 }

/* Registers */
#define SetRdPort(x)		(ADDRESS(0x00),WRITE_DATA((x)>>2),read_port=((x)|3))
#define SERIALISOLATION 	ADDRESS(0x01)
#define CONFIGCONTROL		ADDRESS(0x02)
#define Wake(x)				(ADDRESS(0x03),WRITE_DATA(x))
#define RESOURCEDATA		(ADDRESS(0x04),READ_DATA)
#define STATUS          	(ADDRESS(0x05),READ_DATA)
#define CARDSELECTNUMBER	ADDRESS(0x06)
#define LOGICALDEVICENUMBER	ADDRESS(0x07)
#define ACTIVATE			ADDRESS(0x30)
#define IORANGECHECK		ADDRESS(0x31)

/* Bits */
#define CONFIG_RESET		0x01
#define CONFIG_WAIT_FOR_KEY 0x02
#define CONFIG_RESET_CSN	0x04
#define CONFIG_RESET_DRV	0x07

/* Short  Tags */
#define PnPVerNo_TAG		0x01
#define LogDevId_TAG		0x02
#define CompatDevId_TAG		0x03
#define IRQ_TAG			0x04
#define DMA_TAG			0x05
#define StartDep_TAG		0x06
#define EndDep_TAG		0x07
#define IOport_TAG		0x08
#define FixedIO_TAG		0x09
#define RsvdShortA_TAG		0x0A
#define RsvdShortB_TAG		0x0B
#define RsvdShortC_TAG		0x0C
#define RsvdShortD_TAG		0x0D
#define VendorShort_TAG		0x0E
#define End_TAG			0x0F
/* Long  Tags */
#define MemRange_TAG		0x81
#define ANSIstr_TAG		0x82
#define UNICODEstr_TAG		0x83
#define VendorLong_TAG		0x84
#define Mem32Range_TAG		0x85
#define FixedMem32Range_TAG	0x86
#define RsvdLong0_TAG		0xF0
#define RsvdLong1_TAG		0xF1
#define RsvdLong2_TAG		0xF2
#define RsvdLong3_TAG		0xF3
#define RsvdLong4_TAG		0xF4
#define RsvdLong5_TAG		0xF5
#define RsvdLong6_TAG		0xF6
#define RsvdLong7_TAG		0xF7
#define RsvdLong8_TAG		0xF8
#define RsvdLong9_TAG		0xF9
#define RsvdLongA_TAG		0xFA
#define RsvdLongB_TAG		0xFB
#define RsvdLongC_TAG		0xFC
#define RsvdLongD_TAG		0xFD
#define RsvdLongE_TAG		0xFE
#define RsvdLongF_TAG		0xFF
#define NewBoard_PSEUDOTAG	0x100
