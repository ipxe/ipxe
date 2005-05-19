/**************************************************************************
 * Etherboot -  BOOTP/TFTP Bootstrap Program
 * P2001 NIC driver for Etherboot
 **************************************************************************/

/*
 *  Copyright (C) 2005 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the ISA support functions, if this is an ISA NIC */
#include "isa.h"

#include "hardware.h"
#include "mii.h"
#include "timer.h"


/* NIC specific static variables go here */
static unsigned char MAC_HW_ADDR[6]={MAC_HW_ADDR_DRV};

/* DMA descriptors and buffers */
#define NUM_RX_DESC     4	/* Number of Rx descriptor registers. */
#define DMA_BUF_SIZE	2048	/* Buffer size */
static DMA_DSC txd              __attribute__ ((__section__(".dma.desc")));
static DMA_DSC rxd[NUM_RX_DESC] __attribute__ ((__section__(".dma.desc")));
static char rxb[NUM_RX_DESC * DMA_BUF_SIZE] __attribute__ ((__section__(".dma.buffer")));
static char txb[              DMA_BUF_SIZE] __attribute__ ((__section__(".dma.buffer")));
static unsigned int cur_rx;

/* Device selectors */
static unsigned int cur_channel;	// DMA channel    : 0..3
static unsigned int cur_phy;		// PHY Address    : 0..31
static P2001_ETH_regs_ptr EU;		// Ethernet Unit  : 0x0018_000 with _=0..3

/* mdio handling */
static int          p2001_eth_mdio_read (int phy_id, int location);
static void         p2001_eth_mdio_write(int phy_id, int location, int val);

/* net_device functions */
static int          p2001_eth_poll      (struct nic *nic, int retrieve);
static void         p2001_eth_transmit  (struct nic *nic, const char *d,
					unsigned int t, unsigned int s, const char *p);

static void         p2001_eth_irq       (struct nic *nic, irq_action_t action);

static void         p2001_eth_init      ();
static void         p2001_eth_disable   (struct dev *dev);

static int          p2001_eth_check_link(unsigned int phy);
static int          link;
static void         p2001_eth_phyreset  ();
static int          p2001_eth_probe     (struct dev *dev, unsigned short *probe_addrs __unused);

/* Supported MII list */
static struct mii_chip_info {
	const char * name;
	unsigned int physid;	// (MII_PHYSID2 << 16) | MII_PHYSID1
} mii_chip_table[] = {
	{ "Intel LXT971A",	0x78e20013 },
	{ "Altima AC104-QF",	0x55410022 },
	{NULL,0},
};



/**************************************************************************
 * PHY MANAGEMENT UNIT - Read/write
 **************************************************************************/

/**
 *	mdio_read - read MII PHY register
 *	@dev: the net device to read
 *	@regadr: the phy register id to read
 *
 *	Read MII registers through MDIO and MDC
 *	using MDIO management frame structure and protocol(defined by ISO/IEC).
 */
static int p2001_eth_mdio_read(int phy_id, int location)
{
	int result, boguscnt = 1000;

	do {
		/* Warten bis Hardware inaktiv (MIU = "0") */
		while (P2001_MU->MU_CNTL & 0x8000)
			barrier();

		/* Schreiben MU_CNTL */
		P2001_MU->MU_CNTL = location + (phy_id<<5) + (2<<10);

		/* Warten bis Hardware aktiv (MIU = "1") */
		while ((P2001_MU->MU_CNTL & 0x8000) == 0)
			barrier();
		//asm("nop \r\n nop");

		/* Warten bis Hardware inaktiv (MIU = "0") */
		while (P2001_MU->MU_CNTL & 0x8000)
			barrier();

		/* Fehler, wenn MDIO Read Error (MRE = "1") */
	} while ((P2001_MU->MU_CNTL & 0x4000) && (--boguscnt > 0));

	/* Lesen MU_DATA */
	result = P2001_MU->MU_DATA;

	if (boguscnt == 0)
		return 0;
	if ((result & 0xffff) == 0xffff)
		return 0;

	return result & 0xffff;
}


