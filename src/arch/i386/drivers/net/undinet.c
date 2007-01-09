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
#include <pnpbios.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>
#include <gpxe/if_ether.h>
#include <gpxe/ethernet.h>
#include <undi.h>
#include <undinet.h>


/** @file
 *
 * UNDI network device driver
 *
 */

/** An UNDI NIC */
struct undi_nic {
	/** Entry point */
	SEGOFF16_t entry;
	/** Assigned IRQ number */
	unsigned int irq;
	/** Currently processing ISR */
	int isr_processing;
};

static void undinet_close ( struct net_device *netdev );

/*****************************************************************************
 *
 * UNDI API call
 *
 *****************************************************************************
 */

/**
 * Name UNDI API call
 *
 * @v function		API call number
 * @ret name		API call name
 */
static inline __attribute__ (( always_inline )) const char *
undinet_function_name ( unsigned int function ) {
	switch ( function ) {
	case PXENV_START_UNDI:
		return "PXENV_START_UNDI";
	case PXENV_STOP_UNDI:
		return "PXENV_STOP_UNDI";
	case PXENV_UNDI_STARTUP:
		return "PXENV_UNDI_STARTUP";
	case PXENV_UNDI_CLEANUP:
		return "PXENV_UNDI_CLEANUP";
	case PXENV_UNDI_INITIALIZE:
		return "PXENV_UNDI_INITIALIZE";
	case PXENV_UNDI_RESET_ADAPTER:
		return "PXENV_UNDI_RESET_ADAPTER";
	case PXENV_UNDI_SHUTDOWN:
		return "PXENV_UNDI_SHUTDOWN";
	case PXENV_UNDI_OPEN:
		return "PXENV_UNDI_OPEN";
	case PXENV_UNDI_CLOSE:
		return "PXENV_UNDI_CLOSE";
	case PXENV_UNDI_TRANSMIT:
		return "PXENV_UNDI_TRANSMIT";
	case PXENV_UNDI_SET_MCAST_ADDRESS:
		return "PXENV_UNDI_SET_MCAST_ADDRESS";
	case PXENV_UNDI_SET_STATION_ADDRESS:
		return "PXENV_UNDI_SET_STATION_ADDRESS";
	case PXENV_UNDI_SET_PACKET_FILTER:
		return "PXENV_UNDI_SET_PACKET_FILTER";
	case PXENV_UNDI_GET_INFORMATION:
		return "PXENV_UNDI_GET_INFORMATION";
	case PXENV_UNDI_GET_STATISTICS:
		return "PXENV_UNDI_GET_STATISTICS";
	case PXENV_UNDI_CLEAR_STATISTICS:
		return "PXENV_UNDI_CLEAR_STATISTICS";
	case PXENV_UNDI_INITIATE_DIAGS:
		return "PXENV_UNDI_INITIATE_DIAGS";
	case PXENV_UNDI_FORCE_INTERRUPT:
		return "PXENV_UNDI_FORCE_INTERRUPT";
	case PXENV_UNDI_GET_MCAST_ADDRESS:
		return "PXENV_UNDI_GET_MCAST_ADDRESS";
	case PXENV_UNDI_GET_NIC_TYPE:
		return "PXENV_UNDI_GET_NIC_TYPE";
	case PXENV_UNDI_GET_IFACE_INFO:
		return "PXENV_UNDI_GET_IFACE_INFO";
	/*
	 * Duplicate case value; this is a bug in the PXE specification.
	 *
	 *	case PXENV_UNDI_GET_STATE:
	 *		return "PXENV_UNDI_GET_STATE";
	 */
	case PXENV_UNDI_ISR:
		return "PXENV_UNDI_ISR";
	default:
		return "UNKNOWN API CALL";
	}
}

/**
 * UNDI parameter block
 *
 * Used as the paramter block for all UNDI API calls.  Resides in base
 * memory.
 */
static union u_PXENV_ANY __data16 ( undinet_params );
#define undinet_params __use_data16 ( undinet_params )

/** UNDI entry point
 *
 * Used as the indirection vector for all UNDI API calls.  Resides in
 * base memory.
 */
