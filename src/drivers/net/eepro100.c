/*
 * eepro100.c -- This file implements the eepro100 driver for etherboot.
 *
 *
 * Copyright (C) AW Computer Systems.
 * written by R.E.Wolff -- R.E.Wolff@BitWizard.nl
 *
 *
 * AW Computer Systems is contributing to the free software community
 * by paying for this driver and then putting the result under GPL.
 *
 * If you need a Linux device driver, please contact BitWizard for a
 * quote.
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *              date       version  by   what
 *  Written:    May 29 1997  V0.10  REW  Initial revision.
 * changes:     May 31 1997  V0.90  REW  Works!
 *              Jun 1  1997  V0.91  REW  Cleanup
 *              Jun 2  1997  V0.92  REW  Add some code documentation
 *              Jul 25 1997  V1.00  REW  Tested by AW to work in a PROM
 *                                       Cleanup for publication
 *              Dez 11 2004  V1.10  Kiszka  Add RX ring buffer support
 *
 * This is the etherboot intel etherexpress Pro/100B driver.
 *
 * It was written from scratch, with Donald Beckers eepro100.c kernel
 * driver as a guideline. Mostly the 82557 related definitions and the
 * lower level routines have been cut-and-pasted into this source.
 *
 * The driver was finished before Intel got the NDA out of the closet.
 * I still don't have the docs.
 *
 *
 * Datasheet is now published and available from 
 * ftp://download.intel.com/design/network/manuals/8255X_OpenSDM.pdf
 *    - Michael Brown
 * */

FILE_LICENCE ( GPL2_OR_LATER );

/* Philosophy of this driver.
 *
 * Probing:
 *
 * Using the pci.c functions of the Etherboot code, the 82557 chip is detected.
 * It is verified that the BIOS initialized everything properly and if
 * something is missing it is done now.
 *
 *
 * Initialization:
 *
 *
 * The chip is then initialized to "know" its ethernet address, and to
 * start recieving packets. The Linux driver has a whole transmit and
 * recieve ring of buffers. This is neat if you need high performance:
 * you can write the buffers asynchronously to the chip reading the
 * buffers and transmitting them over the network.  Performance is NOT
 * an issue here. We can boot a 400k kernel in about two
 * seconds. (Theory: 0.4 seconds). Booting a system is going to take
 * about half a minute anyway, so getting 10 times closer to the
 * theoretical limit is going to make a difference of a few percent. */
/* Not totally true: busy networks can cause packet drops due to RX
 * buffer overflows. Fixed in V1.10 of this driver. [Kiszka] */
/*
 *
 * Transmitting and recieving.
 *
 * We have only one transmit descriptor. It has two buffer descriptors:
 * one for the header, and the other for the data.
 * We have multiple receive buffers (currently: 4). The chip is told to
 * receive packets and suspend itself once it ran on the last free buffer.
 * The recieve (poll) routine simply looks at the current recieve buffer,
 * picks the packet if any, and releases this buffer again (classic ring
 * buffer concept). This helps to avoid packet drops on busy networks.
 *
 * Caveats:
 *
 * The Etherboot framework moves the code to the 48k segment from
 * 0x94000 to 0xa0000. There is just a little room between the end of
 * this driver and the 0xa0000 address. If you compile in too many
 * features, this will overflow.
 * The number under "hex" in the output of size that scrolls by while
 * compiling should be less than 8000. Maybe even the stack is up there,
 * so that you need even more headroom.
 */

/* The etherboot authors seem to dislike the argument ordering in
 * outb macros that Linux uses. I disklike the confusion that this
 * has caused even more.... This file uses the Linux argument ordering.  */
/* Sorry not us. It's inherited code from FreeBSD. [The authors] */

#include "etherboot.h"
#include "nic.h"
#include <gpxe/ethernet.h>
#include <gpxe/pci.h>

static int ioaddr;

enum speedo_offsets {
  SCBStatus = 0, SCBCmd = 2,      /* Rx/Command Unit command and status. */
  SCBPointer = 4,                 /* General purpose pointer. */
  SCBPort = 8,                    /* Misc. commands and operands.  */
  SCBflash = 12, SCBeeprom = 14,  /* EEPROM and flash memory control. */
  SCBCtrlMDI = 16,                /* MDI interface control. */
  SCBEarlyRx = 20,                /* Early receive byte count. */
};

