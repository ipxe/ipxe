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
/* Drag in support for whichever bus(es) we want for this NIC */
#include "pci.h"
#include "isa.h"
#include "eisa.h"
#include "isapnp.h"
#include "mca.h"

/* NIC specific static variables go here.  Try to avoid using static
 * variables wherever possible.  In particular, the I/O address can
 * always be accessed via nic->ioaddr.
 */

/*
 * Don't forget to remove "__unused" from all the function parameters!
 *
 */

/**************************************************************************
 * CONNECT - Connect to the network
 **************************************************************************
*/
static int skel_connect ( struct nic *nic __unused ) {
	/*
	 * Connect to the network.  For most NICs, this will probably
	 * be a no-op.  For wireless NICs, this should be the point at
	 * which you attempt to join to an access point.
	 *
	 * Return 0 if the connection failed (e.g. no cable plugged
	 * in), 1 for success.
	 *
	 */
	return 1;
}

/**************************************************************************
 * POLL - Wait for a frame
 **************************************************************************
*/
static int skel_poll ( struct nic *nic __unused, int retrieve __unused ) {
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
 * TRANSMIT - Transmit a frame
 **************************************************************************
*/
static void skel_transmit ( struct nic *nic __unused,
			    const char *dest __unused,
			    unsigned int type __unused,
			    unsigned int size __unused,
			    const char *packet __unused ) {
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
 * DISABLE - Turn off ethernet interface
 **************************************************************************
 */
static void skel_disable ( struct nic *nic __unused ) {
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
 * IRQ - handle interrupts
 **************************************************************************
*/
static void skel_irq ( struct nic *nic __unused, irq_action_t action ) {
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
 * OPERATIONS TABLE - Pointers to all the above methods
 **************************************************************************
 */
static struct nic_operations skel_operations = {
	.connect	= skel_connect,
	.poll		= skel_poll,
	.transmit	= skel_transmit,
	.irq		= skel_irq,
	.disable	= skel_disable,
};

/**************************************************************************
 * PROBE - Look for an adapter
 *
 * You need to define a probe routine for each bus type that your
 * driver supports, together with tables that enable Etherboot to
 * identify that your driver should be used for a particular device.
 *
 * Delete whichever of the following sections you don't need.  For
 * example, most PCI devices will only need the PCI probing section;
 * ISAPnP, EISA, etc. can all be deleted.
 *
 **************************************************************************
 */

/**************************************************************************
 * PCI PROBE - Look for an adapter
 **************************************************************************
 */
static int skel_pci_probe ( struct dev *dev, struct pci_device *pci ) {
	struct nic *nic = nic_device ( dev );

	/* store NIC parameters */
	nic->ioaddr = pci->ioaddr;
	nic->irqno = pci->irq;

	/* Test for physical presence of NIC */
	/*
	  if ( ! my_tests ) {
	  	DBG ( "Could not find NIC: my explanation\n" );
		return 0;
	  }
	*/

	/* point to NIC specific routines */
	nic->nic_op	= &skel_operations;
	return 1;
}

static struct pci_id skel_pci_nics[] = {
PCI_ROM ( 0x0000, 0x0000, "skel-pci", "Skeleton PCI Adaptor" ),
};

static struct pci_driver skel_pci_driver =
	PCI_DRIVER ( "SKELETON/PCI", skel_pci_nics, PCI_NO_CLASS );

BOOT_DRIVER ( "SKELETON/PCI", find_pci_boot_device,
	      skel_pci_driver, skel_pci_probe );