static SEGOFF16_t __data16 ( undinet_entry_point );
#define undinet_entry_point __use_data16 ( undinet_entry_point )

/**
 * Issue UNDI API call
 *
 * @v undinic		UNDI NIC
 * @v function		API call number
 * @v params		UNDI parameter block
 * @v params_len	Length of UNDI parameter block
 * @ret rc		Return status code
 */
static int undinet_call ( struct undi_nic *undinic, unsigned int function,
			  void *params, size_t params_len ) {
	union u_PXENV_ANY *pxenv_any = params;
	PXENV_EXIT_t exit;
	int discard_b, discard_D;
	int rc;

	/* Copy parameter block and entry point */
	assert ( params_len <= sizeof ( undinet_params ) );
	memcpy ( &undinet_params, params, params_len );
	undinet_entry_point = undinic->entry;

	/* Call real-mode entry point.  This calling convention will
	 * work with both the !PXE and the PXENV+ entry points.
	 */
	__asm__ __volatile__ ( REAL_CODE ( "pushw %%es\n\t"
					   "pushw %%di\n\t"
					   "pushw %%bx\n\t"
					   "lcall *%c3\n\t"
					   "addw $6, %%sp\n\t" )
			       : "=a" ( exit ), "=b" ( discard_b ),
			         "=D" ( discard_D )
			       : "p" ( &__from_data16 ( undinet_entry_point )),
			         "b" ( function ),
			         "D" ( &__from_data16 ( undinet_params ) )
			       : "ecx", "edx", "esi", "ebp" );

	/* UNDI API calls may rudely change the status of A20 and not
	 * bother to restore it afterwards.  Intel is known to be
	 * guilty of this.
	 *
	 * Note that we will return to this point even if A20 gets
	 * screwed up by the UNDI driver, because Etherboot always
	 * resides in an even megabyte of RAM.
	 */	
	gateA20_set();

	/* Copy parameter block back */
	memcpy ( params, &undinet_params, params_len );

	/* Determine return status code based on PXENV_EXIT and
	 * PXENV_STATUS
	 */
	if ( exit == PXENV_EXIT_SUCCESS ) {
		rc = 0;
	} else {
		rc = -pxenv_any->Status;
		/* Paranoia; don't return success for the combination
		 * of PXENV_EXIT_FAILURE but PXENV_STATUS_SUCCESS
		 */
		if ( rc == 0 )
			rc = -EIO;
	}

	if ( rc != 0 ) {
		DBGC ( undinic, "UNDINIC %p %s failed: %s\n", undinic,
		       undinet_function_name ( function ), strerror ( rc ) );
	}
	return rc;
}

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
extern void undinet_isr ( void );

/** Dummy chain vector */
static struct segoff prev_handler[ IRQ_MAX + 1 ];

/** IRQ trigger count */
static volatile uint8_t __text16 ( trigger_count ) = 0;
#define trigger_count __use_text16 ( trigger_count )

/**
 * Hook UNDI interrupt service routine
 *
 * @v irq		IRQ number
 *
 * The UNDI ISR specifically does @b not chain to the previous
 * interrupt handler.  BIOSes seem to install somewhat perverse
 * default interrupt handlers; some do nothing other than an iret (and
 * so will cause a screaming interrupt if there really is another
 * interrupting device) and some disable the interrupt at the PIC (and
 * so will bring our own interrupts to a shuddering halt).
 */
static void undinet_hook_isr ( unsigned int irq ) {

	assert ( irq <= IRQ_MAX );

	__asm__ __volatile__ ( TEXT16_CODE ( "\nundinet_isr:\n\t"
					     "incb %%cs:%c0\n\t"
					     "iret\n\t" )
			       : : "p" ( & __from_text16 ( trigger_count ) ) );

	hook_bios_interrupt ( IRQ_INT ( irq ),
			      ( ( unsigned int ) undinet_isr ),
			      &prev_handler[irq] );

}

/**
 * Unhook UNDI interrupt service routine
 *
 * @v irq		IRQ number
 */
static void undinet_unhook_isr ( unsigned int irq ) {

	assert ( irq <= IRQ_MAX );

	unhook_bios_interrupt ( IRQ_INT ( irq ),
				( ( unsigned int ) undinet_isr ),
				&prev_handler[irq] );
}