enum SCBCmdBits {
	SCBMaskCmdDone=0x8000, SCBMaskRxDone=0x4000, SCBMaskCmdIdle=0x2000,
	SCBMaskRxSuspend=0x1000, SCBMaskEarlyRx=0x0800, SCBMaskFlowCtl=0x0400,
	SCBTriggerIntr=0x0200, SCBMaskAll=0x0100,
	/* The rest are Rx and Tx commands. */
	CUStart=0x0010, CUResume=0x0020, CUStatsAddr=0x0040, CUShowStats=0x0050,
	CUCmdBase=0x0060,	/* CU Base address (set to zero) . */
	CUDumpStats=0x0070, /* Dump then reset stats counters. */
	RxStart=0x0001, RxResume=0x0002, RxAbort=0x0004, RxAddrLoad=0x0006,
	RxResumeNoResources=0x0007,
};

static int do_eeprom_cmd(int cmd, int cmd_len);
void hd(void *where, int n);

/***********************************************************************/
/*                       I82557 related defines                        */
/***********************************************************************/

/* Serial EEPROM section.
   A "bit" grungy, but we work our way through bit-by-bit :->. */
/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK    0x01    /* EEPROM shift clock. */
#define EE_CS           0x02    /* EEPROM chip select. */
#define EE_DATA_WRITE   0x04    /* EEPROM chip data in. */
#define EE_DATA_READ    0x08    /* EEPROM chip data out. */
#define EE_WRITE_0      0x4802
#define EE_WRITE_1      0x4806
#define EE_ENB          (0x4800 | EE_CS)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_READ_CMD     6

/* The SCB accepts the following controls for the Tx and Rx units: */
#define  CU_START       0x0010
#define  CU_RESUME      0x0020
#define  CU_STATSADDR   0x0040
#define  CU_SHOWSTATS   0x0050  /* Dump statistics counters. */
#define  CU_CMD_BASE    0x0060  /* Base address to add to add CU commands. */
#define  CU_DUMPSTATS   0x0070  /* Dump then reset stats counters. */

#define  RX_START       0x0001
#define  RX_RESUME      0x0002
#define  RX_ABORT       0x0004
#define  RX_ADDR_LOAD   0x0006
#define  RX_RESUMENR    0x0007
#define INT_MASK        0x0100
#define DRVR_INT        0x0200          /* Driver generated interrupt. */

enum phy_chips { NonSuchPhy=0, I82553AB, I82553C, I82503, DP83840, S80C240,
                                         S80C24, PhyUndefined, DP83840A=10, };

/* Commands that can be put in a command list entry. */
enum commands {
  CmdNOp = 0,
  CmdIASetup = 1,
  CmdConfigure = 2,
  CmdMulticastList = 3,
  CmdTx = 4,
  CmdTDR = 5,
  CmdDump = 6,
  CmdDiagnose = 7,

  /* And some extra flags: */
  CmdSuspend = 0x4000,      /* Suspend after completion. */
  CmdIntr = 0x2000,         /* Interrupt after completion. */
  CmdTxFlex = 0x0008,       /* Use "Flexible mode" for CmdTx command. */
};

/* How to wait for the command unit to accept a command.
   Typically this takes 0 ticks. */
static inline void wait_for_cmd_done(int cmd_ioaddr)
{
  int wait = 0;
  int delayed_cmd;

  do
    if (inb(cmd_ioaddr) == 0) return;
  while(++wait <= 100);
  delayed_cmd = inb(cmd_ioaddr);
  do
    if (inb(cmd_ioaddr) == 0) break;
  while(++wait <= 10000);
  printf("Command %2.2x was not immediately accepted, %d ticks!\n",
      delayed_cmd, wait);
}

