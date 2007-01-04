/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <pxe.h>
#include <realmode.h>
#include <pic8259.h>
#include <biosint.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>
#include <gpxe/if_ether.h>
#include <gpxe/ethernet.h>

/** @file
 *
 * UNDI network device driver
 *
 */

/*****************************************************************************
 *
 * UNDI interrupt service routine
 *
 *****************************************************************************
 */

/**
 * UNDI interrupt service routine
 *
 * The UNDI ISR simply increments a counter (@c trigger_count) and
 * exits.
 */
extern void undi_isr ( void );

/** Vector for chaining to other interrupts handlers */
static struct segoff __text16 ( undi_isr_chain );
#define undi_isr_chain __use_text16 ( undi_isr_chain )

/** IRQ trigger count */
static volatile uint16_t __text16 ( trigger_count );
#define trigger_count __use_text16 ( trigger_count )

/**
 * Hook UNDI interrupt service routine
 *
 * @v irq		IRQ number
 */
static void undi_hook_isr ( unsigned int irq ) {
	__asm__ __volatile__ ( TEXT16_CODE ( "\nundi_isr:\n\t"
					     "incl %%cs:%c0\n\t"
					     "ljmp *%%cs:%c1\n\t" )
			       : : "p" ( & __from_text16 ( trigger_count ) ),
			           "p" ( & __from_text16 ( undi_isr_chain ) ));

	hook_bios_interrupt ( IRQ_INT ( irq ), ( ( unsigned int ) undi_isr ),
			      &undi_isr_chain );

}

/**
 * Unhook UNDI interrupt service routine
 *
 * @v irq		IRQ number
 */
static void undi_unhook_isr ( unsigned int irq ) {
	unhook_bios_interrupt ( IRQ_INT ( irq ), ( ( unsigned int ) undi_isr ),
				&undi_isr_chain );
}

/**
 * Test to see if UNDI ISR has been triggered
 *
 * @ret triggered	ISR has been triggered since last check
 */
static int undi_isr_triggered ( void ) {
	static unsigned int last_trigger_count;
	unsigned int this_trigger_count;

	/* Read trigger_count.  Do this only once; it is volatile */
	this_trigger_count = trigger_count;

	if ( this_trigger_count == last_trigger_count ) {
		/* Not triggered */
		return 0;
	} else {
		/* Triggered */
		last_trigger_count = this_trigger_count;
		return 1;
	}
}

/*****************************************************************************
 *
 * UNDI network device interface
 *
 *****************************************************************************
 */

/** Maximum length of a packet transmitted via the UNDI API */
#define UNDI_PKB_LEN 1514

/** A packet transmitted via the UNDI API */
struct undi_packet {
	uint8_t bytes[UNDI_PKB_LEN];
};

/** UNDI packet buffer */
static struct undi_packet __data16 ( undi_pkb );
#define undi_pkb __use_data16 ( undi_pkb )

/** UNDI transmit buffer descriptor */
static struct s_PXENV_UNDI_TBD __data16 ( undi_tbd );
#define undi_tbd __use_data16 ( undi_tbd )

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 */
static int undi_transmit ( struct net_device *netdev, struct pk_buff *pkb ) {
	struct pxe_device *pxe = netdev->priv;
	struct s_PXENV_UNDI_TRANSMIT undi_transmit;
	size_t len = pkb_len ( pkb );
	int rc;

	/* Copy packet to UNDI packet buffer */
	if ( len > sizeof ( undi_pkb ) )
		len = sizeof ( undi_pkb );
	memcpy ( &undi_pkb, pkb->data, len );

	/* Create PXENV_UNDI_TRANSMIT data structure */
	memset ( &undi_transmit, 0, sizeof ( undi_transmit ) );
	undi_transmit.DestAddr.segment = rm_ds;
	undi_transmit.DestAddr.offset
		= ( ( unsigned ) & __from_data16 ( undi_tbd ) );
	undi_transmit.TBD.segment = rm_ds;
	undi_transmit.TBD.offset
		= ( ( unsigned ) & __from_data16 ( undi_tbd ) );

	/* Create PXENV_UNDI_TBD data structure */
	undi_tbd.ImmedLength = len;
	undi_tbd.Xmit.segment = rm_ds;
	undi_tbd.Xmit.offset 
		= ( ( unsigned ) & __from_data16 ( undi_pkb ) );

	/* Issue PXE API call */
	if ( ( rc = pxe_call ( pxe, PXENV_UNDI_TRANSMIT, &undi_transmit,
			       sizeof ( undi_transmit ) ) ) != 0 ) {
		DBG ( "UNDI_TRANSMIT failed: %s\n", strerror ( rc ) );
	}

	/* Free packet buffer and return */
	free_pkb ( pkb );
	return rc;
}

