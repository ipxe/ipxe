/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
P2001 NIC driver for Etherboot
***************************************************************************/

/*
 *  Copyright (C) 2004 Tobias Lorenz
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
#include "lxt971a.h"
#include "timer.h"


/* NIC specific static variables go here */
static unsigned char MAC_HW_ADDR[6]={MAC_HW_ADDR_DRV};

/* DMA descriptors and buffers */
#define NUM_RX_DESC     4	/* Number of Rx descriptor registers. */
#define DMA_BUF_SIZE	2048	/* Buffer size */
static DMA_DSC txd              __attribute__ ((__section__(".dma.desc")));
static DMA_DSC rxd[NUM_RX_DESC] __attribute__ ((__section__(".dma.desc")));
static unsigned char rxb[NUM_RX_DESC * DMA_BUF_SIZE] __attribute__ ((__section__(".dma.buffer")));
static unsigned char txb[              DMA_BUF_SIZE] __attribute__ ((__section__(".dma.buffer")));
static unsigned int cur_rx;

/* Device selectors */
static unsigned int cur_channel;	// DMA channel    : 0..3
static unsigned int cur_phy;		// PHY Address    : 0..31
static P2001_ETH_regs_ptr EU;		// Ethernet Unit  : 0x0018_000 with _=0..3
static P2001_ETH_regs_ptr MU;		// Management Unit: 0x00180000

#define MDIO_MAXCOUNT 1000			/* mdio abort */
static unsigned int mdio_error;			/* mdio error */

/* Function prototypes */
static void         p2001_eth_mdio_init ();
static void         p2001_eth_mdio_write(unsigned int phyadr, unsigned int regadr, unsigned int data);
static unsigned int p2001_eth_mdio_read (unsigned int phyadr, unsigned int regadr);
extern unsigned int p2001_eth_mdio_error;

static int          p2001_eth_poll      (struct nic *nic, int retrieve);
static void         p2001_eth_transmit  (struct nic *nic, const char *d,
					unsigned int t, unsigned int s, const char *p);

static void         p2001_eth_irq       (struct nic *nic, irq_action_t action);

static void         p2001_eth_init      ();
static void         p2001_eth_disable   (struct dev *dev);

static int          p2001_eth_check_link(unsigned int phy);
static int          p2001_eth_probe     (struct dev *dev, unsigned short *probe_addrs __unused);


/**************************************************************************
PHY MANAGEMENT UNIT - Read/write
***************************************************************************/
static void p2001_eth_mdio_init()
{
	/* reset ethernet PHYs */
	printf("Resetting PHYs...\n");

	/* GPIO24/25: TX_ER2/TX_ER0 */
	/* GPIO26/27: PHY_RESET/TX_ER1 */
	P2001_GPIO->PIN_MUX |= 0x0018;
	// 31-16: 0000 1111 0000 0000
	P2001_GPIO->GPIO2_En |= 0x0400;

	P2001_GPIO->GPIO2_Out |= 0x04000000;
	P2001_GPIO->GPIO2_Out &= ~0x0400;
	mdelay(500);
	P2001_GPIO->GPIO2_Out |= 0x0400;

	/* set management unit clock divisor */
	// max. MDIO CLK = 2.048 MHz (EU.doc)
	// max. MDIO CLK = 8.000 MHz (LXT971A)
	// sysclk/(2*(n+1)) = MDIO CLK <= 2.048 MHz
	// n >= sysclk/4.096 MHz - 1
#if SYSCLK == 73728000
	P2001_MU->MU_DIV = 17;	// 73.728 MHZ =17=> 2.020 MHz
#else
	//MU->MU_DIV = (SYSCLK/4.096)-1;
#error "Please define a proper MDIO CLK divisor for that sysclk."
#endif
	asm("nop \n nop");
}

static void p2001_eth_mdio_write(unsigned int phyadr, unsigned int regadr, unsigned int data)
{
	static unsigned int count;
	count = 0;

	/* Warten bis Hardware inaktiv (MIU = "0") */
	while ((MU->MU_CNTL & 0x8000) && (count < MDIO_MAXCOUNT))
		count++;

	/* Schreiben MU_DATA */
	MU->MU_DATA = data;

	/* Schreiben MU_CNTL */
	MU->MU_CNTL = regadr + (phyadr<<5) + (1<<10);

	/* Warten bis Hardware aktiv (MIU = "1") */
	while (((MU->MU_CNTL & 0x8000) == 0) && (count < MDIO_MAXCOUNT))
		count++;
	//asm("nop \r\n nop");

	/* Warten bis Hardware inaktiv (MIU = "0") */
	while ((MU->MU_CNTL & 0x8000) && (count < MDIO_MAXCOUNT))
		count++;

	mdio_error = (count >= MDIO_MAXCOUNT);
}

