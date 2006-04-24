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
#include <gpxe/pci.h>
#include "isa.h"
#include "eisa.h"
#include "isapnp.h"
#include "mca.h"

/*
 * NIC specific static variables go here.  Try to avoid using static
 * variables wherever possible.  In particular, the I/O address can
 * always be accessed via nic->ioaddr.
 */

/*
 * If you have large static variables (e.g. transmit and receive
 * buffers), you should place them together in a single structure and
 * mark the structure as "shared".  This enables this space to be
 * shared between drivers in multi-driver images, which can easily
 * reduce the runtime size by 50%.
 *
 */
#define SKEL_RX_BUFS	1
#define SKEL_TX_BUFS	1
#define SKEL_RX_BUFSIZE	0
#define SKEL_TX_BUFSIZE 0
struct skel_rx_desc {};
struct skel_tx_desc {};
struct {
	struct skel_rx_desc	rxd[SKEL_RX_BUFS];
	unsigned char		rxb[SKEL_RX_BUFS][SKEL_RX_BUFSIZE];
	struct skel_tx_desc	txd[SKEL_TX_BUFS];
	unsigned char		txb[SKEL_TX_BUFS][SKEL_TX_BUFSIZE];
} skel_bufs __shared;

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
	.transmit	= skel_transmit,
	.poll		= skel_poll,
	.irq		= skel_irq,
};

/**************************************************************************
 * PROBE - Look for an adapter
 *
 * You need to define a probe routine and a disable routine for each
 * bus type that your driver supports, together with tables that
 * enable Etherboot to identify that your driver should be used for a
 * particular device.
 *
 * Delete whichever of the following sections you don't need.  For
 * example, most PCI devices will only need the PCI probing section;
 * ISAPnP, EISA, etc. can all be deleted.
 *
 * Some devices will need custom bus logic.  The ISA 3c509 is a good
 * example of this; it has a contention-resolution mechanism that is
 * similar to ISAPnP, but not close enough to use the generic ISAPnP
 * code.  Look at 3c509.c to see how it works.
 *
 **************************************************************************
 */

/**************************************************************************
 * PCI PROBE and DISABLE
 **************************************************************************
 */
static int skel_pci_probe ( struct nic *nic, struct pci_device *pci ) {

	pci_fill_nic ( nic, pci );

	/* Test for physical presence of NIC */
	/*
	   if ( ! my_tests ) {
	  	DBG ( "Could not find NIC: my explanation\n" );
		return 0;
	   }
	*/

	/* point to NIC specific routines */
	nic->nic_op = &skel_operations;
	return 1;
}

static void skel_pci_disable ( struct nic *nic __unused,
			       struct pci_device *pci __unused ) {
	/* Reset the card to its initial state, disable DMA and
	 * interrupts
	 */
}

static struct pci_id skel_pci_nics[] = {
PCI_ROM ( 0x0000, 0x0000, "skel-pci", "Skeleton PCI Adapter" ),
};

PCI_DRIVER ( skel_pci_driver, skel_pci_nics, PCI_NO_CLASS );

DRIVER ( "SKEL/PCI", nic_driver, pci_driver, skel_pci_driver,
	 skel_pci_probe, skel_pci_disable );

/**************************************************************************
 * EISA PROBE and DISABLE
 **************************************************************************
 */
static int skel_eisa_probe ( struct nic *nic, struct eisa_device *eisa ) {

	eisa_fill_nic ( nic, eisa );
	enable_eisa_device ( eisa );
	nic->irqno = 0; /* No standard way to get irq from EISA cards */

	/* Test for physical presence of NIC */
	/*
	   if ( ! my_tests ) {
	  	DBG ( "Could not find NIC: my explanation\n" );
		return 0;
	   }
	*/

	/* point to NIC specific routines */
	nic->nic_op = &skel_operations;
	return 1;
}

static void skel_eisa_disable ( struct nic *nic __unused,
				struct eisa_device *eisa ) {
	/* Reset the card to its initial state, disable DMA and
	 * interrupts
	 */
	disable_eisa_device ( eisa );
}

static struct eisa_id skel_eisa_nics[] = {
	{ "Skeleton EISA Adapter", EISA_VENDOR('S','K','L'), 0x0000 },
};

EISA_DRIVER ( skel_eisa_driver, skel_eisa_nics );

DRIVER ( "SKEL/EISA", nic_driver, eisa_driver, skel_eisa_driver,
	 skel_eisa_probe, skel_eisa_disable );

ISA_ROM ( "skel-eisa", "Skeleton EISA Adapter" );

/**************************************************************************
 * ISAPnP PROBE and DISABLE
 **************************************************************************
 */
