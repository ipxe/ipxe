/**************************************************************************
*    r8169.c: Etherboot device driver for the RealTek RTL-8169 Gigabit
*    Written 2003 by Timothy Legge <tlegge@rogers.com>
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
*	r8169.c: A RealTek RTL-8169 Gigabit Ethernet driver 
* 		for Linux kernel 2.4.x.
*
*    Written 2002 ShuChen <shuchen@realtek.com.tw>
*	  See Linux Driver for full information
*	
*    Linux Driver Versions: 
*	1.27a, 10.02.2002
*	RTL8169_VERSION "2.2"   <2004/08/09>
* 
*    Thanks to:
*    	Jean Chen of RealTek Semiconductor Corp. for
*    	providing the evaluation NIC used to develop 
*    	this driver.  RealTek's support for Etherboot 
*    	is appreciated.
*    	
*    REVISION HISTORY:
*    ================
*
*    v1.0	11-26-2003	timlegge	Initial port of Linux driver
*    v1.5	01-17-2004	timlegge	Initial driver output cleanup
*    v1.6	03-27-2004	timlegge	Additional Cleanup
*    v1.7	11-22-2005	timlegge	Update to RealTek Driver Version 2.2
*
*		03-19-2008	Hilko Bengen	Cleanups and fixes for newer cards
*				(successfully tested with 8110SC-d onboard NIC)
*    
*    Indent Options: indent -kr -i8
***************************************************************************/

#include "etherboot.h"
#include "nic.h"
#include <gpxe/pci.h>
#include <gpxe/ethernet.h>
#include <gpxe/malloc.h>

#define drv_version "v1.7+"
#define drv_date "03-19-2008"

#define HZ 1000

static u32 ioaddr;

/* Condensed operations for readability. */
#define virt_to_le32desc(addr)  cpu_to_le32(virt_to_bus(addr))
#define le32desc_to_virt(addr)  bus_to_virt(le32_to_cpu(addr))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#undef RTL8169_DEBUG
#undef RTL8169_JUMBO_FRAME_SUPPORT
#undef RTL8169_HW_FLOW_CONTROL_SUPPORT


#undef RTL8169_IOCTL_SUPPORT
#undef RTL8169_DYNAMIC_CONTROL
#define RTL8169_USE_IO

#if 0
/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;
#endif

#if 0
/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC.  */
static int multicast_filter_limit = 32;
#endif

/* MAC address length*/
#define MAC_ADDR_LEN	6

/* max supported gigabit ethernet frame size -- must be at least (dev->mtu+14+4).*/
#define MAX_ETH_FRAME_SIZE	1536

#define TX_FIFO_THRESH 256	/* In bytes */

#define RX_FIFO_THRESH	7	/* 7 means NO threshold, Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST	7	/* Maximum PCI burst, '6' is 1024 */
#define TX_DMA_BURST	7	/* Maximum PCI burst, '6' is 1024 */
#define ETTh                0x3F	/* 0x3F means NO threshold */

#define EarlyTxThld 	0x3F	/* 0x3F means NO early transmit */
#define RxPacketMaxSize	0x0800	/* Maximum size supported is 16K-1 */
#define InterFrameGap	0x03	/* 3 means InterFrameGap = the shortest one */

#define NUM_TX_DESC	1	/* Number of Tx descriptor registers */
#define NUM_RX_DESC	4	/* Number of Rx descriptor registers */
#define RX_BUF_SIZE	1536	/* Rx Buffer size */

#define RTL_MIN_IO_SIZE 0x80
#define TX_TIMEOUT  (6*HZ)

#define RTL8169_TIMER_EXPIRE_TIME 100	//100

#define ETH_HDR_LEN         14
#define DEFAULT_MTU         1500
#define DEFAULT_RX_BUF_LEN  1536


#ifdef RTL8169_JUMBO_FRAME_SUPPORT
#define MAX_JUMBO_FRAME_MTU    ( 10000 )
#define MAX_RX_SKBDATA_SIZE    ( MAX_JUMBO_FRAME_MTU + ETH_HDR_LEN )
#else
#define MAX_RX_SKBDATA_SIZE 1600
#endif				//end #ifdef RTL8169_JUMBO_FRAME_SUPPORT

#ifdef RTL8169_USE_IO
#define RTL_W8(reg, val8)   outb ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16) outw ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32) outl ((val32), ioaddr + (reg))
#define RTL_R8(reg)         inb (ioaddr + (reg))
#define RTL_R16(reg)        inw (ioaddr + (reg))
#define RTL_R32(reg)        ((unsigned long) inl (ioaddr + (reg)))
#else
/* write/read MMIO register */
#define RTL_W8(reg, val8)	writeb ((val8), ioaddr + (reg))
#define RTL_W16(reg, val16)	writew ((val16), ioaddr + (reg))
#define RTL_W32(reg, val32)	writel ((val32), ioaddr + (reg))
#define RTL_R8(reg)		readb (ioaddr + (reg))
#define RTL_R16(reg)		readw (ioaddr + (reg))
#define RTL_R32(reg)		((unsigned long) readl (ioaddr + (reg)))
#endif

enum mac_version {
	RTL_GIGA_MAC_VER_01 = 0x01, // 8169
	RTL_GIGA_MAC_VER_02 = 0x02, // 8169S
	RTL_GIGA_MAC_VER_03 = 0x03, // 8110S
	RTL_GIGA_MAC_VER_04 = 0x04, // 8169SB
	RTL_GIGA_MAC_VER_05 = 0x05, // 8110SCd
	RTL_GIGA_MAC_VER_06 = 0x06, // 8110SCe
	RTL_GIGA_MAC_VER_11 = 0x0b, // 8168Bb
	RTL_GIGA_MAC_VER_12 = 0x0c, // 8168Be
	RTL_GIGA_MAC_VER_13 = 0x0d, // 8101Eb
	RTL_GIGA_MAC_VER_14 = 0x0e, // 8101 ?
	RTL_GIGA_MAC_VER_15 = 0x0f, // 8101 ?
	RTL_GIGA_MAC_VER_16 = 0x11, // 8101Ec
	RTL_GIGA_MAC_VER_17 = 0x10, // 8168Bf
	RTL_GIGA_MAC_VER_18 = 0x12, // 8168CP
	RTL_GIGA_MAC_VER_19 = 0x13, // 8168C
	RTL_GIGA_MAC_VER_20 = 0x14  // 8168C
};