/**
 *	mdio_write - write MII PHY register
 *	@dev: the net device to write
 *	@regadr: the phy register id to write
 *	@value: the register value to write with
 *
 *	Write MII registers with @value through MDIO and MDC
 *	using MDIO management frame structure and protocol(defined by ISO/IEC)
 */
static void p2001_eth_mdio_write(int phy_id, int location, int val)
{
	/* Warten bis Hardware inaktiv (MIU = "0") */
	while (P2001_MU->MU_CNTL & 0x8000)
		barrier();

	/* Schreiben MU_DATA */
	P2001_MU->MU_DATA = val;

	/* Schreiben MU_CNTL */
	P2001_MU->MU_CNTL = location + (phy_id<<5) + (1<<10);

	/* Warten bis Hardware aktiv (MIU = "1") */
	while ((P2001_MU->MU_CNTL & 0x8000) == 0)
		barrier();
	//asm("nop \r\n nop");

	/* Warten bis Hardware inaktiv (MIU = "0") */
	while (P2001_MU->MU_CNTL & 0x8000)
		barrier();
}



/**************************************************************************
 * POLL - Wait for a frame
 **************************************************************************/

/* Function: p2001_eth_poll
 *
 * Description: checks for a received packet and returns it if found.
 *
 * Arguments: struct nic *nic:          NIC data structure
 *
 * Returns:   1 if a packet was received.
 *            0 if no pacet was received.
 *
 * Side effects:
 *            Returns (copies) the packet to the array nic->packet.
 *            Returns the length of the packet in nic->packetlen.
 */
static int p2001_eth_poll(struct nic *nic, int retrieve)
{
	/* return true if there's an ethernet packet ready to read */
	/* nic->packet should contain data on return */
	/* nic->packetlen should contain length of data */

	int retstat = 0;

	if (rxd[cur_rx].stat & (1<<31))	// OWN
		return retstat;

	if (!retrieve)
		return 1;

	nic->packetlen = rxd[cur_rx].cntl & 0xffff;

	if (rxd[cur_rx].stat & ((1<<26)|(1<<25)|(1<<24)|(1<<23)|(1<<22))) {
		/* corrupted packet received */
		printf("p2001_eth_poll: Corrupted packet received, stat = %X\n",
		               rxd[cur_rx].stat);
		retstat = 0;
	} else {
		/* give packet to higher routine */
		memcpy(nic->packet, (rxb + cur_rx*DMA_BUF_SIZE), nic->packetlen);
		retstat = 1;
	}

#ifdef DEBUG_NIC
	printf("p2001_eth_poll: packet from %! to %! received\n", 
		(rxb+cur_rx*DMA_BUF_SIZE)+ETH_ALEN,
		(rxb+cur_rx*DMA_BUF_SIZE));
#endif

	/* disable receiver */
	// FIXME: is that ok? it can produce grave errors.
	EU->RMAC_DMA_EN = 0;				/* clear run bit */

	/* return the descriptor and buffer to receive ring */
	rxd[cur_rx].stat = (1<<31) | (1<<30) | (1<<29);	// DSC0 OWN|START|END
	rxd[cur_rx].cntl = (1<<23);			// DSC1 RECEIVE
	rxd[cur_rx].cntl |= cur_channel << 16;		// DSC1 CHANNEL
	rxd[cur_rx].cntl |= DMA_BUF_SIZE;		// DSC1 LEN

	if (++cur_rx == NUM_RX_DESC)
        	cur_rx = 0;

	/* enable receiver */
	if (!(EU->RMAC_DMA_EN & 0x01))
		EU->RMAC_DMA_EN = 0x01;			/* set run bit */

#ifdef DEBUG_NIC
	printf("RMAC_MIB0..5: %d:%d:%d:%d:%d:%d\n",
		EU->RMAC_MIB0, EU->RMAC_MIB1,
		EU->RMAC_MIB2, EU->RMAC_MIB3,
		EU->RMAC_MIB4, EU->RMAC_MIB5);
#endif

	return retstat;	/* initially as this is called to flush the input */
}



/**************************************************************************
 * TRANSMIT - Transmit a frame
 **************************************************************************/

/* Function: p2001_eth_transmit
 *
 * Description: transmits a packet and waits for completion or timeout.
 *
 * Arguments: char d[6]:          destination ethernet address.
 *            unsigned short t:   ethernet protocol type.
 *            unsigned short s:   size of the data-part of the packet.
 *            char *p:            the data for the packet.
 *    
 * Returns:   void.
 */