static int skel_isapnp_probe ( struct nic *nic,
			       struct isapnp_device *isapnp ) {

	isapnp_fill_nic ( nic, isapnp );
	activate_isapnp_device ( isapnp );

	/* Test for physical presence of NIC */
	/*
	   if ( ! my_tests ) {
	  	DBG ( "Could not find NIC: my explanation\n" );
		return 0;
	   }
	*/

	/* point to NIC specific routines */
	nic->nic_op = &skel_operations;
	return 1;
}

static void skel_isapnp_disable ( struct nic *nic __unused,
				  struct isapnp_device *isapnp ) {
	/* Reset the card to its initial state, disable DMA and
	 * interrupts
	 */
	deactivate_isapnp_device ( isapnp );
}

static struct isapnp_id skel_isapnp_nics[] = {
	{ "Skeleton ISAPnP Adapter", ISAPNP_VENDOR('S','K','L'), 0x0000 },
};

ISAPNP_DRIVER ( skel_isapnp_driver, skel_isapnp_nics );

DRIVER ( "SKEL/ISAPnP", nic_driver, isapnp_driver, skel_isapnp_driver,
	 skel_isapnp_probe, skel_isapnp_disable );

ISA_ROM ( "skel-isapnp", "Skeleton ISAPnP Adapter" );

/**************************************************************************
 * MCA PROBE and DISABLE
 **************************************************************************
 */
static int skel_mca_probe ( struct nic *nic,
			    struct mca_device *mca ) {

	mca_fill_nic ( nic, mca );

	/* MCA parameters are available in the mca->pos[] array */
	/*
	   nic->ioaddr = ( mca->pos[xxx] << 8 ) + mca->pos[yyy];
	   nic->irqno = mca->pos[zzz] & 0x0f;
	*/

	/* Test for physical presence of NIC */
	/*
	   if ( ! my_tests ) {
	  	DBG ( "Could not find NIC: my explanation\n" );
		return 0;
	   }
	*/

	/* point to NIC specific routines */
	nic->nic_op = &skel_operations;
	return 1;
}

static void skel_mca_disable ( struct nic *nic __unused,
			       struct mca_device *mca __unused ) {
	/* Reset the card to its initial state, disable DMA and
	 * interrupts
	 */
}

static struct mca_id skel_mca_nics[] = {
	{ "Skeleton MCA Adapter", 0x0000 },
};

MCA_DRIVER ( skel_mca_driver, skel_mca_nics );

DRIVER ( "SKEL/MCA", nic_driver, mca_driver, skel_mca_driver,
	 skel_mca_probe, skel_mca_disable );

ISA_ROM ( "skel-mca", "Skeleton MCA Adapter" );

/**************************************************************************
 * ISA PROBE and DISABLE
 *
 * The "classical" ISA probe is split into two stages: trying a list
 * of I/O addresses to see if there's anything listening, and then
 * using that I/O address to fill in the information in the nic
 * structure.
 *
 * The list of probe addresses defined in skel_isa_probe_addrs[] will
 * be passed to skel_isa_probe_addr().  If skel_isa_probe_addr()
 * returns true, a struct isa_device will be created with isa->ioaddr
 * set to the working I/O address, and skel_isa_probe() will be
 * called.
 *
 * There is a standard mechanism for overriding the probe address list
 * using ISA_PROBE_ADDRS.  Do not implement any custom code to
 * override the probe address list.
 *
 **************************************************************************
 */
static int skel_isa_probe_addr ( isa_probe_addr_t ioaddr __unused ) {
	return 0;
}

static int skel_isa_probe ( struct nic *nic, struct isa_device *isa ) {

	isa_fill_nic ( nic, isa );
	nic->irqno = 0; /* No standard way to get IRQ for ISA */

	/* Test for physical presence of NIC */
	/*
	   if ( ! my_tests ) {
	  	DBG ( "Could not find NIC: my explanation\n" );
		return 0;
	   }
	*/

	/* point to NIC specific routines */
	nic->nic_op = &skel_operations;
	return 1;
}

static void skel_isa_disable ( struct nic *nic __unused,
			      struct isa_device *isa __unused ) {
	/* Reset the card to its initial state, disable DMA and
	 * interrupts
	 */
}

static isa_probe_addr_t skel_isa_probe_addrs[] = {
	/*
	   0x200, 0x240,
	*/
};

ISA_DRIVER ( skel_isa_driver, skel_isa_probe_addrs, skel_isa_probe_addr,
		     ISA_VENDOR('S','K','L'), 0x0000 );

DRIVER ( "SKEL/ISA", nic_driver, isa_driver, skel_isa_driver,
	 skel_isa_probe, skel_isa_disable );

ISA_ROM ( "skel-isa", "Skeleton ISA Adapter" );