enum cfg_version {
	RTL_CFG_0 = 0x00,
	RTL_CFG_1,
	RTL_CFG_2
};

static struct {
	const char *name;
	u8 mac_version;		/* depend on RTL8169 docs */
	u32 RxConfigMask;	/* should clear the bits supported by this chip */
} rtl_chip_info[] = {
        {"RTL8169",           RTL_GIGA_MAC_VER_01, 0xff7e1880}, // 8169
        {"RTL8169s",          RTL_GIGA_MAC_VER_02, 0xff7e1880}, // 8169S
        {"RTL8110s",          RTL_GIGA_MAC_VER_03, 0xff7e1880}, // 8110S
        {"RTL8169sb/8110sb",  RTL_GIGA_MAC_VER_04, 0xff7e1880}, // 8169SB
        {"RTL8169sc/8110sc-d",RTL_GIGA_MAC_VER_05, 0xff7e1880}, // 8110SCd
        {"RTL8169sc/8110sc-e",RTL_GIGA_MAC_VER_06, 0xff7e1880}, // 8110SCe
        {"RTL8168b/8111b",    RTL_GIGA_MAC_VER_11, 0xff7e1880}, // PCI-E
        {"RTL8168b/8111b",    RTL_GIGA_MAC_VER_12, 0xff7e1880}, // PCI-E
        {"RTL8101e",          RTL_GIGA_MAC_VER_13, 0xff7e1880}, // PCI-E 8139
        {"RTL8100e",          RTL_GIGA_MAC_VER_14, 0xff7e1880}, // PCI-E 8139
        {"RTL8100e",          RTL_GIGA_MAC_VER_15, 0xff7e1880}, // PCI-E 8139
        {"RTL8168b/8111b",    RTL_GIGA_MAC_VER_17, 0xff7e1880}, // PCI-E
        {"RTL8101e",          RTL_GIGA_MAC_VER_16, 0xff7e1880}, // PCI-E
        {"RTL8168cp/8111cp",  RTL_GIGA_MAC_VER_18, 0xff7e1880}, // PCI-E
        {"RTL8168c/8111c",    RTL_GIGA_MAC_VER_19, 0xff7e1880}, // PCI-E
        {"RTL8168c/8111c",    RTL_GIGA_MAC_VER_20, 0xff7e1880}, // PCI-E
};

enum RTL8169_registers {
	MAC0 = 0x0,		/* Ethernet hardware address. */
	MAR0 = 0x8,		/* Multicast filter. */
	TxDescAddrLow = 0x20,
	TxDescAddrHigh	= 0x24,
	TxHDescStartAddr = 0x28,
	FLASH = 0x30,
	ERSR = 0x36,
	ChipCmd = 0x37,
	TxPoll = 0x38,
	IntrMask = 0x3C,
	IntrStatus = 0x3E,
	TxConfig = 0x40,
	RxConfig = 0x44,
	RxMissed = 0x4C,
	Cfg9346 = 0x50,
	Config0 = 0x51,
	Config1 = 0x52,
	Config2 = 0x53,
	Config3 = 0x54,
	Config4 = 0x55,
	Config5 = 0x56,
	MultiIntr = 0x5C,
	PHYAR = 0x60,
	TBICSR = 0x64,
	TBI_ANAR = 0x68,
	TBI_LPAR = 0x6A,
	PHYstatus = 0x6C,
	RxMaxSize = 0xda,
	CPlusCmd = 0xe0,
        IntrMitigate = 0xe2,
	RxDescAddrLow = 0xe4,
	RxDescAddrHigh	= 0xe8,
	ETThReg = 0xEC,
	FuncEvent = 0xF0,
	FuncEventMask = 0xF4,
	FuncPresetState = 0xF8,
	FuncForceEvent = 0xFC,
};

enum RTL8169_register_content {
	/*InterruptStatusBits */
	SYSErr = 0x8000,
	PCSTimeout = 0x4000,
	SWInt = 0x0100,
	TxDescUnavail = 0x80,
	RxFIFOOver = 0x40,
	LinkChg = 0x20,
	RxOverflow = 0x10,
	TxErr = 0x08,
	TxOK = 0x04,
	RxErr = 0x02,
	RxOK = 0x01,

	/*RxStatusDesc */
	RxRES = 0x00200000,
	RxCRC = 0x00080000,
	RxRUNT = 0x00100000,
	RxRWT = 0x00400000,

	/*ChipCmdBits */
	CmdReset = 0x10,
	CmdRxEnb = 0x08,
	CmdTxEnb = 0x04,
	RxBufEmpty = 0x01,

	/*Cfg9346Bits */
	Cfg9346_Lock = 0x00,
	Cfg9346_Unlock = 0xC0,

	/*rx_mode_bits */
	AcceptErr = 0x20,
	AcceptRunt = 0x10,
	AcceptBroadcast = 0x08,
	AcceptMulticast = 0x04,
	AcceptMyPhys = 0x02,
	AcceptAllPhys = 0x01,

	/*RxConfigBits */
	RxCfgFIFOShift = 13,
	RxCfgDMAShift = 8,

	/*TxConfigBits */
	TxInterFrameGapShift = 24,
	TxDMAShift = 8,		/* DMA burst value (0-7) is shift this many bits */

	/*rtl8169_PHYstatus */
	TBI_Enable = 0x80,
	TxFlowCtrl = 0x40,
	RxFlowCtrl = 0x20,
	_1000bpsF = 0x10,
	_100bps = 0x08,
	_10bps = 0x04,
	LinkStatus = 0x02,
	FullDup = 0x01,

	/*GIGABIT_PHY_registers */
	PHY_CTRL_REG = 0,
	PHY_STAT_REG = 1,
	PHY_AUTO_NEGO_REG = 4,
	PHY_1000_CTRL_REG = 9,

	/*GIGABIT_PHY_REG_BIT */
	PHY_Restart_Auto_Nego = 0x0200,
	PHY_Enable_Auto_Nego = 0x1000,

	/* PHY_STAT_REG = 1; */
	PHY_Auto_Neco_Comp = 0x0020,

	/* PHY_AUTO_NEGO_REG = 4; */
	PHY_Cap_10_Half = 0x0020,
	PHY_Cap_10_Full = 0x0040,
	PHY_Cap_100_Half = 0x0080,
	PHY_Cap_100_Full = 0x0100,

