/**************************************************************************
*
*    pcnet32.c -- Etherboot device driver for the AMD PCnet32
*    Written 2003-2003 by Timothy Legge <tlegge@rogers.com>
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
*    Portions of this code based on:
*		pcnet32.c: An AMD PCnet32 ethernet driver for linux:
*
*	(C) 1996-1999 Thomas Bogendoerfer
*		See Linux Driver for full information
*	
*	The transmit and poll functions were written with reference to:
*	lance.c - LANCE NIC driver for Etherboot written by Ken Yap 
*	
*	Linux Driver Version 1.27a, 10.02.2002
* 
* 
*    REVISION HISTORY:
*    ================
*    v1.0	08-06-2003	timlegge	Initial port of Linux driver
*    v1.1	08-23-2003	timlegge	Add multicast support
*    v1.2	01-17-2004	timlegge	Initial driver output cleanup
*    v1.3	03-29-2004	timlegge	More driver cleanup
*    
*    Indent Options: indent -kr -i8
***************************************************************************/

FILE_LICENCE ( GPL2_OR_LATER );

#include "etherboot.h"
#include "nic.h"
#include <gpxe/pci.h>
#include <gpxe/ethernet.h>
#include "mii.h"

/* void hex_dump(const char *data, const unsigned int len); */

/* Etherboot Specific definations */
#define drv_version "v1.3"
#define drv_date "03-29-2004"

static u32 ioaddr;		/* Globally used for the card's io address */
static struct nic_operations pcnet32_operations;

#ifdef EDEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

/* Condensed operations for readability. */
#define virt_to_le32desc(addr)  cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)  bus_to_virt(le32_to_cpu(addr))

/* End Etherboot Specific */

static int cards_found = 0 /* __initdata */ ;

#ifdef REMOVE
/* FIXME: Remove these they are probably pointless */

/* 
 * VLB I/O addresses 
 */
static unsigned int pcnet32_portlist[] /*__initdata */  =
{ 0x300, 0x320, 0x340, 0x360, 0 };

static int pcnet32_debug = 1;
static int tx_start = 1;	/* Mapping -- 0:20, 1:64, 2:128, 3:~220 (depends on chip vers) */
static int pcnet32vlb;		/* check for VLB cards ? */

static struct net_device *pcnet32_dev;

static int max_interrupt_work = 80;
static int rx_copybreak = 200;
#endif
#define PCNET32_PORT_AUI      0x00
#define PCNET32_PORT_10BT     0x01
#define PCNET32_PORT_GPSI     0x02
#define PCNET32_PORT_MII      0x03

#define PCNET32_PORT_PORTSEL  0x03
#define PCNET32_PORT_ASEL     0x04
#define PCNET32_PORT_100      0x40
#define PCNET32_PORT_FD	      0x80

#define PCNET32_DMA_MASK 0xffffffff

/*
 * table to translate option values from tulip
 * to internal options
 */
static unsigned char options_mapping[] = {
	PCNET32_PORT_ASEL,	/*  0 Auto-select      */
	PCNET32_PORT_AUI,	/*  1 BNC/AUI          */
	PCNET32_PORT_AUI,	/*  2 AUI/BNC          */
	PCNET32_PORT_ASEL,	/*  3 not supported    */
	PCNET32_PORT_10BT | PCNET32_PORT_FD,	/*  4 10baseT-FD       */
	PCNET32_PORT_ASEL,	/*  5 not supported    */
	PCNET32_PORT_ASEL,	/*  6 not supported    */
	PCNET32_PORT_ASEL,	/*  7 not supported    */
	PCNET32_PORT_ASEL,	/*  8 not supported    */
	PCNET32_PORT_MII,	/*  9 MII 10baseT      */
	PCNET32_PORT_MII | PCNET32_PORT_FD,	/* 10 MII 10baseT-FD   */
	PCNET32_PORT_MII,	/* 11 MII (autosel)    */
	PCNET32_PORT_10BT,	/* 12 10BaseT          */
	PCNET32_PORT_MII | PCNET32_PORT_100,	/* 13 MII 100BaseTx    */
	PCNET32_PORT_MII | PCNET32_PORT_100 | PCNET32_PORT_FD,	/* 14 MII 100BaseTx-FD */
	PCNET32_PORT_ASEL	/* 15 not supported    */
};

#define MAX_UNITS 8		/* More are supported, limit only on options */
static int options[MAX_UNITS];
static int full_duplex[MAX_UNITS];

/*
 *				Theory of Operation
 * 
 * This driver uses the same software structure as the normal lance
 * driver. So look for a verbose description in lance.c. The differences
 * to the normal lance driver is the use of the 32bit mode of PCnet32
 * and PCnetPCI chips. Because these chips are 32bit chips, there is no
 * 16MB limitation and we don't need bounce buffers.
 */