/**
 * Test to see if UNDI ISR has been triggered
 *
 * @ret triggered	ISR has been triggered since last check
 */
static int undinet_isr_triggered ( void ) {
	static unsigned int last_trigger_count = 0;
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
static struct undi_packet __data16 ( undinet_pkb );
#define undinet_pkb __use_data16 ( undinet_pkb )

/** UNDI transmit buffer descriptor */
static struct s_PXENV_UNDI_TBD __data16 ( undinet_tbd );
#define undinet_tbd __use_data16 ( undinet_tbd )

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 */
static int undinet_transmit ( struct net_device *netdev,
			      struct pk_buff *pkb ) {
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_TRANSMIT undi_transmit;
	size_t len = pkb_len ( pkb );
	int rc;

	/* Copy packet to UNDI packet buffer */
	if ( len > sizeof ( undinet_pkb ) )
		len = sizeof ( undinet_pkb );
	memcpy ( &undinet_pkb, pkb->data, len );

	/* Create PXENV_UNDI_TRANSMIT data structure */
	memset ( &undi_transmit, 0, sizeof ( undi_transmit ) );
	undi_transmit.DestAddr.segment = rm_ds;
	undi_transmit.DestAddr.offset
		= ( ( unsigned ) & __from_data16 ( undinet_tbd ) );
	undi_transmit.TBD.segment = rm_ds;
	undi_transmit.TBD.offset
		= ( ( unsigned ) & __from_data16 ( undinet_tbd ) );

	/* Create PXENV_UNDI_TBD data structure */
	undinet_tbd.ImmedLength = len;
	undinet_tbd.Xmit.segment = rm_ds;
	undinet_tbd.Xmit.offset 
		= ( ( unsigned ) & __from_data16 ( undinet_pkb ) );

	/* Issue PXE API call */
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_TRANSMIT,
				   &undi_transmit,
				   sizeof ( undi_transmit ) ) ) != 0 )
		goto done;

	/* Free packet buffer */
	netdev_tx_complete ( netdev, pkb );

 done:
	return rc;
}

/** 
 * Poll for received packets
 *
 * @v netdev	Network device
 *
 * Fun, fun, fun.  UNDI drivers don't use polling; they use
 * interrupts.  We therefore cheat and pretend that an interrupt has
 * occurred every time undinet_poll() is called.  This isn't too much
 * of a hack; PCI devices share IRQs and so the first thing that a
 * proper ISR should do is call PXENV_UNDI_ISR to determine whether or
 * not the UNDI NIC generated the interrupt; there is no harm done by
 * spurious calls to PXENV_UNDI_ISR.  Similarly, we wouldn't be
 * handling them any more rapidly than the usual rate of
 * undinet_poll() being called even if we did implement a full ISR.
 * So it should work.  Ha!
 *
 * Addendum (21/10/03).  Some cards don't play nicely with this trick,
 * so instead of doing it the easy way we have to go to all the hassle
 * of installing a genuine interrupt service routine and dealing with
 * the wonderful 8259 Programmable Interrupt Controller.  Joy.
 */
static void undinet_poll ( struct net_device *netdev ) {
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_ISR undi_isr;
	struct pk_buff *pkb = NULL;
	size_t len;
	size_t frag_len;
	int rc;

	if ( ! undinic->isr_processing ) {
		/* Do nothing unless ISR has been triggered */
		if ( ! undinet_isr_triggered() )
			return;
		
		/* See if this was our interrupt */
		memset ( &undi_isr, 0, sizeof ( undi_isr ) );
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_START;
		if ( ( rc = undinet_call ( undinic, PXENV_UNDI_ISR, &undi_isr,
					   sizeof ( undi_isr ) ) ) != 0 )
			return;
		if ( undi_isr.FuncFlag != PXENV_UNDI_ISR_OUT_OURS )
			return;
		
		/* Send EOI */
		send_eoi ( undinic->irq );

		/* Start ISR processing */
		undinic->isr_processing = 1;
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_PROCESS;
	} else {
		/* Continue ISR processing */
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
	}

	/* Run through the ISR loop */
	while ( 1 ) {
		if ( ( rc = undinet_call ( undinic, PXENV_UNDI_ISR, &undi_isr,
					   sizeof ( undi_isr ) ) ) != 0 )
			break;
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
				DBGC ( undinic, "UNDINIC %p could not "
				       "allocate %zd bytes for RX buffer\n",
				       undinic, len );
				/* Fragment will be dropped */
				goto done;
			}
			if ( frag_len > pkb_available ( pkb ) ) {
				DBGC ( undinic, "UNDINIC %p fragment too "
				       "large\n", undinic );
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
			undinic->isr_processing = 0;
			goto done;
		default:
			/* Should never happen */
			DBGC ( undinic, "UNDINIC %p ISR returned invalid "
			       "FuncFlag %04x\n", undinic, undi_isr.FuncFlag );
			undinic->isr_processing = 0;
			goto done;
		}
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
	}

 done:
	if ( pkb ) {
		DBGC ( undinic, "UNDINIC %p returned incomplete packet\n",
		       undinic );
		netdev_rx ( netdev, pkb );
	}
}