	/* PHY_1000_CTRL_REG = 9; */
	PHY_Cap_1000_Full = 0x0200,
	PHY_Cap_1000_Half = 0x0100,

	PHY_Cap_PAUSE = 0x0400,
	PHY_Cap_ASYM_PAUSE = 0x0800,

	PHY_Cap_Null = 0x0,

	/*_MediaType*/
	_10_Half = 0x01,
	_10_Full = 0x02,
	_100_Half = 0x04,
	_100_Full = 0x08,
	_1000_Full = 0x10,

	/*_TBICSRBit*/
	TBILinkOK = 0x02000000,
};

enum _DescStatusBit {
	OWNbit = 0x80000000,
	EORbit = 0x40000000,
	FSbit = 0x20000000,
	LSbit = 0x10000000,
};

struct TxDesc {
	u32 status;
	u32 vlan_tag;
	u32 buf_addr;
	u32 buf_Haddr;
};

struct RxDesc {
	u32 status;
	u32 vlan_tag;
	u32 buf_addr;
	u32 buf_Haddr;
};

/* The descriptors for this card are required to be aligned on 256
 * byte boundaries.  As the align attribute does not do more than 16
 * bytes of alignment it requires some extra steps.  Add 256 to the
 * size of the array and the init_ring adjusts the alignment.
 *
 * UPDATE: This is no longer true; we can request arbitrary alignment.
 */

/* Define the TX and RX Descriptors and Buffers */
#define __align_256 __attribute__ (( aligned ( 256 ) ))
struct {
	struct TxDesc tx_ring[NUM_TX_DESC] __align_256;
	unsigned char txb[NUM_TX_DESC * RX_BUF_SIZE];
	struct RxDesc rx_ring[NUM_RX_DESC] __align_256;
	unsigned char rxb[NUM_RX_DESC * RX_BUF_SIZE];
} *r8169_bufs;
#define tx_ring r8169_bufs->tx_ring
#define rx_ring r8169_bufs->rx_ring
#define txb r8169_bufs->txb
#define rxb r8169_bufs->rxb

static struct rtl8169_private {
	void *mmio_addr;	/* memory map physical address */
	int chipset;
	int pcfg;
	int mac_version;
	unsigned long cur_rx;	/* Index into the Rx descriptor buffer of next Rx pkt. */
	unsigned long cur_tx;	/* Index into the Tx descriptor buffer of next Rx pkt. */
	struct TxDesc *TxDescArray;	/* Index of 256-alignment Tx Descriptor buffer */
	struct RxDesc *RxDescArray;	/* Index of 256-alignment Rx Descriptor buffer */
	unsigned char *RxBufferRing[NUM_RX_DESC];	/* Index of Rx Buffer array */
	unsigned char *Tx_skbuff[NUM_TX_DESC];
} tpx;

static const u16 rtl8169_intr_mask =
    LinkChg | RxOverflow | RxFIFOOver | TxErr | TxOK | RxErr | RxOK;
static const unsigned int rtl8169_rx_config =
    (RX_FIFO_THRESH << RxCfgFIFOShift) | (RX_DMA_BURST << RxCfgDMAShift) |
    0x0000000E;

static void rtl8169_hw_phy_config(struct nic *nic __unused);
//static void rtl8169_hw_phy_reset(struct net_device *dev);

#define RTL8169_WRITE_GMII_REG_BIT( ioaddr, reg, bitnum, bitval )\
{ \
       int val; \
       if( bitval == 1 ){ val = ( RTL8169_READ_GMII_REG( ioaddr, reg ) | (bitval<<bitnum) ) & 0xffff ; } \
       else{ val = ( RTL8169_READ_GMII_REG( ioaddr, reg ) & (~(0x0001<<bitnum)) ) & 0xffff ; } \
       RTL8169_WRITE_GMII_REG( ioaddr, reg, val ); \
 }

//=================================================================
//      PHYAR
//      bit             Symbol
//      31              Flag
//      30-21   reserved
//      20-16   5-bit GMII/MII register address
//      15-0    16-bit GMII/MII register data
//=================================================================
static void RTL8169_WRITE_GMII_REG(unsigned long ioaddr, int RegAddr, int value)
{
	int i;

	RTL_W32(PHYAR, 0x80000000 | (RegAddr & 0xFF) << 16 | value);
	udelay(1000);

	for (i = 2000; i > 0; i--) {
		// Check if the RTL8169 has completed writing to the specified MII register
		if (!(RTL_R32(PHYAR) & 0x80000000)) {
			break;
		} else {
			udelay(100);
		}		// end of if( ! (RTL_R32(PHYAR)&0x80000000) )
	}			// end of for() loop
}

//=================================================================
static int RTL8169_READ_GMII_REG(unsigned long ioaddr, int RegAddr)
{
	int i, value = -1;

	RTL_W32(PHYAR, 0x0 | (RegAddr & 0xFF) << 16);
	udelay(1000);

	for (i = 2000; i > 0; i--) {
		// Check if the RTL8169 has completed retrieving data from the specified MII register
		if (RTL_R32(PHYAR) & 0x80000000) {
			value = (int) (RTL_R32(PHYAR) & 0xFFFF);
			break;
		} else {
			udelay(100);
		}		// end of if( RTL_R32(PHYAR) & 0x80000000 )
	}			// end of for() loop
	return value;
}


#if 0
static void mdio_write(int RegAddr, int value)
{
	int i;

	RTL_W32(PHYAR, 0x80000000 | (RegAddr & 0xFF) << 16 | value);
	udelay(1000);

	for (i = 2000; i > 0; i--) {
		/* Check if the RTL8169 has completed writing to the specified MII register */
		if (!(RTL_R32(PHYAR) & 0x80000000)) {
			break;
		} else {
			udelay(100);
		}
	}
}

static int mdio_read(int RegAddr)
{
	int i, value = -1;

	RTL_W32(PHYAR, 0x0 | (RegAddr & 0xFF) << 16);
	udelay(1000);

	for (i = 2000; i > 0; i--) {
		/* Check if the RTL8169 has completed retrieving data from the specified MII register */
		if (RTL_R32(PHYAR) & 0x80000000) {
			value = (int) (RTL_R32(PHYAR) & 0xFFFF);
			break;
		} else {
			udelay(100);
		}
	}
	return value;
}
#endif