/*
 * Set the number of Tx and Rx buffers, using Log_2(# buffers).
 * Reasonable default values are 4 Tx buffers, and 16 Rx buffers.
 * That translates to 2 (4 == 2^^2) and 4 (16 == 2^^4).
 */
#ifndef PCNET32_LOG_TX_BUFFERS
#define PCNET32_LOG_TX_BUFFERS 1
#define PCNET32_LOG_RX_BUFFERS 2
#endif

#define TX_RING_SIZE		(1 << (PCNET32_LOG_TX_BUFFERS))
#define TX_RING_MOD_MASK	(TX_RING_SIZE - 1)
/* FIXME: Fix this to allow multiple tx_ring descriptors */
#define TX_RING_LEN_BITS	0x0000	/*PCNET32_LOG_TX_BUFFERS) << 12) */

#define RX_RING_SIZE		(1 << (PCNET32_LOG_RX_BUFFERS))
#define RX_RING_MOD_MASK	(RX_RING_SIZE - 1)
#define RX_RING_LEN_BITS	((PCNET32_LOG_RX_BUFFERS) << 4)

#define PKT_BUF_SZ		1544

/* Offsets from base I/O address. */
#define PCNET32_WIO_RDP		0x10
#define PCNET32_WIO_RAP		0x12
#define PCNET32_WIO_RESET	0x14
#define PCNET32_WIO_BDP		0x16

#define PCNET32_DWIO_RDP	0x10
#define PCNET32_DWIO_RAP	0x14
#define PCNET32_DWIO_RESET	0x18
#define PCNET32_DWIO_BDP	0x1C

#define PCNET32_TOTAL_SIZE	0x20

/* The PCNET32 Rx and Tx ring descriptors. */
struct pcnet32_rx_head {
	u32 base;
	s16 buf_length;
	s16 status;
	u32 msg_length;
	u32 reserved;
};

struct pcnet32_tx_head {
	u32 base;
	s16 length;
	s16 status;
	u32 misc;
	u32 reserved;
};

/* The PCNET32 32-Bit initialization block, described in databook. */
struct pcnet32_init_block {
	u16 mode;
	u16 tlen_rlen;
	u8 phys_addr[6];
	u16 reserved;
	u32 filter[2];
	/* Receive and transmit ring base, along with extra bits. */
	u32 rx_ring;
	u32 tx_ring;
};
/* PCnet32 access functions */
struct pcnet32_access {
	u16(*read_csr) (unsigned long, int);
	void (*write_csr) (unsigned long, int, u16);
	 u16(*read_bcr) (unsigned long, int);
	void (*write_bcr) (unsigned long, int, u16);
	 u16(*read_rap) (unsigned long);
	void (*write_rap) (unsigned long, u16);
	void (*reset) (unsigned long);
};

/* Define the TX and RX Descriptors and Rings */
struct {
	struct pcnet32_tx_head tx_ring[TX_RING_SIZE]
	__attribute__ ((aligned(16)));
	struct pcnet32_rx_head rx_ring[RX_RING_SIZE]
	__attribute__ ((aligned(16)));
	unsigned char txb[TX_RING_SIZE][PKT_BUF_SZ];
	unsigned char rxb[RX_RING_SIZE][PKT_BUF_SZ];
} pcnet32_bufs __shared;


/*
 * The first three fields of pcnet32_private are read by the ethernet device 
 * so we allocate the structure should be allocated by pci_alloc_consistent().
 */
#define MII_CNT 4
struct pcnet32_private {
	struct pcnet32_init_block init_block;
	struct pci_dev *pci_dev;	/* Pointer to the associated pci device structure */
	const char *name;
	/* The saved address of a sent-in-place packet/buffer, for skfree(). */
	struct sk_buff *tx_skbuff[TX_RING_SIZE];
	struct sk_buff *rx_skbuff[RX_RING_SIZE];
	struct pcnet32_access a;
	unsigned int cur_rx, cur_tx;	/* The next free ring entry */
	char tx_full;
	int options;
	int shared_irq:1,	/* shared irq possible */
	 ltint:1,		/* enable TxDone-intr inhibitor */
	 dxsuflo:1,		/* disable transmit stop on uflo */
	 mii:1;			/* mii port available */
	struct mii_if_info mii_if;
	unsigned char phys[MII_CNT];
	struct net_device *next;
	int full_duplex:1;
} lpx;

static struct pcnet32_private *lp;

static int mdio_read(struct nic *nic __unused, int phy_id, int reg_num);
#if 0
static void mdio_write(struct nic *nic __unused, int phy_id, int reg_num,
		       int val);