/* Elements of the dump_statistics block. This block must be lword aligned. */
static struct speedo_stats {
        u32 tx_good_frames;
        u32 tx_coll16_errs;
        u32 tx_late_colls;
        u32 tx_underruns;
        u32 tx_lost_carrier;
        u32 tx_deferred;
        u32 tx_one_colls;
        u32 tx_multi_colls;
        u32 tx_total_colls;
        u32 rx_good_frames;
        u32 rx_crc_errs;
        u32 rx_align_errs;
        u32 rx_resource_errs;
        u32 rx_overrun_errs;
        u32 rx_colls_errs;
        u32 rx_runt_errs;
        u32 done_marker;
} lstats;

/* A speedo3 TX buffer descriptor with two buffers... */
static struct TxFD {
	volatile s16 status;
	s16 command;
	u32 link;          /* void * */
	u32 tx_desc_addr;  /* (almost) Always points to the tx_buf_addr element. */
	s32 count;         /* # of TBD (=2), Tx start thresh., etc. */
	/* This constitutes two "TBD" entries: hdr and data */
	u32 tx_buf_addr0;  /* void *, header of frame to be transmitted.  */
	s32 tx_buf_size0;  /* Length of Tx hdr. */
	u32 tx_buf_addr1;  /* void *, data to be transmitted.  */
	s32 tx_buf_size1;  /* Length of Tx data. */
} txfd;

struct RxFD {               /* Receive frame descriptor. */
	volatile s16 status;
	s16 command;
	u32 link;                 /* struct RxFD * */
	u32 rx_buf_addr;          /* void * */
	u16 count;
	u16 size;
	char packet[1518];
};

static struct nic_operations eepro100_operations;

#define RXFD_COUNT 4
struct {
	struct RxFD rxfds[RXFD_COUNT];
} eepro100_bufs __shared;
#define rxfds eepro100_bufs.rxfds
static unsigned int rxfd = 0;

static int congenb = 0;         /* Enable congestion control in the DP83840. */
static int txfifo = 8;          /* Tx FIFO threshold in 4 byte units, 0-15 */
static int rxfifo = 8;          /* Rx FIFO threshold, default 32 bytes. */
static int txdmacount = 0;      /* Tx DMA burst length, 0-127, default 0. */
static int rxdmacount = 0;      /* Rx DMA length, 0 means no preemption. */

/* I don't understand a byte in this structure. It was copied from the
 * Linux kernel initialization for the eepro100. -- REW */
static struct ConfCmd {
  s16 status;
  s16 command;
  u32 link;
  unsigned char data[22];
} confcmd = {
  0, 0, 0, /* filled in later */
  {22, 0x08, 0, 0,  0, 0x80, 0x32, 0x03,  1, /* 1=Use MII  0=Use AUI */
   0, 0x2E, 0,  0x60, 0,
   0xf2, 0x48,   0, 0x40, 0xf2, 0x80,        /* 0x40=Force full-duplex */
   0x3f, 0x05, }
};

/***********************************************************************/
/*                       Locally used functions                        */
/***********************************************************************/

/* Support function: mdio_write
 *
 * This probably writes to the "physical media interface chip".
 * -- REW
 */

static int mdio_write(int phy_id, int location, int value)
{
	int val, boguscnt = 64*4;         /* <64 usec. to complete, typ 27 ticks */

	outl(0x04000000 | (location<<16) | (phy_id<<21) | value,
	     ioaddr + SCBCtrlMDI);
	do {
		udelay(16);

		val = inl(ioaddr + SCBCtrlMDI);
		if (--boguscnt < 0) {
			printf(" mdio_write() timed out with val = %X.\n", val);
			break;
		}
	} while (! (val & 0x10000000));
	return val & 0xffff;
}

/* Support function: mdio_read
 *
 * This probably reads a register in the "physical media interface chip".
 * -- REW
 */
static int mdio_read(int phy_id, int location)
{
	int val, boguscnt = 64*4;               /* <64 usec. to complete, typ 27 ticks */
	outl(0x08000000 | (location<<16) | (phy_id<<21), ioaddr + SCBCtrlMDI);
	do {
		udelay(16);

		val = inl(ioaddr + SCBCtrlMDI);

		if (--boguscnt < 0) {
			printf( " mdio_read() timed out with val = %X.\n", val);
			break;
		}
	} while (! (val & 0x10000000));
	return val & 0xffff;
}