static void p2001_eth_transmit(
	struct nic *nic __unused,
	const char *d,			/* Destination */
	unsigned int t,			/* Type */
	unsigned int s,			/* size */
	const char *p)			/* Packet */
{
	unsigned int nstype;
#ifdef DEBUG_NIC
	unsigned int status;
#endif

	/* assemble packet */
	memcpy(txb, d, ETH_ALEN);			// destination
	memcpy(txb+ETH_ALEN, nic->node_addr, ETH_ALEN);	// source
	nstype = htons(t);
	memcpy(txb+2*ETH_ALEN, (char*)&nstype, 2);	// type
	memcpy(txb+ETH_HLEN, p, s);			// packet
	s += ETH_HLEN;

	/* pad to minimum packet size */
//	while (s<ETH_ZLEN)
//		txb[s++] = '\0';
	// TMAC_CNTL.ATP does the same

#ifdef DEBUG_NIC
	printf("p2001_eth_transmit: packet from %! to %! sent (size: %d)\n", txb+ETH_ALEN, txb, s);
#endif

	/* configure descriptor */
	txd.stat = (1<<31) | (1<<30) | (1<<29);	// DSC0 OWN|START|END
	txd.cntl = cur_channel << 16;		// DSC1 CHANNEL
	txd.cntl |= s;				// DSC1 LEN

	/* restart the transmitter */
	EU->TMAC_DMA_EN = 0x01;		/* set run bit */
	while(EU->TMAC_DMA_EN & 0x01);	/* wait */

#ifdef DEBUG_NIC
	/* check status */
	status = EU->TMAC_DMA_STAT;
	if (status & ~(0x40))	// not END
		printf("p2001_eth_transmit: dma status=0x%hx\n", status);

	printf("TMAC_MIB6..7: %d:%d\n", EU->TMAC_MIB6, EU->TMAC_MIB7);
#endif
}



/**************************************************************************
 * IRQ - Enable, Disable or Force Interrupts
 **************************************************************************/

/* Function: p2001_eth_irq
 *
 * Description: Enable, Disable, or Force, interrupts
 *    
 * Arguments: struct nic *nic:          NIC data structure
 *            irq_action_t action:      Requested action       
 *
 * Returns:   void.
 */

static void
p2001_eth_irq(struct nic *nic __unused, irq_action_t action __unused)
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
 * INIT - Initialize device
 **************************************************************************/

/* Function: p2001_init
 *
 * Description: resets the ethernet controller chip and various
 *    data structures required for sending and receiving packets.
 *    
 * returns:   void.
 */