#endif
enum pci_flags_bit {
	PCI_USES_IO = 1, PCI_USES_MEM = 2, PCI_USES_MASTER = 4,
	PCI_ADDR0 = 0x10 << 0, PCI_ADDR1 = 0x10 << 1, PCI_ADDR2 =
	    0x10 << 2, PCI_ADDR3 = 0x10 << 3,
};


static u16 pcnet32_wio_read_csr(unsigned long addr, int index)
{
	outw(index, addr + PCNET32_WIO_RAP);
	return inw(addr + PCNET32_WIO_RDP);
}

static void pcnet32_wio_write_csr(unsigned long addr, int index, u16 val)
{
	outw(index, addr + PCNET32_WIO_RAP);
	outw(val, addr + PCNET32_WIO_RDP);
}

static u16 pcnet32_wio_read_bcr(unsigned long addr, int index)
{
	outw(index, addr + PCNET32_WIO_RAP);
	return inw(addr + PCNET32_WIO_BDP);
}

static void pcnet32_wio_write_bcr(unsigned long addr, int index, u16 val)
{
	outw(index, addr + PCNET32_WIO_RAP);
	outw(val, addr + PCNET32_WIO_BDP);
}

static u16 pcnet32_wio_read_rap(unsigned long addr)
{
	return inw(addr + PCNET32_WIO_RAP);
}

static void pcnet32_wio_write_rap(unsigned long addr, u16 val)
{
	outw(val, addr + PCNET32_WIO_RAP);
}

static void pcnet32_wio_reset(unsigned long addr)
{
	inw(addr + PCNET32_WIO_RESET);
}

static int pcnet32_wio_check(unsigned long addr)
{
	outw(88, addr + PCNET32_WIO_RAP);
	return (inw(addr + PCNET32_WIO_RAP) == 88);
}

static struct pcnet32_access pcnet32_wio = {
      read_csr:pcnet32_wio_read_csr,
      write_csr:pcnet32_wio_write_csr,
      read_bcr:pcnet32_wio_read_bcr,
      write_bcr:pcnet32_wio_write_bcr,
      read_rap:pcnet32_wio_read_rap,
      write_rap:pcnet32_wio_write_rap,
      reset:pcnet32_wio_reset
};

static u16 pcnet32_dwio_read_csr(unsigned long addr, int index)
{
	outl(index, addr + PCNET32_DWIO_RAP);
	return (inl(addr + PCNET32_DWIO_RDP) & 0xffff);
}

static void pcnet32_dwio_write_csr(unsigned long addr, int index, u16 val)
{
	outl(index, addr + PCNET32_DWIO_RAP);
	outl(val, addr + PCNET32_DWIO_RDP);
}

static u16 pcnet32_dwio_read_bcr(unsigned long addr, int index)
{
	outl(index, addr + PCNET32_DWIO_RAP);
	return (inl(addr + PCNET32_DWIO_BDP) & 0xffff);
}

static void pcnet32_dwio_write_bcr(unsigned long addr, int index, u16 val)
{
	outl(index, addr + PCNET32_DWIO_RAP);
	outl(val, addr + PCNET32_DWIO_BDP);
}

static u16 pcnet32_dwio_read_rap(unsigned long addr)
{
	return (inl(addr + PCNET32_DWIO_RAP) & 0xffff);
}

static void pcnet32_dwio_write_rap(unsigned long addr, u16 val)
{
	outl(val, addr + PCNET32_DWIO_RAP);
}

static void pcnet32_dwio_reset(unsigned long addr)
{
	inl(addr + PCNET32_DWIO_RESET);
}

static int pcnet32_dwio_check(unsigned long addr)
{
	outl(88, addr + PCNET32_DWIO_RAP);
	return ((inl(addr + PCNET32_DWIO_RAP) & 0xffff) == 88);
}

static struct pcnet32_access pcnet32_dwio = {
      read_csr:pcnet32_dwio_read_csr,
      write_csr:pcnet32_dwio_write_csr,
      read_bcr:pcnet32_dwio_read_bcr,
      write_bcr:pcnet32_dwio_write_bcr,
      read_rap:pcnet32_dwio_read_rap,
      write_rap:pcnet32_dwio_write_rap,
      reset:pcnet32_dwio_reset
};