/* The fixes for the code were kindly provided by Dragan Stancevic
   <visitor@valinux.com> to strictly follow Intel specifications of EEPROM
   access timing.
   The publicly available sheet 64486302 (sec. 3.1) specifies 1us access
   interval for serial EEPROM.  However, it looks like that there is an
   additional requirement dictating larger udelay's in the code below.
   2000/05/24  SAW */
static int do_eeprom_cmd(int cmd, int cmd_len)
{
	unsigned retval = 0;
	long ee_addr = ioaddr + SCBeeprom;

	outw(EE_ENB, ee_addr); udelay(2);
	outw(EE_ENB | EE_SHIFT_CLK, ee_addr); udelay(2);

	/* Shift the command bits out. */
	do {
		short dataval = (cmd & (1 << cmd_len)) ? EE_WRITE_1 : EE_WRITE_0;
		outw(dataval, ee_addr); udelay(2);
		outw(dataval | EE_SHIFT_CLK, ee_addr); udelay(2);
		retval = (retval << 1) | ((inw(ee_addr) & EE_DATA_READ) ? 1 : 0);
	} while (--cmd_len >= 0);
	outw(EE_ENB, ee_addr); udelay(2);

	/* Terminate the EEPROM access. */
	outw(EE_ENB & ~EE_CS, ee_addr);
	return retval;
}

#if 0
static inline void whereami (const char *str)
{
  printf ("%s\n", str);
  sleep (2);
}
#else
#define whereami(s)
#endif

static void eepro100_irq(struct nic *nic __unused, irq_action_t action)
{
	uint16_t enabled_mask = ( SCBMaskCmdDone | SCBMaskCmdIdle |
				  SCBMaskEarlyRx | SCBMaskFlowCtl );

	switch ( action ) {
	case DISABLE :
		outw(SCBMaskAll, ioaddr + SCBCmd);
		break;
	case ENABLE :
		outw(enabled_mask, ioaddr + SCBCmd);
		break;
	case FORCE :
		outw(enabled_mask | SCBTriggerIntr, ioaddr + SCBCmd);
		break;
	}
}

/* function: eepro100_transmit
 * This transmits a packet.
 *
 * Arguments: char d[6]:          destination ethernet address.
 *            unsigned short t:   ethernet protocol type.
 *            unsigned short s:   size of the data-part of the packet.
 *            char *p:            the data for the packet.
 * returns:   void.
 */

static void eepro100_transmit(struct nic *nic, const char *d, unsigned int t, unsigned int s, const char *p)
{
	struct eth_hdr {
		unsigned char dst_addr[ETH_ALEN];
		unsigned char src_addr[ETH_ALEN];
		unsigned short type;
	} hdr;
	unsigned short status;
	int s1, s2;
	unsigned long ct;

	status = inw(ioaddr + SCBStatus);
	/* Acknowledge all of the current interrupt sources ASAP. */
	outw(status & 0xfc00, ioaddr + SCBStatus);

#ifdef	DEBUG
	printf ("transmitting type %hX packet (%d bytes). status = %hX, cmd=%hX\n",
		t, s, status, inw (ioaddr + SCBCmd));
#endif

	memcpy (&hdr.dst_addr, d, ETH_ALEN);
	memcpy (&hdr.src_addr, nic->node_addr, ETH_ALEN);

	hdr.type = htons (t);

	txfd.status = 0;
	txfd.command = CmdSuspend | CmdTx | CmdTxFlex;
	txfd.link   = virt_to_bus (&txfd);
	txfd.count   = 0x02208000;
	txfd.tx_desc_addr = virt_to_bus(&txfd.tx_buf_addr0);

	txfd.tx_buf_addr0 = virt_to_bus (&hdr);
	txfd.tx_buf_size0 = sizeof (hdr);

	txfd.tx_buf_addr1 = virt_to_bus (p);
	txfd.tx_buf_size1 = s;

#ifdef	DEBUG
	printf ("txfd: \n");
	hd (&txfd, sizeof (txfd));
#endif

	outl(virt_to_bus(&txfd), ioaddr + SCBPointer);
	outb(CU_START, ioaddr + SCBCmd);
	wait_for_cmd_done(ioaddr + SCBCmd);

	s1 = inw (ioaddr + SCBStatus);

	ct = currticks();
	/* timeout 10 ms for transmit */
	while (!txfd.status && ct + 10*1000)
		/* Wait */;
	s2 = inw (ioaddr + SCBStatus);

#ifdef	DEBUG
	printf ("s1 = %hX, s2 = %hX.\n", s1, s2);
#endif
}

