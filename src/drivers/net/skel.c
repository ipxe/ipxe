/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Skeleton NIC driver for Etherboot
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"
/* to get the ISA support functions, if this is an ISA NIC */
#include "isa.h"

/* NIC specific static variables go here */

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int skel_poll(struct nic *nic, int retrieve)
{
	/* Work out whether or not there's an ethernet packet ready to
	 * read.  Return 0 if not.
	 */
	/* 
	   if ( ! <packet_ready> ) return 0;
	*/

	/* retrieve==0 indicates that we are just checking for the
	 * presence of a packet but don't want to read it just yet.
	 */
	/*
	  if ( ! retrieve ) return 1;
	*/

	/* Copy data to nic->packet.  Data should include the
	 * link-layer header (dest MAC, source MAC, type).
	 * Store length of data in nic->packetlen.
	 * Return true to indicate a packet has been read.
	 */
	/* 
	   nic->packetlen = <packet_length>;
	   memcpy ( nic->packet, <packet_data>, <packet_length> );
	   return 1;
	*/

	return 0;	/* Remove this line once this method is implemented */
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void skel_transmit(
	struct nic *nic,
	const char *dest,		/* Destination */
	unsigned int type,		/* Type */
	unsigned int size,		/* size */
	const char *packet)		/* Packet */
{
	/* Transmit packet to dest MAC address.  You will need to
	 * construct the link-layer header (dest MAC, source MAC,
	 * type).
	 */
	/*
	  unsigned int nstype = htons ( type );
	  memcpy ( <tx_buffer>, dest, ETH_ALEN );
	  memcpy ( <tx_buffer> + ETH_ALEN, nic->node_addr, ETH_ALEN );
	  memcpy ( <tx_buffer> + 2 * ETH_ALEN, &nstype, 2 );
	  memcpy ( <tx_buffer> + ETH_HLEN, data, size );
	  <transmit_data> ( <tx_buffer>, size + ETH_HLEN );
	 */
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void skel_disable(struct dev *dev)
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
}

/**************************************************************************
IRQ - handle interrupts
***************************************************************************/
static void skel_irq(struct nic *nic, irq_action_t action)
{
	/* This routine is somewhat optional.  Etherboot itself
	 * doesn't use interrupts, but they are required under some
	 * circumstances when we're acting as a PXE stack.
	 *
	 * If you don't implement this routine, the only effect will
	 * be that your driver cannot be used via Etherboot's UNDI
	 * API.  This won't affect programs that use only the UDP
	 * portion of the PXE API, such as pxelinux.
	 */
       
	switch ( action ) {
	case DISABLE :
	case ENABLE :
		/* Set receive interrupt enabled/disabled state */
		/*
		  outb ( action == ENABLE ? IntrMaskEnabled : IntrMaskDisabled,
		  	 nic->ioaddr + IntrMaskRegister );
		 */
		break;
	case FORCE :
		/* Force NIC to generate a receive interrupt */
		/*
		  outb ( ForceInterrupt, nic->ioaddr + IntrForceRegister );
		 */
		break;
	}
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/

#define board_found 1
#define valid_link 0
static int skel_probe(struct dev *dev, struct pci_device *pci)
{
	struct nic *nic = (struct nic *)dev;

	if (board_found && valid_link)
	{
		/* store NIC parameters */
		nic->ioaddr = pci->ioaddr & ~3;
		nic->irqno = pci->irq;
		/* point to NIC specific routines */
		dev->disable  = skel_disable;
		nic->poll     = skel_poll;
		nic->transmit = skel_transmit;
		nic->irq      = skel_irq;
		return 1;
	}
	/* else */
	return 0;
}

static struct pci_id skel_nics[] = {
PCI_ROM(0x0000, 0x0000, "skel-pci", "Skeleton PCI Adaptor"),
};

static struct pci_driver skel_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "SKELETON/PCI",
	.probe    = skel_probe,
	.ids      = skel_nics,
	.id_count = sizeof(skel_nics)/sizeof(skel_nics[0]),
	.class    = 0,
};

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/
static int skel_isa_probe(struct dev *dev, unsigned short *probe_addrs)
{
	struct nic *nic = (struct nic *)dev;
	/* if probe_addrs is 0, then routine can use a hardwired default */
	if (board_found && valid_link)
	{
		/* point to NIC specific routines */
		dev->disable  = skel_disable;
		nic->poll     = skel_poll;
		nic->transmit = skel_transmit;

		/* Report the ISA pnp id of the board */
		dev->devid.vendor_id = htons(GENERIC_ISAPNP_VENDOR);
		dev->devid.vendor_id = htons(0x1234);
		return 1;
	}
	/* else */
	return 0;
}

ISA_ROM("skel-isa", "Skeleton ISA driver")
static struct isa_driver skel_isa_driver __isa_driver = {
	.type    = NIC_DRIVER,
	.name    = "SKELETON/ISA",
	.probe   = skel_isa_probe,
	.ioaddrs = 0,
};