/* Initialize the PCNET32 Rx and Tx rings. */
static int pcnet32_init_ring(struct nic *nic)
{
	int i;

	lp->tx_full = 0;
	lp->cur_rx = lp->cur_tx = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		pcnet32_bufs.rx_ring[i].base =
			virt_to_le32desc(&pcnet32_bufs.rxb[i]);
		pcnet32_bufs.rx_ring[i].buf_length = le16_to_cpu(-PKT_BUF_SZ);
		pcnet32_bufs.rx_ring[i].status = le16_to_cpu(0x8000);
	}

	/* The Tx buffer address is filled in as needed, but we do need to clear
	   the upper ownership bit. */
	for (i = 0; i < TX_RING_SIZE; i++) {
		pcnet32_bufs.tx_ring[i].base = 0;
		pcnet32_bufs.tx_ring[i].status = 0;
	}


	lp->init_block.tlen_rlen =
	    le16_to_cpu(TX_RING_LEN_BITS | RX_RING_LEN_BITS);
	for (i = 0; i < 6; i++)
		lp->init_block.phys_addr[i] = nic->node_addr[i];
	lp->init_block.rx_ring = virt_to_le32desc(&pcnet32_bufs.rx_ring[0]);
	lp->init_block.tx_ring = virt_to_le32desc(&pcnet32_bufs.tx_ring[0]);
	return 0;
}