/** 
 * Poll for received packets
 *
 * @v netdev	Network device
 *
 * Fun, fun, fun.  UNDI drivers don't use polling; they use
 * interrupts.  We therefore cheat and pretend that an interrupt has
 * occurred every time undi_poll() is called.  This isn't too much of
 * a hack; PCI devices share IRQs and so the first thing that a proper
 * ISR should do is call PXENV_UNDI_ISR to determine whether or not
 * the UNDI NIC generated the interrupt; there is no harm done by
 * spurious calls to PXENV_UNDI_ISR.  Similarly, we wouldn't be
 * handling them any more rapidly than the usual rate of undi_poll()
 * being called even if we did implement a full ISR.  So it should
 * work.  Ha!
 *
 * Addendum (21/10/03).  Some cards don't play nicely with this trick,
 * so instead of doing it the easy way we have to go to all the hassle
 * of installing a genuine interrupt service routine and dealing with
 * the wonderful 8259 Programmable Interrupt Controller.  Joy.
 */
static void undi_poll ( struct net_device *netdev ) {
	struct pxe_device *pxe = netdev->priv;
	struct s_PXENV_UNDI_ISR undi_isr;
	struct pk_buff *pkb = NULL;
	size_t len;
	size_t frag_len;
	int rc;

	/* Do nothing unless ISR has been triggered */
	if ( ! undi_isr_triggered() )
		return;

	/* See if this was our interrupt */
	memset ( &undi_isr, 0, sizeof ( undi_isr ) );
	undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_START;
	if ( ( rc = pxe_call ( pxe, PXENV_UNDI_ISR, &undi_isr,
			       sizeof ( undi_isr ) ) ) != 0 ) {
		DBG ( "UNDI_ISR (START) failed: %s\n", strerror ( rc ) );
		return;
	}
	if ( undi_isr.FuncFlag != PXENV_UNDI_ISR_OUT_OURS )
		return;

	/* Send EOI */
	send_eoi ( pxe->irq );

	/* Run through the ISR loop */
	undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_PROCESS;
	while ( 1 ) {
		if ( ( rc = pxe_call ( pxe, PXENV_UNDI_ISR, &undi_isr,
				       sizeof ( undi_isr ) ) ) != 0 ) {
			DBG ( "UNDI_ISR (PROCESS/GET_NEXT) failed: %s\n",
			      strerror ( rc ) );
			break;
		}
		switch ( undi_isr.FuncFlag ) {
		case PXENV_UNDI_ISR_OUT_TRANSMIT:
			/* We don't care about transmit completions */
			break;
		case PXENV_UNDI_ISR_OUT_RECEIVE:
			/* Packet fragment received */
			len = undi_isr.FrameLength;
			frag_len = undi_isr.BufferLength;
			if ( ! pkb )
				pkb = alloc_pkb ( len );
			if ( ! pkb ) {
				DBG ( "UNDI could not allocate %zd bytes for "
				      "receive buffer\n", len );
				break;
			}
			if ( frag_len > pkb_available ( pkb ) ) {
				DBG ( "UNDI fragment too large\n" );
				frag_len = pkb_available ( pkb );
			}
			copy_from_real ( pkb_put ( pkb, frag_len ),
					 undi_isr.Frame.segment,
					 undi_isr.Frame.offset, frag_len );
			if ( pkb_len ( pkb ) == len ) {
				netdev_rx ( netdev, pkb );
				pkb = NULL;
			}
			break;
		case PXENV_UNDI_ISR_OUT_DONE:
			/* Processing complete */
			goto done;
		default:
			/* Should never happen */
			DBG ( "UNDI ISR returned invalid FuncFlag %04x\n",
			      undi_isr.FuncFlag );
			goto done;
		}
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
	}

 done:
	if ( pkb ) {
		DBG ( "UNDI returned incomplete packet\n" );
		netdev_rx ( netdev, pkb );
	}
}