/**
 * Open NIC
 *
 * @v netdev		Net device
 * @ret rc		Return status code
 */
static int undinet_open ( struct net_device *netdev ) {
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_SET_STATION_ADDRESS set_address;
	struct s_PXENV_UNDI_OPEN open;
	int rc;

	/* Hook interrupt service routine and enable interrupt */
	undinet_hook_isr ( undinic->irq );
	enable_irq ( undinic->irq );
	send_eoi ( undinic->irq );

	/* Set station address.  Required for some PXE stacks; will
	 * spuriously fail on others.  Ignore failures.  We only ever
	 * use it to set the MAC address to the card's permanent value
	 * anyway.
	 */
	memcpy ( set_address.StationAddress, netdev->ll_addr,
		 sizeof ( set_address.StationAddress ) );
	undinet_call ( undinic, PXENV_UNDI_SET_STATION_ADDRESS,
		       &set_address, sizeof ( set_address ) );

	/* Open NIC */
	memset ( &open, 0, sizeof ( open ) );
	open.PktFilter = ( FLTR_DIRECTED | FLTR_BRDCST );
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_OPEN, &open,
				   sizeof ( open ) ) ) != 0 )
		goto err;

	return 0;

 err:
	undinet_close ( netdev );
	return rc;
}

/**
 * Close NIC
 *
 * @v netdev		Net device
 */
static void undinet_close ( struct net_device *netdev ) {
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_CLOSE close;

	/* Ensure ISR has exited cleanly */
	while ( undinic->isr_processing )
		undinet_poll ( netdev );

	/* Close NIC */
	undinet_call ( undinic, PXENV_UNDI_CLOSE, &close, sizeof ( close ) );

	/* Disable interrupt and unhook ISR */
	disable_irq ( undinic->irq );
	undinet_unhook_isr ( undinic->irq );
}

/**
 * Probe UNDI device
 *
 * @v undi		UNDI device
 * @ret rc		Return status code
 */