/**************************************************************************
RESET - Reset adapter
***************************************************************************/
static void pcnet32_reset(struct nic *nic)
{
	/* put the card in its initial state */
	u16 val;
	int i;

	/* Reset the PCNET32 */
	lp->a.reset(ioaddr);

	/* switch pcnet32 to 32bit mode */
	lp->a.write_bcr(ioaddr, 20, 2);

	/* set/reset autoselect bit */
	val = lp->a.read_bcr(ioaddr, 2) & ~2;
	if (lp->options & PCNET32_PORT_ASEL)
		val |= 2;
	lp->a.write_bcr(ioaddr, 2, val);

	/* handle full duplex setting */
	if (lp->full_duplex) {
		val = lp->a.read_bcr(ioaddr, 9) & ~3;
		if (lp->options & PCNET32_PORT_FD) {
			val |= 1;
			if (lp->options ==
			    (PCNET32_PORT_FD | PCNET32_PORT_AUI))
				val |= 2;
		} else if (lp->options & PCNET32_PORT_ASEL) {
			/* workaround of xSeries250, turn on for 79C975 only */
			i = ((lp->a.
			      read_csr(ioaddr,
				       88) | (lp->a.read_csr(ioaddr,
							     89) << 16)) >>
			     12) & 0xffff;
			if (i == 0x2627)
				val |= 3;
		}
		lp->a.write_bcr(ioaddr, 9, val);
	}

	/* set/reset GPSI bit in test register */
	val = lp->a.read_csr(ioaddr, 124) & ~0x10;
	if ((lp->options & PCNET32_PORT_PORTSEL) == PCNET32_PORT_GPSI)
		val |= 0x10;
	lp->a.write_csr(ioaddr, 124, val);

	if (lp->mii && !(lp->options & PCNET32_PORT_ASEL)) {
		val = lp->a.read_bcr(ioaddr, 32) & ~0x38;	/* disable Auto Negotiation, set 10Mpbs, HD */
		if (lp->options & PCNET32_PORT_FD)
			val |= 0x10;
		if (lp->options & PCNET32_PORT_100)
			val |= 0x08;
		lp->a.write_bcr(ioaddr, 32, val);
	} else {
		if (lp->options & PCNET32_PORT_ASEL) {	/* enable auto negotiate, setup, disable fd */
			val = lp->a.read_bcr(ioaddr, 32) & ~0x98;
			val |= 0x20;
			lp->a.write_bcr(ioaddr, 32, val);
		}
	}

#ifdef DO_DXSUFLO
	if (lp->dxsuflo) {	/* Disable transmit stop on underflow */
		val = lp->a.read_csr(ioaddr, 3);
		val |= 0x40;
		lp->a.write_csr(ioaddr, 3, val);
	}
#endif
	if (1)
	{
		//disable interrupts
		val = lp->a.read_csr(ioaddr, 3);
		val = val
			| (1 << 14) //BABLM intr disabled
			| (1 << 12) //MISSM missed frame mask intr disabled
			| (1 << 10) //RINTM receive intr disabled
			| (1 << 9) //TINTM transmit intr disabled
			| (1 << 8) //IDONM init done intr disabled
			;
		lp->a.write_csr(ioaddr, 3, val);
	}

	if (lp->ltint) {	/* Enable TxDone-intr inhibitor */
		val = lp->a.read_csr(ioaddr, 5);
		val |= (1 << 14);
		lp->a.write_csr(ioaddr, 5, val);
	}
	lp->init_block.mode =
	    le16_to_cpu((lp->options & PCNET32_PORT_PORTSEL) << 7);
	lp->init_block.filter[0] = 0xffffffff;
	lp->init_block.filter[1] = 0xffffffff;

	pcnet32_init_ring(nic);


	/* Re-initialize the PCNET32, and start it when done. */
	lp->a.write_csr(ioaddr, 1,
			(virt_to_bus(&lp->init_block)) & 0xffff);
	lp->a.write_csr(ioaddr, 2, (virt_to_bus(&lp->init_block)) >> 16);
	lp->a.write_csr(ioaddr, 4, 0x0915);
	lp->a.write_csr(ioaddr, 0, 0x0001);


	i = 0;
	while (i++ < 100)
		if (lp->a.read_csr(ioaddr, 0) & 0x0100)
			break;
	/* 
	 * We used to clear the InitDone bit, 0x0100, here but Mark Stockton
	 * reports that doing so triggers a bug in the '974.
	 */
	lp->a.write_csr(ioaddr, 0, 0x0042);

	dprintf(("pcnet32 open, csr0 %hX.\n", lp->a.read_csr(ioaddr, 0)));

}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int pcnet32_poll(struct nic *nic __unused, int retrieve)
{
	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */

	signed char status;
	int entry;

	entry = lp->cur_rx & RX_RING_MOD_MASK;
	status = (le16_to_cpu(pcnet32_bufs.rx_ring[entry].status) >> 8);

	if (status < 0)
		return 0;

	if ( ! retrieve ) return 1;

	if (status == 0x03) {
		nic->packetlen =
			(le32_to_cpu(pcnet32_bufs.rx_ring[entry].msg_length)
			 & 0xfff) - 4;
		memcpy(nic->packet, &pcnet32_bufs.rxb[entry], nic->packetlen);

		/* Andrew Boyd of QNX reports that some revs of the 79C765
		 * clear the buffer length */
		pcnet32_bufs.rx_ring[entry].buf_length
			= le16_to_cpu(-PKT_BUF_SZ);
		/* prime for next receive */
		pcnet32_bufs.rx_ring[entry].status |= le16_to_cpu(0x8000);
		/* Switch to the next Rx ring buffer */
		lp->cur_rx++;

	} else {
		return 0;
	}

	return 1;
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void pcnet32_transmit(struct nic *nic __unused, const char *d,	/* Destination */
			     unsigned int t,	/* Type */
			     unsigned int s,	/* size */
			     const char *p)
{				/* Packet */
	/* send the packet to destination */
	unsigned long time;
	u8 *ptxb;
	u16 nstype;
	u16 status;
	int entry = 0;		/*lp->cur_tx & TX_RING_MOD_MASK; */

	status = 0x8300;
	/* point to the current txb incase multiple tx_rings are used */
	ptxb = pcnet32_bufs.txb[lp->cur_tx];

	/* copy the packet to ring buffer */
	memcpy(ptxb, d, ETH_ALEN);	/* dst */
	memcpy(ptxb + ETH_ALEN, nic->node_addr, ETH_ALEN);	/* src */
	nstype = htons((u16) t);	/* type */
	memcpy(ptxb + 2 * ETH_ALEN, (u8 *) & nstype, 2);	/* type */
	memcpy(ptxb + ETH_HLEN, p, s);

	s += ETH_HLEN;
	while (s < ETH_ZLEN)	/* pad to min length */
		ptxb[s++] = '\0';

	pcnet32_bufs.tx_ring[entry].length = le16_to_cpu(-s);
	pcnet32_bufs.tx_ring[entry].misc = 0x00000000;
	pcnet32_bufs.tx_ring[entry].base = (u32) virt_to_le32desc(ptxb);

	/* we set the top byte as the very last thing */
	pcnet32_bufs.tx_ring[entry].status = le16_to_cpu(status);


	/* Trigger an immediate send poll */
	lp->a.write_csr(ioaddr, 0, 0x0048);

	/* wait for transmit complete */
	lp->cur_tx = 0;		/* (lp->cur_tx + 1); */
	time = currticks() + TICKS_PER_SEC;	/* wait one second */
	while (currticks() < time &&
	       ((short) le16_to_cpu(pcnet32_bufs.tx_ring[entry].status) < 0));

	if ((short) le16_to_cpu(pcnet32_bufs.tx_ring[entry].status) < 0)
		printf("PCNET32 timed out on transmit\n");

	/* Stop pointing at the current txb
	 * otherwise the card continues to send the packet */
	pcnet32_bufs.tx_ring[entry].base = 0;

}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void pcnet32_disable ( struct nic *nic __unused ) {
	/* Stop the PCNET32 here -- it ocassionally polls memory if we don't */
	lp->a.write_csr(ioaddr, 0, 0x0004);

	/*
	 * Switch back to 16-bit mode to avoid problems with dumb 
	 * DOS packet driver after a warm reboot
	 */
	lp->a.write_bcr(ioaddr, 20, 0);
}

/**************************************************************************
IRQ - Enable, Disable, or Force interrupts
***************************************************************************/
static void pcnet32_irq(struct nic *nic __unused, irq_action_t action __unused)
{
  switch ( action ) {
  case DISABLE :
    break;
  case ENABLE :
    break;
  case FORCE :
    break;
  }
}


/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
You should omit the last argument struct pci_device * for a non-PCI NIC
***************************************************************************/
static int pcnet32_probe ( struct nic *nic, struct pci_device *pci ) {

	int i, media;
	int fdx, mii, fset, dxsuflo, ltint;
	int chip_version;
	struct pcnet32_access *a = NULL;
	char *chipname;
	u8 promaddr[6];
	int shared = 1;

	if (pci->ioaddr == 0)
		return 0;

	/* BASE is used throughout to address the card */
	ioaddr = pci->ioaddr;
	printf("pcnet32.c: Found %s, Vendor=0x%hX Device=0x%hX\n",
	       pci->driver_name, pci->vendor, pci->device);

	nic->irqno  = 0;
	nic->ioaddr = pci->ioaddr & ~3;

	/* reset the chip */
	pcnet32_wio_reset(ioaddr);

	/* NOTE: 16-bit check is first, otherwise some older PCnet chips fail */
	if (pcnet32_wio_read_csr(ioaddr, 0) == 4
	    && pcnet32_wio_check(ioaddr)) {
		a = &pcnet32_wio;
	} else {
		pcnet32_dwio_reset(ioaddr);
		if (pcnet32_dwio_read_csr(ioaddr, 0) == 4
		    && pcnet32_dwio_check(ioaddr)) {
			a = &pcnet32_dwio;
		} else
			return 0;
	}

	chip_version =
	    a->read_csr(ioaddr, 88) | (a->read_csr(ioaddr, 89) << 16);

	dprintf(("PCnet chip version is 0x%X\n", chip_version));
	if ((chip_version & 0xfff) != 0x003)
		return 0;

	/* initialize variables */
	fdx = mii = fset = dxsuflo = ltint = 0;
	chip_version = (chip_version >> 12) & 0xffff;

	switch (chip_version) {
	case 0x2420:
		chipname = "PCnet/PCI 79C970";	/* PCI */
		break;
	case 0x2430:
		if (shared)
			chipname = "PCnet/PCI 79C970";	/* 970 gives the wrong chip id back */
		else
			chipname = "PCnet/32 79C965";	/* 486/VL bus */
		break;
	case 0x2621:
		chipname = "PCnet/PCI II 79C970A";	/* PCI */
		fdx = 1;
		break;
	case 0x2623:
		chipname = "PCnet/FAST 79C971";	/* PCI */
		fdx = 1;
		mii = 1;
		fset = 1;
		ltint = 1;
		break;
	case 0x2624:
		chipname = "PCnet/FAST+ 79C972";	/* PCI */
		fdx = 1;
		mii = 1;
		fset = 1;
		break;
	case 0x2625:
		chipname = "PCnet/FAST III 79C973";	/* PCI */
		fdx = 1;
		mii = 1;
		break;
	case 0x2626:
		chipname = "PCnet/Home 79C978";	/* PCI */
		fdx = 1;
		/* 
		 * This is based on specs published at www.amd.com.  This section
		 * assumes that a card with a 79C978 wants to go into 1Mb HomePNA
		 * mode.  The 79C978 can also go into standard ethernet, and there
		 * probably should be some sort of module option to select the
		 * mode by which the card should operate
		 */
		/* switch to home wiring mode */
		media = a->read_bcr(ioaddr, 49);

		printf("media reset to %#x.\n", media);
		a->write_bcr(ioaddr, 49, media);
		break;
	case 0x2627:
		chipname = "PCnet/FAST III 79C975";	/* PCI */
		fdx = 1;
		mii = 1;
		break;
	default:
		chipname = "UNKNOWN";
		printf("PCnet version %#x, no PCnet32 chip.\n",
		       chip_version);
		return 0;
	}

	/*
	 *  On selected chips turn on the BCR18:NOUFLO bit. This stops transmit
	 *  starting until the packet is loaded. Strike one for reliability, lose
	 *  one for latency - although on PCI this isnt a big loss. Older chips 
	 *  have FIFO's smaller than a packet, so you can't do this.
	 */

	if (fset) {
		a->write_bcr(ioaddr, 18,
			     (a->read_bcr(ioaddr, 18) | 0x0800));
		a->write_csr(ioaddr, 80,
			     (a->read_csr(ioaddr, 80) & 0x0C00) | 0x0c00);
		dxsuflo = 1;
		ltint = 1;
	}

	DBG ( "%s at %hX,", chipname, (unsigned int) ioaddr );

	/* read PROM address */
	for (i = 0; i < 6; i++)
		promaddr[i] = inb(ioaddr + i);

	/* Update the nic structure with the MAC Address */
	for (i = 0; i < ETH_ALEN; i++) {
		nic->node_addr[i] = promaddr[i];
	}

	/* Print out some hardware info */
	DBG ( "%s: IO Addr 0x%hX, MAC Addr %s\n ", chipname, (unsigned int) ioaddr,
	      eth_ntoa ( nic->node_addr ) );

	/* Set to pci bus master */
	adjust_pci_device(pci);

	/* point to private storage */
	lp = &lpx;

#if EBDEBUG
	if (((chip_version + 1) & 0xfffe) == 0x2624) {	/* Version 0x2623 or 0x2624 */
		i = a->read_csr(ioaddr, 80) & 0x0C00;	/* Check tx_start_pt */
		dprintf(("    tx_start_pt(0x%hX):", i));
		switch (i >> 10) {
		case 0:
			dprintf(("  20 bytes,"));
			break;
		case 1:
			dprintf(("  64 bytes,"));
			break;
		case 2:
			dprintf((" 128 bytes,"));
			break;
		case 3:
			dprintf(("~220 bytes,"));
			break;
		}
		i = a->read_bcr(ioaddr, 18);	/* Check Burst/Bus control */
		dprintf((" BCR18(%hX):", i & 0xffff));
		if (i & (1 << 5))
			dprintf(("BurstWrEn "));
		if (i & (1 << 6))
			dprintf(("BurstRdEn "));
		if (i & (1 << 7))
			dprintf(("DWordIO "));
		if (i & (1 << 11))
			dprintf(("NoUFlow "));
		i = a->read_bcr(ioaddr, 25);
		dprintf(("    SRAMSIZE=0x%hX,", i << 8));
		i = a->read_bcr(ioaddr, 26);
		dprintf((" SRAM_BND=0x%hX,", i << 8));
		i = a->read_bcr(ioaddr, 27);
		if (i & (1 << 14))
			dprintf(("LowLatRx"));
	}
#endif
	lp->name = chipname;
	lp->shared_irq = shared;
	lp->full_duplex = fdx;
	lp->dxsuflo = dxsuflo;
	lp->ltint = ltint;
	lp->mii = mii;
	/* FIXME: Fix Options for only one card */
	if ((cards_found >= MAX_UNITS)
	    || ((unsigned int) options[cards_found] > sizeof(options_mapping)))
		lp->options = PCNET32_PORT_ASEL;
	else
		lp->options = options_mapping[options[cards_found]];

	if (fdx && !(lp->options & PCNET32_PORT_ASEL) &&
	    ((cards_found >= MAX_UNITS) || full_duplex[cards_found]))
		lp->options |= PCNET32_PORT_FD;

	if (!a) {
		printf("No access methods\n");
		return 0;
	}

	//  lp->a = *a;
	//  Causes a loader:
	//     bin/blib.a(pcnet32.o)(.text+0x6b6): In function `pcnet32_probe':
	//     drivers/net/pcnet32.c:871: undefined reference to `memcpy'
	//     make: *** [bin/pcnet32.dsk.tmp] Error 1
	//  So we do:
	memcpy ( &lp->a, a, sizeof ( lp->a ) );
	//   To explicity call memcpy.

	/* detect special T1/E1 WAN card by checking for MAC address */
	if (nic->node_addr[0] == 0x00 && nic->node_addr[1] == 0xe0
	    && nic->node_addr[2] == 0x75)
		lp->options = PCNET32_PORT_FD | PCNET32_PORT_GPSI;

	lp->init_block.mode = le16_to_cpu(0x0003);	/* Disable Rx and Tx. */
	lp->init_block.tlen_rlen =
	    le16_to_cpu(TX_RING_LEN_BITS | RX_RING_LEN_BITS);
	for (i = 0; i < 6; i++)
		lp->init_block.phys_addr[i] = nic->node_addr[i];
	lp->init_block.filter[0] = 0xffffffff;
	lp->init_block.filter[1] = 0xffffffff;
	lp->init_block.rx_ring = virt_to_bus(&pcnet32_bufs.rx_ring);
	lp->init_block.tx_ring = virt_to_bus(&pcnet32_bufs.tx_ring);

	/* switch pcnet32 to 32bit mode */
	a->write_bcr(ioaddr, 20, 2);

	a->write_csr(ioaddr, 1, (virt_to_bus(&lp->init_block)) & 0xffff);
	a->write_csr(ioaddr, 2, (virt_to_bus(&lp->init_block)) >> 16);

	/* 
	 * To auto-IRQ we enable the initialization-done and DMA error
	 * interrupts. For ISA boards we get a DMA error, but VLB and PCI
	 * boards will work.
	 */
	/* Trigger an initialization just for the interrupt. */

	
//	a->write_csr(ioaddr, 0, 0x41); 
//	mdelay(1);

	cards_found++;

	/* point to NIC specific routines */
	pcnet32_reset(nic);
	if (mii) {
		int tmp;
		int phy, phy_idx = 0;
		u16 mii_lpa;
		lp->phys[0] = 1;	/* Default Setting */
		for (phy = 1; phy < 32 && phy_idx < MII_CNT; phy++) {
			int mii_status = mdio_read(nic, phy, MII_BMSR);
			if (mii_status != 0xffff && mii_status != 0x0000) {
				lp->phys[phy_idx++] = phy;
				lp->mii_if.advertising =
				    mdio_read(nic, phy, MII_ADVERTISE);
				if ((mii_status & 0x0040) == 0) {
				  tmp = phy;
				  dprintf (("MII PHY found at address %d, status " 
					    "%hX advertising %hX\n", phy, mii_status, 
					    lp->mii_if.advertising));
				}
			}
		}
		if (phy_idx == 0)
			printf("No MII transceiver found!\n");
		lp->mii_if.phy_id = lp->phys[0];

		lp->mii_if.advertising =
		    mdio_read(nic, lp->phys[0], MII_ADVERTISE);

		mii_lpa = mdio_read(nic, lp->phys[0], MII_LPA);
		lp->mii_if.advertising &= mii_lpa;
		if (lp->mii_if.advertising & ADVERTISE_100FULL)
			printf("100Mbps Full-Duplex\n");
		else if (lp->mii_if.advertising & ADVERTISE_100HALF)
			printf("100Mbps Half-Duplex\n");
		else if (lp->mii_if.advertising & ADVERTISE_10FULL)
			printf("10Mbps Full-Duplex\n");
		else if (lp->mii_if.advertising & ADVERTISE_10HALF)
			printf("10Mbps Half-Duplex\n");
		else
			printf("\n");
	} else {
		/* The older chips are fixed 10Mbps, and some support full duplex,
		 * although not via autonegotiation, but only via configuration.  */
		if (fdx)
			printf("10Mbps Full-Duplex\n");
		else
			printf("10Mbps Half-Duplex\n");
	}

	nic->nic_op	= &pcnet32_operations;

	return 1;
}
static int mdio_read(struct nic *nic __unused, int phy_id, int reg_num)
{
	u16 val_out;
	int phyaddr;

	if (!lp->mii)
		return 0;

	phyaddr = lp->a.read_bcr(ioaddr, 33);

	lp->a.write_bcr(ioaddr, 33,
			((phy_id & 0x1f) << 5) | (reg_num & 0x1f));
	val_out = lp->a.read_bcr(ioaddr, 34);
	lp->a.write_bcr(ioaddr, 33, phyaddr);

	return val_out;
}

#if 0
static void mdio_write(struct nic *nic __unused, int phy_id, int reg_num,
		       int val)
{
	int phyaddr;

	if (!lp->mii)
		return;

	phyaddr = lp->a.read_bcr(ioaddr, 33);

	lp->a.write_bcr(ioaddr, 33,
			((phy_id & 0x1f) << 5) | (reg_num & 0x1f));
	lp->a.write_bcr(ioaddr, 34, val);
	lp->a.write_bcr(ioaddr, 33, phyaddr);
}
#endif

static struct nic_operations pcnet32_operations = {
	.connect	= dummy_connect,
	.poll		= pcnet32_poll,
	.transmit	= pcnet32_transmit,
	.irq		= pcnet32_irq,

};

static struct pci_device_id pcnet32_nics[] = {
	PCI_ROM(0x1022, 0x2000, "pcnet32", "AMD PCnet/PCI", 0),
	PCI_ROM(0x1022, 0x2625, "pcnetfastiii", "AMD PCNet FAST III", 0),
	PCI_ROM(0x1022, 0x2001, "amdhomepna", "AMD PCnet/HomePNA", 0),
};

PCI_DRIVER ( pcnet32_driver, pcnet32_nics, PCI_NO_CLASS );

DRIVER ( "PCNET32/PCI", nic_driver, pci_driver, pcnet32_driver,
	 pcnet32_probe, pcnet32_disable );

/*
 * Local variables:
 *  c-basic-offset: 8
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