/*
 * Sometimes the receiver stops making progress.  This routine knows how to
 * get it going again, without losing packets or being otherwise nasty like
 * a chip reset would be.  Previously the driver had a whole sequence
 * of if RxSuspended, if it's no buffers do one thing, if it's no resources,
 * do another, etc.  But those things don't really matter.  Separate logic
 * in the ISR provides for allocating buffers--the other half of operation
 * is just making sure the receiver is active.  speedo_rx_soft_reset does that.
 * This problem with the old, more involved algorithm is shown up under
 * ping floods on the order of 60K packets/second on a 100Mbps fdx network.
 */
static void
speedo_rx_soft_reset(void)
{
	int i;


#ifdef	DEBUG
	printf("reset\n");
#endif
	wait_for_cmd_done(ioaddr + SCBCmd);
	/*
	 * Put the hardware into a known state.
	 */
	outb(RX_ABORT, ioaddr + SCBCmd);

	for (i = 0; i < RXFD_COUNT; i++) {
		rxfds[i].status      = 0;
		rxfds[i].rx_buf_addr = 0xffffffff;
		rxfds[i].count       = 0;
		rxfds[i].size        = 1528;
	}

	wait_for_cmd_done(ioaddr + SCBCmd);

	outl(virt_to_bus(&rxfds[rxfd]), ioaddr + SCBPointer);
	outb(RX_START, ioaddr + SCBCmd);
}

/* function: eepro100_poll / eth_poll
 * This receives a packet from the network.
 *
 * Arguments: none
 *
 * returns:   1 if a packet was received.
 *            0 if no packet was received.
 * side effects:
 *            returns the packet in the array nic->packet.
 *            returns the length of the packet in nic->packetlen.
 */

static int eepro100_poll(struct nic *nic, int retrieve)
{
	if (rxfds[rxfd].status) {
		if (!retrieve)
			return 1;
#ifdef	DEBUG
		printf("Got a packet: Len = %d, rxfd = %d.\n",
		       rxfds[rxfd].count & 0x3fff, rxfd);
#endif
		/* First save the data from the rxfd */
		nic->packetlen = rxfds[rxfd].count & 0x3fff;
		memcpy(nic->packet, rxfds[rxfd].packet, nic->packetlen);

		rxfds[rxfd].status      = 0;
		rxfds[rxfd].command     = 0xc000;
		rxfds[rxfd].rx_buf_addr = 0xFFFFFFFF;
		rxfds[rxfd].count       = 0;
		rxfds[rxfd].size        = 1528;
		rxfds[(rxfd-1) % RXFD_COUNT].command = 0x0000;
		rxfd = (rxfd+1) % RXFD_COUNT;

#ifdef	DEBUG
		hd (nic->packet, 0x30);
#endif

		/* Acknowledge all conceivable interrupts */
		outw(0xff00, ioaddr + SCBStatus);

		return 1;
	}

	/*
	 * The chip may have suspended reception for various reasons.
	 * Check for that, and re-prime it should this be the case.
	 */
	switch ((inw(ioaddr + SCBStatus) >> 2) & 0xf) {
		case 0:  /* Idle */
			break;
		case 1:  /* Suspended */
		case 2:  /* No resources (RxFDs) */
		case 9:  /* Suspended with no more RBDs */
		case 10: /* No resources due to no RBDs */
		case 12: /* Ready with no RBDs */
			speedo_rx_soft_reset();
			break;
		default:
			/* reserved values */
			break;
	}
	return 0;
}

/* function: eepro100_disable
 * resets the card. This is used to allow Etherboot or Linux
 * to probe the card again from a "virginal" state....
 * Arguments: none
 *
 * returns:   void.
 */