static void p2001_eth_init()
{
	static int i;

	/* activate MII 3 */
	if (cur_channel == 3)
		P2001_GPIO->PIN_MUX |= (1<<8);	// MII_3_en = 1

#ifdef RMII
	/* RMII init sequence */
	if (link & LPA_100) {
		EU->CONF_RMII = (1<<2) | (1<<1);		// softres | 100Mbit
		EU->CONF_RMII = (1<<2) | (1<<1) | (1<<0);	// softres | 100Mbit | RMII
		EU->CONF_RMII = (1<<1) | (1<<0);		// 100 Mbit | RMII
	} else {
		EU->CONF_RMII = (1<<2);				// softres
		EU->CONF_RMII = (1<<2) | (1<<0);		// softres | RMII
		EU->CONF_RMII = (1<<0);				// RMII
	}
#endif

	/* disable transceiver */
//	EU->TMAC_DMA_EN = 0;		/* clear run bit */
//	EU->RMAC_DMA_EN = 0;		/* clear run bit */

	/* set rx filter (physical mac addresses) */
	EU->RMAC_PHYU =
		(MAC_HW_ADDR[0]<< 8) +
		(MAC_HW_ADDR[1]<< 0);
	EU->RMAC_PHYL =
		(MAC_HW_ADDR[2]<<24) +
		(MAC_HW_ADDR[3]<<16) +
		(MAC_HW_ADDR[4]<<8 ) +
		(MAC_HW_ADDR[5]<<0 );

	/* initialize the tx descriptor ring */
//	txd.stat = (1<<31) | (1<<30) | (1<<29);			// DSC0 OWN|START|END
//	txd.cntl = cur_channel << 16;				// DSC1 CHANNEL
//	txd.cntl |= DMA_BUF_SIZE;				// DSC1 LEN
	txd.buf = (char *)&txb;					// DSC2 BUFFER
	txd.next = &txd;					// DSC3 NEXTDSC @self
	EU->TMAC_DMA_DESC = &txd;

	/* initialize the rx descriptor ring */
	cur_rx = 0;
	for (i = 0; i < NUM_RX_DESC; i++) {
		rxd[i].stat = (1<<31) | (1<<30) | (1<<29);	// DSC0 OWN|START|END
		rxd[i].cntl = (1<<23);				// DSC1 RECEIVE
		rxd[i].cntl |= cur_channel << 16;		// DSC1 CHANNEL
		rxd[i].cntl |= DMA_BUF_SIZE;			// DSC1 LEN
		rxd[i].buf = &rxb[i*DMA_BUF_SIZE];		// DSC2 BUFFER (EU-RX data)
		rxd[i].next = &rxd[i+1];			// DSC3 NEXTDSC @next
	}
	rxd[NUM_RX_DESC-1].next = &rxd[0];			// DSC3 NEXTDSC @first
	EU->RMAC_DMA_DESC = &rxd[0];

	/* set transmitter mode */
	if (link & LPA_DUPLEX)
		EU->TMAC_CNTL =	(1<<4) |	/* COI: Collision ignore */
				(1<<3) |	/* CSI: Carrier Sense ignore */
				(1<<2);		/* ATP: Automatic Transmit Padding */
	else
		EU->TMAC_CNTL =	(1<<2);		/* ATP: Automatic Transmit Padding */

	/* set receive mode */
	EU->RMAC_CNTL = (1<<3) |	/* BROAD: Broadcast packets */
			(1<<1);		/* PHY  : Packets to out MAC address */

	/* enable receiver */
	EU->RMAC_DMA_EN = 1;		/* set run bit */
}



/**************************************************************************
 * DISABLE - Turn off ethernet interface
 **************************************************************************/
static void p2001_eth_disable(struct dev *dev __unused)
{
	/* put the card in its initial state */
	/* This function serves 3 purposes.
	 * This disables DMA and interrupts so we don't receive
	 *  unexpected packets or interrupts from the card after
	 *  etherboot has finished. 
	 * This frees resources so etherboot may use
	 *  this driver on another interface
	 * This allows etherboot to reinitialize the interface
	 *  if something is something goes wrong.
	 */

	/* disable transmitter */
	EU->TMAC_DMA_EN = 0;		/* clear run bit */

	/* disable receiver */
	EU->RMAC_DMA_EN = 0;		/* clear run bit */
}



/**************************************************************************
 * LINK - Check for valid link
 **************************************************************************/
static int p2001_eth_check_link(unsigned int phy)
{
	static int status;
	static unsigned int i, physid;

	/* print some information about out PHY */
	physid = (p2001_eth_mdio_read(phy, MII_PHYSID2) << 16) |
		  p2001_eth_mdio_read(phy, MII_PHYSID1);
	printf("PHY %d, ID 0x%x ", phy, physid);
	for (i = 0; mii_chip_table[i].physid; i++)
		if (mii_chip_table[i].physid == physid) {
			printf("(%s).\n", mii_chip_table[i].name);
			break;
		}
	if (!mii_chip_table[i].physid)
		printf("(unknown).\n");

	/* Use 0x3300 for restarting NWay */
	printf("Starting auto-negotiation... ");
	p2001_eth_mdio_write(phy, MII_BMCR, 0x3300);

	/* Bit 1.5 is set once the Auto-Negotiation process is completed. */
	i = 0;
	do {
		mdelay(500);
		status = p2001_eth_mdio_read(phy, MII_BMSR);
		if (!status || (i++ > 6))	// 6*500ms = 3s timeout
			goto failed;
	} while (!(status & BMSR_ANEGCOMPLETE));

	/* Bits 1.2 is set once the link is established. */
	if ((status = p2001_eth_mdio_read(phy, MII_BMSR)) & BMSR_LSTATUS) {
		link = p2001_eth_mdio_read(phy, MII_ADVERTISE) &
		       p2001_eth_mdio_read(phy, MII_LPA);
		printf("  Valid link, operating at: %sMb-%s\n",
			(link & LPA_100) ? "100" : "10",
			(link & LPA_DUPLEX) ? "FD" : "HD");
		return 1;
	}

failed:
	if (!status)
		printf("Failed\n");
	else
		printf("No valid link\n");
	return 0;
}