static void rtl8169_get_mac_version( struct rtl8169_private *tp,
                                     u32 ioaddr )
{
	/*
	 * The driver currently handles the 8168Bf and the 8168Be identically
	 * but they can be identified more specifically through the test below
	 * if needed:
	 *
	 * (RTL_R32(TxConfig) & 0x700000) == 0x500000 ? 8168Bf : 8168Be
	 *
	 * Same thing for the 8101Eb and the 8101Ec:
	 *
	 * (RTL_R32(TxConfig) & 0x700000) == 0x200000 ? 8101Eb : 8101Ec
	 */
	const struct {
		u32 mask;
		u32 val;
		int mac_version;
	} mac_info[] = {
		/* 8168B family. */
		{ 0x7c800000, 0x3c800000,	RTL_GIGA_MAC_VER_18 },
		{ 0x7cf00000, 0x3c000000,	RTL_GIGA_MAC_VER_19 },
		{ 0x7cf00000, 0x3c200000,	RTL_GIGA_MAC_VER_20 },
		{ 0x7c800000, 0x3c000000,	RTL_GIGA_MAC_VER_20 },
		/* 8168B family. */
		{ 0x7cf00000, 0x38000000,	RTL_GIGA_MAC_VER_12 },
		{ 0x7cf00000, 0x38500000,	RTL_GIGA_MAC_VER_17 },
		{ 0x7c800000, 0x38000000,	RTL_GIGA_MAC_VER_17 },
		{ 0x7c800000, 0x30000000,	RTL_GIGA_MAC_VER_11 },
		/* 8101 family. */
		{ 0x7cf00000, 0x34000000,	RTL_GIGA_MAC_VER_13 },
		{ 0x7cf00000, 0x34200000,	RTL_GIGA_MAC_VER_16 },
		{ 0x7c800000, 0x34000000,	RTL_GIGA_MAC_VER_16 },
		/* FIXME: where did these entries come from ? -- FR */
		{ 0xfc800000, 0x38800000,	RTL_GIGA_MAC_VER_15 },
		{ 0xfc800000, 0x30800000,	RTL_GIGA_MAC_VER_14 },
		/* 8110 family. */
		{ 0xfc800000, 0x98000000,	RTL_GIGA_MAC_VER_06 },
		{ 0xfc800000, 0x18000000,	RTL_GIGA_MAC_VER_05 },
		{ 0xfc800000, 0x10000000,	RTL_GIGA_MAC_VER_04 },
		{ 0xfc800000, 0x04000000,	RTL_GIGA_MAC_VER_03 },
		{ 0xfc800000, 0x00800000,	RTL_GIGA_MAC_VER_02 },
		{ 0xfc800000, 0x00000000,	RTL_GIGA_MAC_VER_01 },
		{ 0x00000000, 0x00000000,	RTL_GIGA_MAC_VER_01 }	/* Catch-all */
	}, *p = mac_info;

	unsigned long rv;

        rv = (RTL_R32(TxConfig));

	while ((rv & p->mask) != p->val)
		p++;
	tp->mac_version = p->mac_version;

	if (p->mask == 0x00000000) {
                DBG("unknown MAC (%08lx)\n", rv);
	}
}

#define IORESOURCE_MEM 0x00000200

static int rtl8169_init_board(struct pci_device *pdev)
{
	int i;
//	unsigned long mmio_end, mmio_flags
        unsigned long mmio_start, mmio_len;
	struct rtl8169_private *tp = &tpx;

	adjust_pci_device(pdev);

	mmio_start = pci_bar_start(pdev, PCI_BASE_ADDRESS_1);
//       mmio_end = pci_resource_end (pdev, 1);
//       mmio_flags = pci_resource_flags (pdev, PCI_BASE_ADDRESS_1);
	mmio_len = pci_bar_size(pdev, PCI_BASE_ADDRESS_1);

	// make sure PCI base addr 1 is MMIO
//     if (!(mmio_flags & IORESOURCE_MEM)) {
//             printf ("region #1 not an MMIO resource, aborting\n");
//             return 0;
//     }

	// check for weird/broken PCI region reporting
	if (mmio_len < RTL_MIN_IO_SIZE) {
		printf("Invalid PCI region size(s), aborting\n");
		return 0;
	}
#ifdef RTL8169_USE_IO
	ioaddr = pci_bar_start(pdev, PCI_BASE_ADDRESS_0);
#else
	// ioremap MMIO region
	ioaddr = (unsigned long) ioremap(mmio_start, mmio_len);
	if (ioaddr == 0) {
		printk("cannot remap MMIO, aborting\n");
		return 0;
	}
#endif

	tp->mmio_addr = (void*)ioaddr;
	/* Soft reset the chip. */
	RTL_W8(ChipCmd, CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--)
		if ((RTL_R8(ChipCmd) & CmdReset) == 0)
			break;
		else
			udelay(10);

	/* Identify chip attached to board */
        rtl8169_get_mac_version( tp, ioaddr );

	// rtl8169_print_mac_version(tp);

	{
		unsigned char val8 =
		    (unsigned char) (RTL8169_READ_GMII_REG(ioaddr, 3) &
				     0x000f);
		if (val8 == 0x00) {
			tp->pcfg = RTL_CFG_0;
		} else if (val8 == 0x01) {
			tp->pcfg = RTL_CFG_1;
		} else if (val8 == 0x02) {
			tp->pcfg = RTL_CFG_2;
		} else {
			tp->pcfg = RTL_CFG_2;
		}
	}

	/* identify chip attached to board */

	for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--)
		if (tp->mac_version == rtl_chip_info[i].mac_version) {
			tp->chipset = i;
			goto match;
		}
	/* if unknown chip, assume array element #0, original RTL-8169 in this case */
	DBG ( "PCI device: unknown chip version, assuming RTL-8169\n" );
	DBG ( "PCI device: TxConfig = %#lX\n", ( unsigned long ) RTL_R32 ( TxConfig ) );

	tp->chipset = 0;
	return 1;

      match:
	return 0;

}