static unsigned int p2001_eth_mdio_read(unsigned int phyadr, unsigned int regadr)
{
	static unsigned int count;
	count = 0;

	do {
		/* Warten bis Hardware inaktiv (MIU = "0") */
		while ((MU->MU_CNTL & 0x8000) && (count < MDIO_MAXCOUNT))
			count++;

		/* Schreiben MU_CNTL */
		MU->MU_CNTL = regadr + (phyadr<<5) + (2<<10);

		/* Warten bis Hardware aktiv (MIU = "1") */
		while (((MU->MU_CNTL & 0x8000) == 0) && (count < MDIO_MAXCOUNT))
			count++;
		//asm("nop \r\n nop");

		/* Warten bis Hardware inaktiv (MIU = "0") */
		while ((MU->MU_CNTL & 0x8000) && (count < MDIO_MAXCOUNT))
			count++;

		/* Fehler, wenn MDIO Read Error (MRE = "1") */
	} while ((MU->MU_CNTL & 0x4000) && (count < MDIO_MAXCOUNT));

	/* Lesen MU_DATA */
	mdio_error = (count >= MDIO_MAXCOUNT);
	return MU->MU_DATA;
}


/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
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
TRANSMIT - Transmit a frame
***************************************************************************/
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
	printf("p2001_eth_transmit: packet from %! to %! sent\n", txb+ETH_ALEN, txb);
#endif

	/* configure descriptor */
	txd.stat = (1<<31) | (1<<30) | (1<<29);	// DSC0 OWN|START|END
	txd.cntl = cur_channel << 16;		// DSC1 CHANNEL
	txd.cntl |= s;				// DSC1 LEN

	/* restart the transmitter */
	EU->TMAC_DMA_EN = 0x01;		/* set run bit */
	while(EU->TMAC_DMA_EN & 0x01) ;	/* wait */

#ifdef DEBUG_NIC
	/* check status */
	status = EU->TMAC_DMA_STAT;
	if (status & ~(0x40))
		printf("p2001_eth_transmit: dma status=0x%hx\n", status);

	printf("TMAC_MIB6..7: %d:%d\n", EU->TMAC_MIB6, EU->TMAC_MIB7);
#endif
}


/**************************************************************************
IRQ - Enable, Disable or Force Interrupts
***************************************************************************/
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
INIT - Initialize device
***************************************************************************/
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
	txd.buf = &txb;						// DSC2 BUFFER
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
	EU->TMAC_CNTL = (1<<4) |	/* COI: Collision ignore */
			//(1<<3) |	/* CSI: Carrier Sense ignore */
			(1<<2);		/* ATP: Automatic Transmit Padding */

	/* set receive mode */
	EU->RMAC_CNTL = (1<<3) |	/* BROAD: Broadcast packets */
			(1<<1);		/* PHY  : Packets to out MAC address */

	/* enable receiver */
	EU->RMAC_DMA_EN = 1;		/* set run bit */
}


/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
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
LINK - Check for valid link
***************************************************************************/
static int p2001_eth_check_link(unsigned int phy)
{
	static int status;
	static unsigned int count;
	count = 0;

	/* Use 0x3300 for restarting NWay */
	printf("Starting auto-negotiation... ");
	p2001_eth_mdio_write(phy, Adr_LXT971A_Control, 0x3300);
	if (mdio_error)
		goto failed;

	/* Bits 1.5 and 17.7 are set to 1 once the Auto-Negotiation process to completed. */
	do {
		mdelay(500);
		status = p2001_eth_mdio_read(phy, Adr_LXT971A_Status1);
		if (mdio_error || (count++ > 6))	// 6*500ms = 3s timeout
			goto failed;
	} while (!(status & 0x20));
	
	/* Bits 1.2 and 17.10 are set to 1 once the link is established. */
	if (p2001_eth_mdio_read(phy, Adr_LXT971A_Status1) & 0x04) {
		/* Bits 17.14 and 17.9 can be used to determine the link operation conditions (speed and duplex). */
		printf("Valid link, operating at: %sMb-%s\n",
			(p2001_eth_mdio_read(phy, Adr_LXT971A_Status2) & 0x4000) ? "100" : "10",
			(p2001_eth_mdio_read(phy, Adr_LXT971A_Status2) & 0x0200) ? "FD" : "HD");
			return 1;
	}

failed:
	if (mdio_error)
		printf("Failed\n");
	else
		printf("No valid link\n");
	return 0;
}


/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/
static int p2001_eth_probe(struct dev *dev, unsigned short *probe_addrs __unused)
{
	struct nic *nic = (struct nic *)dev;
	/* if probe_addrs is 0, then routine can use a hardwired default */
	static int board_found;
	static int valid_link;

	/* reset phys and configure mdio clk */
	p2001_eth_mdio_init();

	/* find the correct PHY/DMA/MAC combination */
	MU = P2001_MU;	// MU for all PHYs is only in EU0
	printf("Searching for P2001 NICs...\n");
	for (cur_channel=0; cur_channel<4; cur_channel++) {
		switch(cur_channel) {
			case 0:
				EU = P2001_EU0;
				cur_phy = 0;
				break;
			case 1:
				EU = P2001_EU1;
				cur_phy = 1;
				break;
			case 2:
				EU = P2001_EU2;
				cur_phy = 2;
				break;
			case 3:
				EU = P2001_EU3;
				cur_phy = 3;
				break;
		}

		/* first a non destructive test for initial value RMAC_TLEN=1518 */
		board_found = (EU->RMAC_TLEN == 1518);
		if (board_found) {
			printf("Checking EU%d...\n", cur_channel);

			valid_link = p2001_eth_check_link(cur_phy);
			if (valid_link) {
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
				dev->devid.vendor_id = htons(0x1234);
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