/**
 * Open NIC
 *
 * @v netdev		Net device
 * @ret rc		Return status code
 */
static int undi_open ( struct net_device *netdev ) {
	struct pxe_device *pxe = netdev->priv;
	struct s_PXENV_UNDI_SET_STATION_ADDRESS set_address;
	struct s_PXENV_UNDI_OPEN open;
	int rc;

	/* Hook interrupt service routine */
	undi_hook_isr ( pxe->irq );

	/* Set station address.  Required for some PXE stacks; will
	 * spuriously fail on others.  Ignore failures.  We only ever
	 * use it to set the MAC address to the card's permanent value
	 * anyway.
	 */
	memcpy ( set_address.StationAddress, netdev->ll_addr,
		 sizeof ( set_address.StationAddress ) );
	if ( ( rc = pxe_call ( pxe, PXENV_UNDI_SET_STATION_ADDRESS,
			       &set_address, sizeof ( set_address ) ) ) != 0 ){
		DBG ( "UNDI_SET_STATION_ADDRESS failed: %s\n",
		      strerror ( rc ) );
	}

	/* Open NIC */
	memset ( &open, 0, sizeof ( open ) );
	open.PktFilter = ( FLTR_DIRECTED | FLTR_BRDCST );
	if ( ( rc = pxe_call ( pxe, PXENV_UNDI_OPEN, &open,
			       sizeof ( open ) ) ) != 0 ) {
		DBG ( "UNDI_OPEN failed: %s\n", strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Close NIC
 *
 * @v netdev		Net device
 */
static void undi_close ( struct net_device *netdev ) {
	struct pxe_device *pxe = netdev->priv;
	struct s_PXENV_UNDI_CLOSE close;
	int rc;

	/* Close NIC */
	if ( ( rc = pxe_call ( pxe, PXENV_UNDI_CLOSE, &close,
			       sizeof ( close ) ) ) != 0 ) {
		DBG ( "UNDI_CLOSE failed: %s\n", strerror ( rc ) );
	}

	/* Unhook ISR */
	undi_unhook_isr ( pxe->irq );
}

/**
 * Probe PXE device
 *
 * @v pxe		PXE device
 * @ret rc		Return status code
 */
int undi_probe ( struct pxe_device *pxe ) {
	struct net_device *netdev;
	int rc;

	/* Allocate net device */
	netdev = alloc_etherdev ( 0 );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err;
	}
	netdev->priv = pxe;
	pxe_set_drvdata ( pxe, netdev );

	/* Fill in NIC information */
	memcpy ( netdev->ll_addr, pxe->hwaddr, ETH_ALEN );

	/* Point to NIC specific routines */
	netdev->open	 = undi_open;
	netdev->close	 = undi_close;
	netdev->transmit = undi_transmit;
	netdev->poll	 = undi_poll;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err;

	return 0;

 err:
	free_netdev ( netdev );
	return rc;
}

/**
 * Remove PXE device
 *
 * @v pxe		PXE device
 */
void undi_remove ( struct pxe_device *pxe ) {
	struct net_device *netdev = pxe_get_drvdata ( pxe );

	unregister_netdev ( netdev );
	free_netdev ( netdev );
}