/**************************************************************************
IRQ - Wait for a frame
***************************************************************************/
static void r8169_irq(struct nic *nic __unused, irq_action_t action)
{
	int intr_status = 0;
	int interested = RxOverflow | RxFIFOOver | RxErr | RxOK;

	switch (action) {
	case DISABLE:
	case ENABLE:
		intr_status = RTL_R16(IntrStatus);
		/* h/w no longer present (hotplug?) or major error, 
		   bail */
		if (intr_status == 0xFFFF)
			break;
		
		intr_status = intr_status & ~interested;
		if (action == ENABLE)
			intr_status = intr_status | interested;
		RTL_W16(IntrMask, intr_status);
		break;
	case FORCE:
		RTL_W8(TxPoll, (RTL_R8(TxPoll) | 0x01));
		break;
	}
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int r8169_poll(struct nic *nic, int retrieve)
{
	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */
	int cur_rx;
	unsigned int intr_status = 0;
	struct rtl8169_private *tp = &tpx;

	cur_rx = tp->cur_rx;
	if ((tp->RxDescArray[cur_rx].status & OWNbit) == 0) {
		/* There is a packet ready */
                DBG("r8169_poll(): packet ready\n");
		if (!retrieve)
			return 1;
		intr_status = RTL_R16(IntrStatus);
		/* h/w no longer present (hotplug?) or major error,
		   bail */
		if (intr_status == 0xFFFF) {
                        DBG("r8169_poll(): unknown error\n");
			return 0;
                }
		RTL_W16(IntrStatus, intr_status &
			~(RxFIFOOver | RxOverflow | RxOK));

		if (!(tp->RxDescArray[cur_rx].status & RxRES)) {
			nic->packetlen = (int) (tp->RxDescArray[cur_rx].
						status & 0x00001FFF) - 4;
			memcpy(nic->packet, tp->RxBufferRing[cur_rx],
			       nic->packetlen);
			if (cur_rx == NUM_RX_DESC - 1)
				tp->RxDescArray[cur_rx].status =
				    (OWNbit | EORbit) + RX_BUF_SIZE;
			else
				tp->RxDescArray[cur_rx].status =
				    OWNbit + RX_BUF_SIZE;
			tp->RxDescArray[cur_rx].buf_addr =
			    virt_to_bus(tp->RxBufferRing[cur_rx]);
			tp->RxDescArray[cur_rx].buf_Haddr = 0;
		} else
			printf("Error Rx");
		/* FIXME: shouldn't I reset the status on an error */
		cur_rx = (cur_rx + 1) % NUM_RX_DESC;
		tp->cur_rx = cur_rx;
		RTL_W16(IntrStatus, intr_status &
			(RxFIFOOver | RxOverflow | RxOK));

		return 1;

	}
	tp->cur_rx = cur_rx;
	/* FIXME: There is no reason to do this as cur_rx did not change */

	return (0);		/* initially as this is called to flush the input */

}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void r8169_transmit(struct nic *nic, const char *d,	/* Destination */
			   unsigned int t,	/* Type */
			   unsigned int s,	/* size */
			   const char *p)
{				/* Packet */
	/* send the packet to destination */

	u16 nstype;
	u32 to;
	u8 *ptxb;
	struct rtl8169_private *tp = &tpx;
	int entry = tp->cur_tx % NUM_TX_DESC;

	/* point to the current txb incase multiple tx_rings are used */
	ptxb = tp->Tx_skbuff[entry * MAX_ETH_FRAME_SIZE];
	memcpy(ptxb, d, ETH_ALEN);
	memcpy(ptxb + ETH_ALEN, nic->node_addr, ETH_ALEN);
	nstype = htons((u16) t);
	memcpy(ptxb + 2 * ETH_ALEN, (u8 *) & nstype, 2);
	memcpy(ptxb + ETH_HLEN, p, s);
	s += ETH_HLEN;
	s &= 0x0FFF;
	while (s < ETH_ZLEN)
		ptxb[s++] = '\0';

	tp->TxDescArray[entry].buf_addr = virt_to_bus(ptxb);
	tp->TxDescArray[entry].buf_Haddr = 0;
	if (entry != (NUM_TX_DESC - 1))
		tp->TxDescArray[entry].status =
		    (OWNbit | FSbit | LSbit) | ((s > ETH_ZLEN) ? s :
						ETH_ZLEN);
	else
		tp->TxDescArray[entry].status =
		    (OWNbit | EORbit | FSbit | LSbit) | ((s > ETH_ZLEN) ? s
							 : ETH_ZLEN);
	RTL_W8(TxPoll, 0x40);	/* set polling bit */

	tp->cur_tx++;
	to = currticks() + TX_TIMEOUT;
	while ((tp->TxDescArray[entry].status & OWNbit) && (currticks() < to));	/* wait */

	if (currticks() >= to) {
		printf("TX Time Out");
	}
}

static void rtl8169_set_rx_mode(struct nic *nic __unused)
{
	u32 mc_filter[2];	/* Multicast hash filter */
	int rx_mode;
	u32 tmp = 0;
	struct rtl8169_private *tp = &tpx;

	/* IFF_ALLMULTI */
	/* Too many to filter perfectly -- accept all multicasts. */
	rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
	mc_filter[1] = mc_filter[0] = 0xffffffff;

	tmp =
	    rtl8169_rx_config | rx_mode | (RTL_R32(RxConfig) &
					   rtl_chip_info[tp->chipset].
					   RxConfigMask);

	RTL_W32(RxConfig, tmp);
	RTL_W32(MAR0 + 0, mc_filter[0]);
	RTL_W32(MAR0 + 4, mc_filter[1]);
}
static void rtl8169_hw_start(struct nic *nic)
{
	u32 i;
	struct rtl8169_private *tp = &tpx;

	/* Soft reset the chip. */
	RTL_W8(ChipCmd, CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--) {
		if ((RTL_R8(ChipCmd) & CmdReset) == 0)
			break;
		else
			udelay(10);
	}

	RTL_W8(Cfg9346, Cfg9346_Unlock);
	RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);
	RTL_W8(ETThReg, ETTh);

	/* For gigabit rtl8169 */
	RTL_W16(RxMaxSize, RxPacketMaxSize);

	/* Set Rx Config register */
	i = rtl8169_rx_config | (RTL_R32(RxConfig) &
				 rtl_chip_info[tp->chipset].RxConfigMask);
	RTL_W32(RxConfig, i);

	/* Set DMA burst size and Interframe Gap Time */
	RTL_W32(TxConfig,
		(TX_DMA_BURST << TxDMAShift) | (InterFrameGap <<
						TxInterFrameGapShift));


	RTL_W16(CPlusCmd, RTL_R16(CPlusCmd));

	if (tp->mac_version == RTL_GIGA_MAC_VER_02 || tp->mac_version == RTL_GIGA_MAC_VER_03) {
		RTL_W16(CPlusCmd,
			(RTL_R16(CPlusCmd) | (1 << 14) | (1 << 3)));
		DBG
		    ("Set MAC Reg C+CR Offset 0xE0: bit-3 and bit-14\n");
	} else {
		RTL_W16(CPlusCmd, (RTL_R16(CPlusCmd) | (1 << 3)));
		DBG("Set MAC Reg C+CR Offset 0xE0: bit-3.\n");
	}

	{
		//RTL_W16(IntrMitigate, 0x1517);
		//RTL_W16(IntrMitigate, 0x152a);
		//RTL_W16(IntrMitigate, 0x282a);
		RTL_W16(IntrMitigate, 0x0000);
	}

	tp->cur_rx = 0;

	RTL_W32(TxDescAddrLow, virt_to_le32desc(tp->TxDescArray));
	RTL_W32(TxDescAddrHigh, virt_to_le32desc(NULL));
	RTL_W32(RxDescAddrLow, virt_to_le32desc(tp->RxDescArray));
	RTL_W32(RxDescAddrHigh, virt_to_le32desc(NULL));
	RTL_W8(Cfg9346, Cfg9346_Lock);
	udelay(10);

	RTL_W32(RxMissed, 0);

	rtl8169_set_rx_mode(nic);

	/* no early-rx interrupts */
	RTL_W16(MultiIntr, RTL_R16(MultiIntr) & 0xF000);

	RTL_W16(IntrMask, rtl8169_intr_mask);

}