static void eepro100_disable ( struct nic *nic __unused ) {
/* from eepro100_reset */
	outl(0, ioaddr + SCBPort);
/* from eepro100_disable */
	/* See if this PartialReset solves the problem with interfering with
	   kernel operation after Etherboot hands over. - Ken 20001102 */
	outl(2, ioaddr + SCBPort);

	/* The following is from the Intel e100 driver.
	 * This hopefully solves the problem with hanging hard DOS images. */

	/* wait for the reset to take effect */
	udelay(20);

	/* Mask off our interrupt line -- it is unmasked after reset */
	{
		u16 intr_status;
		/* Disable interrupts on our PCI board by setting the mask bit */
		outw(INT_MASK, ioaddr + SCBCmd);
		intr_status = inw(ioaddr + SCBStatus);
		/* ack and clear intrs */
		outw(intr_status, ioaddr + SCBStatus);
		inw(ioaddr + SCBStatus);
	}
}

/* exported function: eepro100_probe / eth_probe
 * initializes a card
 *
 * side effects:
 *            leaves the ioaddress of the 82557 chip in the variable ioaddr.
 *            leaves the 82557 initialized, and ready to recieve packets.
 */

static int eepro100_probe ( struct nic *nic, struct pci_device *pci ) {

	unsigned short sum = 0;
	int i;
	int read_cmd, ee_size;
	int options;
	int rx_mode;
	unsigned long ct;

	/* we cache only the first few words of the EEPROM data
	   be careful not to access beyond this array */
	unsigned short eeprom[16];

	if (pci->ioaddr == 0)
		return 0;

	adjust_pci_device(pci);

        nic->ioaddr = pci->ioaddr;
        nic->irqno = pci->irq;

	ioaddr = nic->ioaddr;

	if ((do_eeprom_cmd(EE_READ_CMD << 24, 27) & 0xffe0000)
		== 0xffe0000) {
		ee_size = 0x100;
		read_cmd = EE_READ_CMD << 24;
	} else {
		ee_size = 0x40;
		read_cmd = EE_READ_CMD << 22;
	}

	for (i = 0, sum = 0; i < ee_size; i++) {
		unsigned short value = do_eeprom_cmd(read_cmd | (i << 16), 27);
		if (i < (int)(sizeof(eeprom)/sizeof(eeprom[0])))
			eeprom[i] = value;
		sum += value;
	}

	for (i=0;i<ETH_ALEN;i++) {
		nic->node_addr[i] =  (eeprom[i/2] >> (8*(i&1))) & 0xff;
	}

	DBG ( "Ethernet addr: %s\n", eth_ntoa ( nic->node_addr ) );

	if (sum != 0xBABA)
		printf("eepro100: Invalid EEPROM checksum %#hX, "
		       "check settings before activating this device!\n", sum);
	outl(0, ioaddr + SCBPort);
	udelay (10000);
	whereami ("Got eeprom.");

	/* Base = 0, disable all interrupts  */
	outl(0, ioaddr + SCBPointer);
	outw(INT_MASK | RX_ADDR_LOAD, ioaddr + SCBCmd);
	wait_for_cmd_done(ioaddr + SCBCmd);
	whereami ("set rx base addr.");

	outl(virt_to_bus(&lstats), ioaddr + SCBPointer);
	outb(CU_STATSADDR, ioaddr + SCBCmd);
	wait_for_cmd_done(ioaddr + SCBCmd);
	whereami ("set stats addr.");

	/* INIT RX stuff. */
	for (i = 0; i < RXFD_COUNT; i++) {
		rxfds[i].status      = 0x0000;
		rxfds[i].command     = 0x0000;
		rxfds[i].rx_buf_addr = 0xFFFFFFFF;
		rxfds[i].count       = 0;
		rxfds[i].size        = 1528;
		rxfds[i].link        = virt_to_bus(&rxfds[i+1]);
	}

	rxfds[RXFD_COUNT-1].status  = 0x0000;
	rxfds[RXFD_COUNT-1].command = 0xC000;
	rxfds[RXFD_COUNT-1].link    = virt_to_bus(&rxfds[0]);

	outl(virt_to_bus(&rxfds[0]), ioaddr + SCBPointer);
	outb(RX_START, ioaddr + SCBCmd);
	wait_for_cmd_done(ioaddr + SCBCmd);

	whereami ("started RX process.");

	/* INIT TX stuff. */

	/* Base = 0 */
	outl(0, ioaddr + SCBPointer);
	outb(CU_CMD_BASE, ioaddr + SCBCmd);
	wait_for_cmd_done(ioaddr + SCBCmd);

	whereami ("set TX base addr.");

	txfd.command      = (CmdIASetup);
	txfd.status       = 0x0000;
	txfd.link         = virt_to_bus (&confcmd);

	{
		char *t = (char *)&txfd.tx_desc_addr;

		for (i=0;i<ETH_ALEN;i++)
			t[i] = nic->node_addr[i];
	}

#ifdef	DEBUG
	printf ("Setup_eaddr:\n");
	hd (&txfd, 0x20);
#endif
	/*      options = 0x40; */ /* 10mbps half duplex... */
	options = 0x00;            /* Autosense */

#ifdef PROMISC
	rx_mode = 3;
#elif ALLMULTI
	rx_mode = 1;
#else
	rx_mode = 0;
#endif

	if (   ((eeprom[6]>>8) & 0x3f) == DP83840
	       || ((eeprom[6]>>8) & 0x3f) == DP83840A) {
		int mdi_reg23 = mdio_read(eeprom[6] & 0x1f, 23) | 0x0422;
		if (congenb)
			mdi_reg23 |= 0x0100;
		printf("  DP83840 specific setup, setting register 23 to %hX.\n",
		       mdi_reg23);
		mdio_write(eeprom[6] & 0x1f, 23, mdi_reg23);
	}
	whereami ("Done DP8340 special setup.");
	if (options != 0) {
		mdio_write(eeprom[6] & 0x1f, 0,
			   ((options & 0x20) ? 0x2000 : 0) |    /* 100mbps? */
			   ((options & 0x10) ? 0x0100 : 0)); /* Full duplex? */
		whereami ("set mdio_register.");
	}

	confcmd.command  = CmdSuspend | CmdConfigure;
	confcmd.status   = 0x0000;
	confcmd.link     = virt_to_bus (&txfd);
	confcmd.data[1]  = (txfifo << 4) | rxfifo;
	confcmd.data[4]  = rxdmacount;
	confcmd.data[5]  = txdmacount + 0x80;
	confcmd.data[15] = (rx_mode & 2) ? 0x49: 0x48;
	confcmd.data[19] = (options & 0x10) ? 0xC0 : 0x80;
	confcmd.data[21] = (rx_mode & 1) ? 0x0D: 0x05;

	outl(virt_to_bus(&txfd), ioaddr + SCBPointer);
	outb(CU_START, ioaddr + SCBCmd);
	wait_for_cmd_done(ioaddr + SCBCmd);

	whereami ("started TX thingy (config, iasetup).");

	ct = currticks();
	while (!txfd.status && ct + 10*1000 < currticks())
		/* Wait */;

	/* Read the status register once to disgard stale data */
	mdio_read(eeprom[6] & 0x1f, 1);
	/* Check to see if the network cable is plugged in.
	 * This allows for faster failure if there is nothing
	 * we can do.
	 */
	if (!(mdio_read(eeprom[6] & 0x1f, 1) & (1 << 2))) {
		printf("Valid link not established\n");
		eepro100_disable(nic);
		return 0;
	}
	nic->nic_op	= &eepro100_operations;
	return 1;
}