/**************************************************************************
 * PHYRESET - hardware reset all MII PHYs
 **************************************************************************/

/**
 *	p2001_eth_phyreset - hardware reset all MII PHYs
 */
static void p2001_eth_phyreset()
{
	/* GPIO24/25: TX_ER2/TX_ER0 */
	/* GPIO26/27: PHY_RESET/TX_ER1 */
	P2001_GPIO->PIN_MUX |= 0x0018;
	// 31-16: 0000 1111 0000 0000
	P2001_GPIO->GPIO2_En |= 0x0400;

	P2001_GPIO->GPIO2_Out |= 0x04000000;
	P2001_GPIO->GPIO2_Out &= ~0x0400;
	mdelay(500);
	P2001_GPIO->GPIO2_Out |= 0x0400;

#ifdef RMII
	/* RMII_clk_sel = 0xxb  no RMII (default) */
	/* RMII_clk_sel = 100b	COL_0 */
	/* RMII_clk_sel = 101b	COL_1 */
	/* RMII_clk_sel = 110b	COL_2 */
	/* RMII_clk_sel = 111b	COL_3 */
	P2001_GPIO->PIN_MUX |= (4 << 13);
#endif
}



/**************************************************************************
 * PROBE - Look for an adapter, this routine's visible to the outside
 **************************************************************************/

static int p2001_eth_probe(struct dev *dev, unsigned short *probe_addrs __unused)
{
	struct nic *nic = (struct nic *)dev;
	/* if probe_addrs is 0, then routine can use a hardwired default */

	/* reset phys and configure mdio clk */
	printf("Resetting PHYs...\n");
	p2001_eth_phyreset();

	/* set management unit clock divisor */
	// max. MDIO CLK = 2.048 MHz (EU.doc)
	P2001_MU->MU_DIV = (SYSCLK/4096000)-1;	// 2.048 MHz
	//asm("nop \n nop");

	/* find the correct PHY/DMA/MAC combination */
	printf("Searching for P2001 NICs...\n");
	cur_phy = -1;
	for (cur_channel=0; cur_channel<4; cur_channel++) {
		EU = P2001_EU(cur_channel);

		/* find next phy */
		while (++cur_phy < 16) {
			//printf("phy detect %d\n", cur_phy);
			if (p2001_eth_mdio_read(cur_phy, MII_BMSR) != 0)
				break;
		}
		if (cur_phy == 16) {
			printf("no more MII PHYs found\n");
			break;
		}

		/* first a non destructive test for initial value RMAC_TLEN=1518 */
		if (EU->RMAC_TLEN == 1518) {
			printf("Checking EU%d...\n", cur_channel);

			if (p2001_eth_check_link(cur_phy)) {
				/* initialize device */
				p2001_eth_init(nic);

				/* set node address */
				printf("Setting MAC address to %!\n", MAC_HW_ADDR);
				memcpy(nic->node_addr, MAC_HW_ADDR, 6);

				/* point to NIC specific routines */
				dev->disable  = p2001_eth_disable;
				nic->poll     = p2001_eth_poll;
				nic->transmit = p2001_eth_transmit;
				nic->irq      = p2001_eth_irq;

				/* Report the ISA pnp id of the board */
				dev->devid.vendor_id = htons(GENERIC_ISAPNP_VENDOR);
				return 1;
			}
		}
	}
	/* else */
	return 0;
}

ISA_ROM("p2001_eth", "P2001 Ethernet Driver")
static struct isa_driver p2001_eth_driver __isa_driver = {
	.type    = NIC_DRIVER,
	.name    = "P2001 Ethernet Driver",
	.probe   = p2001_eth_probe,
	.ioaddrs = 0,
};