static void rtl8169_init_ring(struct nic *nic __unused)
{
	int i;
	struct rtl8169_private *tp = &tpx;

	tp->cur_rx = 0;
	tp->cur_tx = 0;
	memset(tp->TxDescArray, 0x0, NUM_TX_DESC * sizeof(struct TxDesc));
	memset(tp->RxDescArray, 0x0, NUM_RX_DESC * sizeof(struct RxDesc));

	for (i = 0; i < NUM_TX_DESC; i++) {
		tp->Tx_skbuff[i] = &txb[i];
	}

	for (i = 0; i < NUM_RX_DESC; i++) {
		if (i == (NUM_RX_DESC - 1))
			tp->RxDescArray[i].status =
			    (OWNbit | EORbit) | RX_BUF_SIZE;
		else
			tp->RxDescArray[i].status = OWNbit | RX_BUF_SIZE;

		tp->RxBufferRing[i] = &rxb[i * RX_BUF_SIZE];
		tp->RxDescArray[i].buf_addr =
		    virt_to_bus(tp->RxBufferRing[i]);
		tp->RxDescArray[i].buf_Haddr = 0;
	}
}

/**************************************************************************
RESET - Finish setting up the ethernet interface
***************************************************************************/
static void r8169_reset(struct nic *nic)
{
	int i;
	struct rtl8169_private *tp = &tpx;

	tp->TxDescArray = tx_ring;
	tp->RxDescArray = rx_ring;

	rtl8169_init_ring(nic);
	rtl8169_hw_start(nic);
	/* Construct a perfect filter frame with the mac address as first match
	 * and broadcast for all others */
	for (i = 0; i < 192; i++)
		txb[i] = 0xFF;

	txb[0] = nic->node_addr[0];
	txb[1] = nic->node_addr[1];
	txb[2] = nic->node_addr[2];
	txb[3] = nic->node_addr[3];
	txb[4] = nic->node_addr[4];
	txb[5] = nic->node_addr[5];
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void r8169_disable ( struct nic *nic __unused ) {
	int i;
	struct rtl8169_private *tp = &tpx;

	/* Stop the chip's Tx and Rx DMA processes. */
	RTL_W8(ChipCmd, 0x00);

	/* Disable interrupts by clearing the interrupt mask. */
	RTL_W16(IntrMask, 0x0000);

	RTL_W32(RxMissed, 0);

	tp->TxDescArray = NULL;
	tp->RxDescArray = NULL;
	for (i = 0; i < NUM_RX_DESC; i++) {
		tp->RxBufferRing[i] = NULL;
	}
}

static struct nic_operations r8169_operations = {
	.connect	= dummy_connect,
	.poll		= r8169_poll,
	.transmit	= r8169_transmit,
	.irq		= r8169_irq,

};

static struct pci_device_id r8169_nics[] = {
	PCI_ROM(0x10ec, 0x8169, "r8169", "RealTek RTL8169 Gigabit Ethernet"),
        PCI_ROM(0x16ec, 0x0116, "usr-r8169", "US Robotics RTL8169 Gigabit Ethernet"),
        PCI_ROM(0x1186, 0x4300, "dlink-r8169", "D-Link RTL8169 Gigabit Ethernet"),
	PCI_ROM(0x1737, 0x1032, "linksys-r8169", "Linksys RTL8169 Gigabit Ethernet"),
	PCI_ROM(0x10ec, 0x8129, "r8169-8129", "RealTek RT8129 Fast Ethernet Adapter"),
	PCI_ROM(0x10ec, 0x8136, "r8169-8101e", "RealTek RTL8101E PCI Express Fast Ethernet controller"),
	PCI_ROM(0x10ec, 0x8167, "r8169-8110sc/8169sc", "RealTek RTL-8110SC/8169SC Gigabit Ethernet"),
	PCI_ROM(0x10ec, 0x8168, "r8169-8168b", "RealTek RTL8111/8168B PCI Express Gigabit Ethernet controller"),
};

PCI_DRIVER ( r8169_driver, r8169_nics, PCI_NO_CLASS );

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/

#define board_found 1
#define valid_link 0
static int r8169_probe ( struct nic *nic, struct pci_device *pci ) {

	static int board_idx = -1;
	static int printed_version = 0;
	struct rtl8169_private *tp = &tpx;
	int i, rc;
	int option = -1, Cap10_100 = 0, Cap1000 = 0;

	printf ( "r8169.c: Found %s, Vendor=%hX Device=%hX\n",
	       pci->driver_name, pci->vendor, pci->device );

	board_idx++;

	printed_version = 1;

	/* Quick and very dirty hack to get r8169 driver working
	 * again, pre-rewrite
	 */
	if ( ! r8169_bufs )
		r8169_bufs = malloc_dma ( sizeof ( *r8169_bufs ), 256 );
	if ( ! r8169_bufs )
		return 0;
	memset ( r8169_bufs, 0, sizeof ( *r8169_bufs ) );

	rc = rtl8169_init_board(pci);	/* Return code is meaningless */

	/* Get MAC address.  FIXME: read EEPROM */
	for (i = 0; i < MAC_ADDR_LEN; i++)
		nic->node_addr[i] = RTL_R8(MAC0 + i);

	DBG ( "%s: Identified chip type is '%s'.\n", pci->driver_name,
		 rtl_chip_info[tp->chipset].name );

	/* Print out some hardware info */
	DBG ( "%s: %s at ioaddr %#hx, ", pci->driver_name, eth_ntoa ( nic->node_addr ),
	      (unsigned int) ioaddr );

	/* Config PHY */
	rtl8169_hw_phy_config(nic);
 
	DBG("Set MAC Reg C+CR Offset 0x82h = 0x01h\n");
	RTL_W8(0x82, 0x01);

	pci_write_config_byte(pci, PCI_LATENCY_TIMER, 0x40);

	if (tp->mac_version <= RTL_GIGA_MAC_VER_06)
		pci_write_config_byte(pci, PCI_CACHE_LINE_SIZE, 0x08);

	if (tp->mac_version == RTL_GIGA_MAC_VER_02) {
		DBG("Set MAC Reg C+CR Offset 0x82h = 0x01h\n");
		RTL_W8(0x82, 0x01);
		DBG("Set PHY Reg 0x0bh = 0x00h\n");
		RTL8169_WRITE_GMII_REG(ioaddr, 0x0b, 0x0000); //w 0x0b 15 0 0
	}

	r8169_reset(nic);

	/* if TBI is not endbled */
	if (!(RTL_R8(PHYstatus) & TBI_Enable)) {
		int val = RTL8169_READ_GMII_REG(ioaddr, PHY_AUTO_NEGO_REG);

#ifdef RTL8169_HW_FLOW_CONTROL_SUPPORT
		val |= PHY_Cap_PAUSE | PHY_Cap_ASYM_PAUSE;
#endif				//end #define RTL8169_HW_FLOW_CONTROL_SUPPORT

		/* Force RTL8169 in 10/100/1000 Full/Half mode. */
		if (option > 0) {
			printf(" Force-mode Enabled.\n");
			Cap10_100 = 0, Cap1000 = 0;
			switch (option) {
			case _10_Half:
				Cap10_100 = PHY_Cap_10_Half;
				Cap1000 = PHY_Cap_Null;
				break;
			case _10_Full:
				Cap10_100 = PHY_Cap_10_Full;
				Cap1000 = PHY_Cap_Null;
				break;
			case _100_Half:
				Cap10_100 = PHY_Cap_100_Half;
				Cap1000 = PHY_Cap_Null;
				break;
			case _100_Full:
				Cap10_100 = PHY_Cap_100_Full;
				Cap1000 = PHY_Cap_Null;
				break;
			case _1000_Full:
				Cap10_100 = PHY_Cap_Null;
				Cap1000 = PHY_Cap_1000_Full;
				break;
			default:
				break;
			}
			RTL8169_WRITE_GMII_REG(ioaddr, PHY_AUTO_NEGO_REG, Cap10_100 | (val & 0xC1F));	//leave PHY_AUTO_NEGO_REG bit4:0 unchanged
			RTL8169_WRITE_GMII_REG(ioaddr, PHY_1000_CTRL_REG,
					       Cap1000);
		} else {
			DBG ( "%s: Auto-negotiation Enabled.\n",  pci->driver_name );

			// enable 10/100 Full/Half Mode, leave PHY_AUTO_NEGO_REG bit4:0 unchanged
			RTL8169_WRITE_GMII_REG(ioaddr, PHY_AUTO_NEGO_REG,
					       PHY_Cap_10_Half |
					       PHY_Cap_10_Full |
					       PHY_Cap_100_Half |
					       PHY_Cap_100_Full | (val &
								   0xC1F));

			// enable 1000 Full Mode
//                     RTL8169_WRITE_GMII_REG( ioaddr, PHY_1000_CTRL_REG, PHY_Cap_1000_Full );
			RTL8169_WRITE_GMII_REG(ioaddr, PHY_1000_CTRL_REG, PHY_Cap_1000_Full | PHY_Cap_1000_Half);	//rtl8168

		}		// end of if( option > 0 )

		// Enable auto-negotiation and restart auto-nigotiation
		RTL8169_WRITE_GMII_REG(ioaddr, PHY_CTRL_REG,
				       PHY_Enable_Auto_Nego |
				       PHY_Restart_Auto_Nego);
		udelay(100);

		// wait for auto-negotiation process
		for (i = 10000; i > 0; i--) {
			//check if auto-negotiation complete
			if (RTL8169_READ_GMII_REG(ioaddr, PHY_STAT_REG) &
			    PHY_Auto_Neco_Comp) {
				udelay(100);
				option = RTL_R8(PHYstatus);
				if (option & _1000bpsF) {
					printf
					    ("1000Mbps Full-duplex operation.\n");
				} else {
					printf
					    ("%sMbps %s-duplex operation.\n",
					     (option & _100bps) ? "100" :
					     "10",
					     (option & FullDup) ? "Full" :
					     "Half");
				}
				break;
			} else {
				udelay(100);
			}	// end of if( RTL8169_READ_GMII_REG(ioaddr, 1) & 0x20 )
		}		// end for-loop to wait for auto-negotiation process


	} else {
		udelay(100);
		printf
		    ("%s: 1000Mbps Full-duplex operation, TBI Link %s!\n",
		     pci->driver_name,
		     (RTL_R32(TBICSR) & TBILinkOK) ? "OK" : "Failed");

	}

	r8169_reset(nic);

	/* point to NIC specific routines */
	nic->nic_op = &r8169_operations;

	nic->irqno  = pci->irq;
	nic->ioaddr = ioaddr;

	return 1;
}

//======================================================================================================
/*
static void rtl8169_hw_PHY_reset(struct nic *nic __unused)
{
        int val, phy_reset_expiretime = 50;
        struct rtl8169_private *priv = dev->priv;
        unsigned long ioaddr = priv->ioaddr;

        DBG("%s: Reset RTL8169s PHY\n", dev->name);

        val = ( RTL8169_READ_GMII_REG( ioaddr, 0 ) | 0x8000 ) & 0xffff;
        RTL8169_WRITE_GMII_REG( ioaddr, 0, val );

        do //waiting for phy reset
        {
                if( RTL8169_READ_GMII_REG( ioaddr, 0 ) & 0x8000 ){
                        phy_reset_expiretime --;
                        udelay(100);
                }
                else{
                        break;
                }
        }while( phy_reset_expiretime >= 0 );

        assert( phy_reset_expiretime > 0 );
}

*/

struct phy_reg {
	u16 reg;
	u16 val;
};

static void rtl_phy_write(void *ioaddr, struct phy_reg *regs, int len)
{
	while (len-- > 0) {
		RTL8169_WRITE_GMII_REG((u32)ioaddr, regs->reg, regs->val);
		regs++;
	}
}

static void rtl8169s_hw_phy_config(void *ioaddr)
{
	struct {
		u16 regs[5]; /* Beware of bit-sign propagation */
	} phy_magic[5] = { {
		{ 0x0000,	//w 4 15 12 0
		  0x00a1,	//w 3 15 0 00a1
		  0x0008,	//w 2 15 0 0008
		  0x1020,	//w 1 15 0 1020
		  0x1000 } },{	//w 0 15 0 1000
		{ 0x7000,	//w 4 15 12 7
		  0xff41,	//w 3 15 0 ff41
		  0xde60,	//w 2 15 0 de60
		  0x0140,	//w 1 15 0 0140
		  0x0077 } },{	//w 0 15 0 0077
		{ 0xa000,	//w 4 15 12 a
		  0xdf01,	//w 3 15 0 df01
		  0xdf20,	//w 2 15 0 df20
		  0xff95,	//w 1 15 0 ff95
		  0xfa00 } },{	//w 0 15 0 fa00
		{ 0xb000,	//w 4 15 12 b
		  0xff41,	//w 3 15 0 ff41
		  0xde20,	//w 2 15 0 de20
		  0x0140,	//w 1 15 0 0140
		  0x00bb } },{	//w 0 15 0 00bb
		{ 0xf000,	//w 4 15 12 f
		  0xdf01,	//w 3 15 0 df01
		  0xdf20,	//w 2 15 0 df20
		  0xff95,	//w 1 15 0 ff95
		  0xbf00 }	//w 0 15 0 bf00
		}
	}, *p = phy_magic;
	unsigned int i;

	RTL8169_WRITE_GMII_REG((u32)ioaddr, 0x1f, 0x0001);		//w 31 2 0 1
	RTL8169_WRITE_GMII_REG((u32)ioaddr, 0x15, 0x1000);		//w 21 15 0 1000
	RTL8169_WRITE_GMII_REG((u32)ioaddr, 0x18, 0x65c7);		//w 24 15 0 65c7
	RTL8169_WRITE_GMII_REG_BIT((u32)ioaddr, 4, 11, 0);	//w 4 11 11 0

	for (i = 0; i < ARRAY_SIZE(phy_magic); i++, p++) {
		int val, pos = 4;

		val = (RTL8169_READ_GMII_REG((u32)ioaddr, pos) & 0x0fff) | (p->regs[0] & 0xffff);
		RTL8169_WRITE_GMII_REG((u32)ioaddr, pos, val);
		while (--pos >= 0)
			RTL8169_WRITE_GMII_REG((u32)ioaddr, pos, p->regs[4 - pos] & 0xffff);
		RTL8169_WRITE_GMII_REG_BIT((u32)ioaddr, 4, 11, 1); //w 4 11 11 1
		RTL8169_WRITE_GMII_REG_BIT((u32)ioaddr, 4, 11, 0); //w 4 11 11 0
	}
	RTL8169_WRITE_GMII_REG((u32)ioaddr, 0x1f, 0x0000); //w 31 2 0 0
}

static void rtl8169sb_hw_phy_config(void *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0002 },
		{ 0x01, 0x90d0 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168cp_hw_phy_config(void *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0000 },
		{ 0x1d, 0x0f00 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x1ec8 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168c_hw_phy_config(void *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0001 },
		{ 0x12, 0x2300 },
		{ 0x1f, 0x0002 },
		{ 0x00, 0x88d4 },
		{ 0x01, 0x82b1 },
		{ 0x03, 0x7002 },
		{ 0x08, 0x9e30 },
		{ 0x09, 0x01f0 },
		{ 0x0a, 0x5500 },
		{ 0x0c, 0x00c8 },
		{ 0x1f, 0x0003 },
		{ 0x12, 0xc096 },
		{ 0x16, 0x000a },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8168cx_hw_phy_config(void *ioaddr)
{
	struct phy_reg phy_reg_init[] = {
		{ 0x1f, 0x0000 },
		{ 0x12, 0x2300 },
		{ 0x1f, 0x0003 },
		{ 0x16, 0x0f0a },
		{ 0x1f, 0x0000 },
		{ 0x1f, 0x0002 },
		{ 0x0c, 0x7eb8 },
		{ 0x1f, 0x0000 }
	};

	rtl_phy_write(ioaddr, phy_reg_init, ARRAY_SIZE(phy_reg_init));
}

static void rtl8169_hw_phy_config(struct nic *nic __unused)
{
	struct rtl8169_private *tp = &tpx;
	void *ioaddr = tp->mmio_addr;
	DBG("rtl8169_hw_phy_config(): card at addr=0x%lx: priv->mac_version=%d, priv->pcfg=%d\n", (unsigned long) ioaddr, tp->mac_version, tp->pcfg);

	switch (tp->mac_version) {
	case RTL_GIGA_MAC_VER_01:
		break;
	case RTL_GIGA_MAC_VER_02:
	case RTL_GIGA_MAC_VER_03:
		rtl8169s_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_04:
		rtl8169sb_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_18:
		rtl8168cp_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_19:
		rtl8168c_hw_phy_config(ioaddr);
		break;
	case RTL_GIGA_MAC_VER_20:
		rtl8168cx_hw_phy_config(ioaddr);
		break;
	default:
		break;
	}
}

DRIVER ( "r8169/PCI", nic_driver, pci_driver, r8169_driver,
	 r8169_probe, r8169_disable );

/*
 * Local variables:
 *  c-basic-offset: 8
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