int undinet_probe ( struct undi_device *undi ) {
	struct net_device *netdev;
	struct undi_nic *undinic;
	struct s_PXENV_START_UNDI start_undi;
	struct s_PXENV_UNDI_STARTUP undi_startup;
	struct s_PXENV_UNDI_INITIALIZE undi_initialize;
	struct s_PXENV_UNDI_GET_INFORMATION undi_info;
	struct s_PXENV_UNDI_SHUTDOWN undi_shutdown;
	struct s_PXENV_UNDI_CLEANUP undi_cleanup;
	struct s_PXENV_STOP_UNDI stop_undi;
	int rc;

	/* Allocate net device */
	netdev = alloc_etherdev ( sizeof ( *undinic ) );
	if ( ! netdev )
		return -ENOMEM;
	undinic = netdev->priv;
	undi_set_drvdata ( undi, netdev );
	memset ( undinic, 0, sizeof ( *undinic ) );
	undinic->entry = undi->entry;
	DBGC ( undinic, "UNDINIC %p using UNDI %p\n", undinic, undi );

	/* Hook in UNDI stack */
	memset ( &start_undi, 0, sizeof ( start_undi ) );
	start_undi.AX = undi->pci_busdevfn;
	start_undi.BX = undi->isapnp_csn;
	start_undi.DX = undi->isapnp_read_port;
	start_undi.ES = BIOS_SEG;
	start_undi.DI = find_pnp_bios();
	if ( ( rc = undinet_call ( undinic, PXENV_START_UNDI, &start_undi,
				   sizeof ( start_undi ) ) ) != 0 )
		goto err_start_undi;

	/* Bring up UNDI stack */
	memset ( &undi_startup, 0, sizeof ( undi_startup ) );
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_STARTUP, &undi_startup,
				   sizeof ( undi_startup ) ) ) != 0 )
		goto err_undi_startup;
	memset ( &undi_initialize, 0, sizeof ( undi_initialize ) );
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_INITIALIZE,
				   &undi_initialize,
				   sizeof ( undi_initialize ) ) ) != 0 )
		goto err_undi_initialize;

	/* Get device information */
	memset ( &undi_info, 0, sizeof ( undi_info ) );
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_GET_INFORMATION,
				   &undi_info, sizeof ( undi_info ) ) ) != 0 )
		goto err_undi_get_information;
	memcpy ( netdev->ll_addr, undi_info.PermNodeAddress, ETH_ALEN );
	undinic->irq = undi_info.IntNumber;
	if ( undinic->irq > IRQ_MAX ) {
		DBGC ( undinic, "UNDINIC %p invalid IRQ %d\n",
		       undinic, undinic->irq );
		goto err_bad_irq;
	}
	DBGC ( undinic, "UNDINIC %p is %s on IRQ %d\n",
	       undinic, eth_ntoa ( netdev->ll_addr ), undinic->irq );

	/* Point to NIC specific routines */
	netdev->open	 = undinet_open;
	netdev->close	 = undinet_close;
	netdev->transmit = undinet_transmit;
	netdev->poll	 = undinet_poll;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;

	return 0;

 err_register:
 err_bad_irq:
 err_undi_get_information:
 err_undi_initialize:
	/* Shut down UNDI stack */
	memset ( &undi_shutdown, 0, sizeof ( undi_shutdown ) );
	undinet_call ( undinic, PXENV_UNDI_SHUTDOWN, &undi_shutdown,
		       sizeof ( undi_shutdown ) );
	memset ( &undi_cleanup, 0, sizeof ( undi_cleanup ) );
	undinet_call ( undinic, PXENV_UNDI_CLEANUP, &undi_cleanup,
		       sizeof ( undi_cleanup ) );
 err_undi_startup:
	/* Unhook UNDI stack */
	memset ( &stop_undi, 0, sizeof ( stop_undi ) );
	undinet_call ( undinic, PXENV_STOP_UNDI, &stop_undi,
		       sizeof ( stop_undi ) );
 err_start_undi:
	free_netdev ( netdev );
	undi_set_drvdata ( undi, NULL );
	return rc;
}

/**
 * Remove UNDI device
 *
 * @v undi		UNDI device
 */
void undinet_remove ( struct undi_device *undi ) {
	struct net_device *netdev = undi_get_drvdata ( undi );
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_SHUTDOWN undi_shutdown;
	struct s_PXENV_UNDI_CLEANUP undi_cleanup;
	struct s_PXENV_STOP_UNDI stop_undi;

	/* Unregister net device */
	unregister_netdev ( netdev );

	/* Shut down UNDI stack */
	memset ( &undi_shutdown, 0, sizeof ( undi_shutdown ) );
	undinet_call ( undinic, PXENV_UNDI_SHUTDOWN, &undi_shutdown,
		       sizeof ( undi_shutdown ) );
	memset ( &undi_cleanup, 0, sizeof ( undi_cleanup ) );
	undinet_call ( undinic, PXENV_UNDI_CLEANUP, &undi_cleanup,
		       sizeof ( undi_cleanup ) );

	/* Unhook UNDI stack */
	memset ( &stop_undi, 0, sizeof ( stop_undi ) );
	undinet_call ( undinic, PXENV_STOP_UNDI, &stop_undi,
		       sizeof ( stop_undi ) );

	/* Free network device */
	free_netdev ( netdev );
}