/*********************************************************************/

#ifdef	DEBUG

/* Hexdump a number of bytes from memory... */
void hd (void *where, int n)
{
	int i;

	while (n > 0) {
		printf ("%X ", where);
		for (i=0;i < ( (n>16)?16:n);i++)
			printf (" %hhX", ((char *)where)[i]);
		printf ("\n");
		n -= 16;
		where += 16;
	}
}
#endif

static struct nic_operations eepro100_operations = {
	.connect	= dummy_connect,
	.poll		= eepro100_poll,
	.transmit	= eepro100_transmit,
	.irq		= eepro100_irq,

};

static struct pci_device_id eepro100_nics[] = {
PCI_ROM(0x8086, 0x1029, "id1029",        "Intel EtherExpressPro100 ID1029", 0),
PCI_ROM(0x8086, 0x1030, "id1030",        "Intel EtherExpressPro100 ID1030", 0),
PCI_ROM(0x8086, 0x1031, "82801cam",      "Intel 82801CAM (ICH3) Chipset Ethernet Controller", 0),
PCI_ROM(0x8086, 0x1032, "eepro100-1032", "Intel PRO/100 VE Network Connection", 0),
PCI_ROM(0x8086, 0x1033, "eepro100-1033", "Intel PRO/100 VM Network Connection", 0),
PCI_ROM(0x8086, 0x1034, "eepro100-1034", "Intel PRO/100 VM Network Connection", 0),
PCI_ROM(0x8086, 0x1035, "eepro100-1035", "Intel 82801CAM (ICH3) Chipset Ethernet Controller", 0),
PCI_ROM(0x8086, 0x1036, "eepro100-1036", "Intel 82801CAM (ICH3) Chipset Ethernet Controller", 0),
PCI_ROM(0x8086, 0x1037, "eepro100-1037", "Intel 82801CAM (ICH3) Chipset Ethernet Controller", 0),
PCI_ROM(0x8086, 0x1038, "id1038",        "Intel PRO/100 VM Network Connection", 0),
PCI_ROM(0x8086, 0x1039, "82562et",       "Intel PRO100 VE 82562ET", 0),
PCI_ROM(0x8086, 0x103a, "id103a",        "Intel Corporation 82559 InBusiness 10/100", 0),
PCI_ROM(0x8086, 0x103b, "82562etb",      "Intel PRO100 VE 82562ETB", 0),
PCI_ROM(0x8086, 0x103c, "eepro100-103c", "Intel PRO/100 VM Network Connection", 0),
PCI_ROM(0x8086, 0x103d, "eepro100-103d", "Intel PRO/100 VE Network Connection", 0),
PCI_ROM(0x8086, 0x103e, "eepro100-103e", "Intel PRO/100 VM Network Connection", 0),
PCI_ROM(0x8086, 0x1051, "prove",       "Intel PRO/100 VE Network Connection", 0),
PCI_ROM(0x8086, 0x1059, "82551qm",       "Intel PRO/100 M Mobile Connection", 0),
PCI_ROM(0x8086, 0x1209, "82559er",       "Intel EtherExpressPro100 82559ER", 0),
PCI_ROM(0x8086, 0x1227, "82865",         "Intel 82865 EtherExpress PRO/100A", 0),
PCI_ROM(0x8086, 0x1228, "82556",         "Intel 82556 EtherExpress PRO/100 Smart", 0),
PCI_ROM(0x8086, 0x1229, "eepro100",      "Intel EtherExpressPro100", 0),
PCI_ROM(0x8086, 0x2449, "82562em",       "Intel EtherExpressPro100 82562EM", 0),
PCI_ROM(0x8086, 0x2459, "82562-1",       "Intel 82562 based Fast Ethernet Connection", 0),
PCI_ROM(0x8086, 0x245d, "82562-2",       "Intel 82562 based Fast Ethernet Connection", 0),
PCI_ROM(0x8086, 0x1050, "82562ez",       "Intel 82562EZ Network Connection", 0),
PCI_ROM(0x8086, 0x1051, "eepro100-1051", "Intel 82801EB/ER (ICH5/ICH5R) Chipset Ethernet Controller", 0),
PCI_ROM(0x8086, 0x1065, "82562-3",       "Intel 82562 based Fast Ethernet Connection", 0),
PCI_ROM(0x8086, 0x5200, "eepro100-5200", "Intel EtherExpress PRO/100 Intelligent Server", 0),
PCI_ROM(0x8086, 0x5201, "eepro100-5201", "Intel EtherExpress PRO/100 Intelligent Server", 0),
};

/* Cards with device ids 0x1030 to 0x103F, 0x2449, 0x2459 or 0x245D might need
 * a workaround for hardware bug on 10 mbit half duplex (see linux driver eepro100.c)
 * 2003/03/17 gbaum */


PCI_DRIVER ( eepro100_driver, eepro100_nics, PCI_NO_CLASS );

DRIVER ( "EEPRO100", nic_driver, pci_driver, eepro100_driver,
	 eepro100_probe, eepro100_disable );

/*
 * Local variables:
 *  c-basic-offset: 8
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
