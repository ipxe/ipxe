/* PXE API interface for Etherboot.
 *
 * Copyright (C) 2004 Michael Brown <mbrown@fensystems.co.uk>.
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

/* Tags used in this file:
 *
 * FIXME : obvious
 * PXESPEC : question over interpretation of the PXE spec.
 */

#ifdef PXE_EXPORT

#include "etherboot.h"
#include "pxe.h"
#include "pxe_export.h"
#include "pxe_callbacks.h"
#include "nic.h"
#include "pci.h"
#include "dev.h"
#include "cpu.h"
#include "timer.h"

#if TRACE_PXE
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif

/* Not sure why this isn't a globally-used structure within Etherboot.
 * (Because I didn't want to use packed to prevent gcc from aligning
 * source however it liked. Also nstype is a u16, not a uint. - Ken)
 */
typedef	struct {
	char dest[ETH_ALEN];
	char source[ETH_ALEN];
	unsigned int nstype;
} media_header_t;
static const char broadcast_mac[] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

/* Global pointer to currently installed PXE stack */
pxe_stack_t *pxe_stack = NULL;

/* Various startup/shutdown routines.  The startup/shutdown call
 * sequence is incredibly badly defined in the Intel PXE spec, for
 * example:
 *
 *   PXENV_UNDI_INITIALIZE says that the parameters used to initialize
 *   the adaptor should be those supplied to the most recent
 *   PXENV_UNDI_STARTUP call.  PXENV_UNDI_STARTUP takes no parameters.
 *
 *   PXENV_UNDI_CLEANUP says that the rest of the API will not be
 *   available after making this call.  Figure 3-3 ("Early UNDI API
 *   usage") shows a call to PXENV_UNDI_CLEANUP being followed by a
 *   call to the supposedly now unavailable PXENV_STOP_UNDI.
 *
 *   PXENV_UNLOAD_BASE_STACK talks about freeing up the memory
 *   occupied by the PXE stack.  Figure 4-3 ("PXE IPL") shows a call
 *   to PXENV_STOP_UNDI being made after the call to
 *   PXENV_UNLOAD_BASE_STACK, by which time the entire PXE stack
 *   should have been freed (and, potentially, zeroed).
 *
 *   Nothing, anywhere, seems to mention who's responsible for freeing
 *   up the base memory allocated for the stack segment.  It's not
 *   even clear whether or not this is expected to be in free base
 *   memory rather than claimed base memory.
 *
 * Consequently, we adopt a rather defensive strategy, designed to
 * work with any conceivable sequence of initialisation or shutdown
 * calls.  We have only two things that we care about:
 *
 *   1. Have we hooked INT 1A and INT 15,E820(etc.)?
 *   2. Is the NIC initialised?
 *
 * The NIC should never be initialised without the vectors being
 * hooked, similarly the vectors should never be unhooked with the NIC
 * still initialised.  We do, however, want to be able to have the
 * vectors hooked with the NIC shutdown.  We therefore have three
 * possible states:
 *
 *   1. Ready to unload: interrupts unhooked, NIC shutdown.
 *   2. Midway: interrupts hooked, NIC shutdown.
 *   3. Fully ready: interrupts hooked, NIC initialised.
 *
 * We provide the three states CAN_UNLOAD, MIDWAY and READY to define
 * these, and the call pxe_ensure_state() to ensure that the stack is
 * in the specified state.  All our PXE API call implementations
 * should use this call to ensure that the state is as required for
 * that PXE API call.  This enables us to cope with whatever the
 * end-user's interpretation of the PXE spec may be.  It even allows
 * for someone calling e.g. PXENV_START_UNDI followed by
 * PXENV_UDP_WRITE, without bothering with any of the intervening
 * calls.
 *
 * pxe_ensure_state() returns 1 for success, 0 for failure.  In the
 * event of failure (which can arise from e.g. asking for state READY
 * when we don't know where our NIC is), the error code
 * PXENV_STATUS_UNDI_INVALID_STATE should be returned to the user.
 * The macros ENSURE_XXX() can be used to achieve this without lots of
 * duplicated code.
 */

/* pxe_[un]hook_stack are architecture-specific and provided in
 * pxe_callbacks.c
 */

int pxe_initialise_nic ( void ) {
	if ( pxe_stack->state >= READY ) return 1;

	/* Check if NIC is initialised.  nic.dev.disable is set to 0
	 * when disable() is called, so we use this.
	 */
	if ( nic.dev.disable ) {
		/* NIC may have been initialised independently
		 * (e.g. when we set up the stack prior to calling the
		 * NBP).
		 */
		pxe_stack->state = READY;
		return 1;
	}

	/* If we already have a NIC defined, reuse that one with
	 * PROBE_AWAKE.  If one was specifed via PXENV_START_UNDI, try
	 * that one first.  Otherwise, set PROBE_FIRST.
	 */
	if ( nic.dev.state.pci.dev.use_specified == 1 ) {
		nic.dev.how_probe = PROBE_NEXT;
		DBG ( " initialising NIC specified via START_UNDI" );
	} else if ( nic.dev.state.pci.dev.driver ) {
		DBG ( " reinitialising NIC" );
		nic.dev.how_probe = PROBE_AWAKE;
	} else {
		DBG ( " probing for any NIC" );
		nic.dev.how_probe = PROBE_FIRST;
	}
	
	/* Call probe routine to bring up the NIC */
	if ( eth_probe ( &nic.dev ) != PROBE_WORKED ) {
		DBG ( " failed" );
		return 0;
	}
	pxe_stack->state = READY;
	return 1;
}

int pxe_shutdown_nic ( void ) {
	if ( pxe_stack->state <= MIDWAY ) return 1;

	eth_irq ( DISABLE );
	eth_disable();
	pxe_stack->state = MIDWAY;
	return 1;
}

int ensure_pxe_state ( pxe_stack_state_t wanted ) {
	int success = 1;

	if ( ! pxe_stack ) return 0;
	if ( wanted >= MIDWAY )
		success = success & hook_pxe_stack();
	if ( wanted > MIDWAY ) {
		success = success & pxe_initialise_nic();
	} else {
		success = success & pxe_shutdown_nic();
	}
	if ( wanted < MIDWAY )
		success = success & unhook_pxe_stack();
	return success;
}

#define ENSURE_CAN_UNLOAD(structure) if ( ! ensure_pxe_state(CAN_UNLOAD) ) { \
			structure->Status = PXENV_STATUS_UNDI_INVALID_STATE; \
			return PXENV_EXIT_FAILURE; }
#define ENSURE_MIDWAY(structure) if ( ! ensure_pxe_state(MIDWAY) ) { \
			structure->Status = PXENV_STATUS_UNDI_INVALID_STATE; \
			return PXENV_EXIT_FAILURE; }
#define ENSURE_READY(structure) if ( ! ensure_pxe_state(READY) ) { \
			structure->Status = PXENV_STATUS_UNDI_INVALID_STATE; \
			return PXENV_EXIT_FAILURE; }

/*****************************************************************************
 *
 * Actual PXE API calls
 *
 *****************************************************************************/

/* PXENV_START_UNDI
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_start_undi ( t_PXENV_START_UNDI *start_undi ) {
	unsigned char bus, devfn;
	struct pci_probe_state *pci = &nic.dev.state.pci;
	struct dev *dev = &nic.dev;

	DBG ( "PXENV_START_UNDI" );
	ENSURE_MIDWAY(start_undi);

	/* Record PCI bus & devfn passed by caller, so we know which
	 * NIC they want to use.
	 *
	 * If they don't match our already-existing NIC structure, set
	 * values to ensure that the specified NIC is used at the next
	 * call to pxe_intialise_nic().
	 */
	bus = ( start_undi->ax >> 8 ) & 0xff;
	devfn = start_undi->ax & 0xff;

	if ( ( pci->dev.driver == NULL ) ||
	     ( pci->dev.bus != bus ) || ( pci->dev.devfn != devfn ) ) {
		/* This is quite a bit of a hack and relies on
		 * knowledge of the internal operation of Etherboot's
		 * probe mechanism.
		 */
		DBG ( " set PCI %hhx:%hhx.%hhx",
		      bus, PCI_SLOT(devfn), PCI_FUNC(devfn) );
		dev->type = BOOT_NIC;
		dev->to_probe = PROBE_PCI;
		memset ( &dev->state, 0, sizeof(dev->state) );
		pci->advance = 1;
		pci->dev.use_specified = 1;
		pci->dev.bus = bus;
		pci->dev.devfn = devfn;
	}

	start_undi->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_STARTUP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_startup ( t_PXENV_UNDI_STARTUP *undi_startup ) {
	DBG ( "PXENV_UNDI_STARTUP" );
	ENSURE_MIDWAY(undi_startup);

	undi_startup->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_CLEANUP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_cleanup ( t_PXENV_UNDI_CLEANUP *undi_cleanup ) {
	DBG ( "PXENV_UNDI_CLEANUP" );
	ENSURE_CAN_UNLOAD ( undi_cleanup );

	undi_cleanup->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_INITIALIZE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_initialize ( t_PXENV_UNDI_INITIALIZE
				     *undi_initialize ) {
	DBG ( "PXENV_UNDI_INITIALIZE" );
	ENSURE_MIDWAY ( undi_initialize );

	undi_initialize->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_RESET_ADAPTER
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_reset_adapter ( t_PXENV_UNDI_RESET_ADAPTER
					*undi_reset_adapter ) {
	DBG ( "PXENV_UNDI_RESET_ADAPTER" );

	ENSURE_MIDWAY ( undi_reset_adapter );
	ENSURE_READY ( undi_reset_adapter );

	undi_reset_adapter->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_SHUTDOWN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_shutdown ( t_PXENV_UNDI_SHUTDOWN *undi_shutdown ) {
	DBG ( "PXENV_UNDI_SHUTDOWN" );
	ENSURE_MIDWAY ( undi_shutdown );

	undi_shutdown->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_OPEN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_open ( t_PXENV_UNDI_OPEN *undi_open ) {
	DBG ( "PXENV_UNDI_OPEN" );
	ENSURE_READY ( undi_open );

	/* PXESPEC: This is where we choose to enable interrupts.
	 * Can't actually find where we're meant to in the PXE spec,
	 * but this should work.
	 */
	eth_irq ( ENABLE );

	undi_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_CLOSE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_close ( t_PXENV_UNDI_CLOSE *undi_close ) {
	DBG ( "PXENV_UNDI_CLOSE" );
	ENSURE_MIDWAY ( undi_close );

	undi_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_TRANSMIT
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_transmit ( t_PXENV_UNDI_TRANSMIT *undi_transmit ) {
	t_PXENV_UNDI_TBD *tbd;
	const char *dest;
	unsigned int type;
	unsigned int length;
	const char *data;
	media_header_t *media_header;

	DBG ( "PXENV_UNDI_TRANSMIT" );
	ENSURE_READY ( undi_transmit );

	/* We support only the "immediate" portion of the TBD.  Who
	 * knows what Intel's "engineers" were smoking when they came
	 * up with the array of transmit data blocks...
	 */
	tbd = SEGOFF16_TO_PTR ( undi_transmit->TBD );
	if ( tbd->DataBlkCount > 0 ) {
		undi_transmit->Status = PXENV_STATUS_UNDI_INVALID_PARAMETER;
		return PXENV_EXIT_FAILURE;
	}
	data = SEGOFF16_TO_PTR ( tbd->Xmit );
	length = tbd->ImmedLength;

	/* If destination is broadcast, we need to supply the MAC address */
	if ( undi_transmit->XmitFlag == XMT_BROADCAST ) {
		dest = broadcast_mac;
	} else {
		dest = SEGOFF16_TO_PTR ( undi_transmit->DestAddr );
	}

	/* We can't properly support P_UNKNOWN without rewriting all
	 * the driver transmit() methods, so we cheat: if P_UNKNOWN is
	 * specified we rip the destination address and type out of
	 * the pre-assembled packet, then skip over the header.
	 */
	switch ( undi_transmit->Protocol ) {
	case P_IP:	type = IP;	break;
	case P_ARP:	type = ARP;	break;
	case P_RARP:	type = RARP;	break;
	case P_UNKNOWN:
		media_header = (media_header_t*)data;
		dest = media_header->dest;
		type = ntohs ( media_header->nstype );
		data += ETH_HLEN;
		length -= ETH_HLEN;
		break;
	default:
		undi_transmit->Status = PXENV_STATUS_UNDI_INVALID_PARAMETER;
		return PXENV_EXIT_FAILURE;
	}

	/* Send the packet */
	eth_transmit ( dest, type, length, data );
	
	undi_transmit->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_SET_MCAST_ADDRESS
 *
 * Status: stub (no PXE multicast support)
 */
PXENV_EXIT_t pxenv_undi_set_mcast_address ( t_PXENV_UNDI_SET_MCAST_ADDRESS
					    *undi_set_mcast_address ) {
	DBG ( "PXENV_UNDI_SET_MCAST_ADDRESS" );
	/* ENSURE_READY ( undi_set_mcast_address ); */
	undi_set_mcast_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SET_STATION_ADDRESS
 *
 * Status: working (deliberately incomplete)
 */
PXENV_EXIT_t pxenv_undi_set_station_address ( t_PXENV_UNDI_SET_STATION_ADDRESS
					      *undi_set_station_address ) {
	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS" );
	ENSURE_READY ( undi_set_station_address );

	/* We don't offer a facility to set the MAC address; this
	 * would require adding extra code to all the Etherboot
	 * drivers, for very little benefit.  If we're setting it to
	 * the current value anyway then return success, otherwise
	 * return UNSUPPORTED.
	 */
	if ( memcmp ( nic.node_addr,
		      &undi_set_station_address->StationAddress,
		      ETH_ALEN ) == 0 ) {
		undi_set_station_address->Status = PXENV_STATUS_SUCCESS;
		return PXENV_EXIT_SUCCESS;
	}
	undi_set_station_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SET_PACKET_FILTER
 *
 * Status: won't implement (would require driver API changes for no
 * real benefit)
 */
PXENV_EXIT_t pxenv_undi_set_packet_filter ( t_PXENV_UNDI_SET_PACKET_FILTER
					    *undi_set_packet_filter ) {
	DBG ( "PXENV_UNDI_SET_PACKET_FILTER" );
	/* ENSURE_READY ( undi_set_packet_filter ); */
	undi_set_packet_filter->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_INFORMATION
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_information ( t_PXENV_UNDI_GET_INFORMATION
					  *undi_get_information ) {
	DBG ( "PXENV_UNDI_GET_INFORMATION" );
	ENSURE_READY ( undi_get_information );

	undi_get_information->BaseIo = nic.ioaddr;
	undi_get_information->IntNumber = nic.irqno;
	/* Cheat: assume all cards can cope with this */
	undi_get_information->MaxTranUnit = ETH_MAX_MTU;
	/* Cheat: we only ever have Ethernet cards */
	undi_get_information->HwType = ETHER_TYPE;
	undi_get_information->HwAddrLen = ETH_ALEN;
	/* Cheat: assume card is always configured with its permanent
	 * node address.  This is a valid assumption within Etherboot
	 * at the time of writing.
	 */
	memcpy ( &undi_get_information->CurrentNodeAddress, nic.node_addr,
		 ETH_ALEN );
	memcpy ( &undi_get_information->PermNodeAddress, nic.node_addr,
		 ETH_ALEN );
	undi_get_information->ROMAddress = nic.rom_info->rom_segment;
	/* We only provide the ability to receive or transmit a single
	 * packet at a time.  This is a bootloader, not an OS.
	 */
	undi_get_information->RxBufCt = 1;
	undi_get_information->TxBufCt = 1;
	undi_get_information->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_STATISTICS
 *
 * Status: won't implement (would require driver API changes for no
 * real benefit)
 */
PXENV_EXIT_t pxenv_undi_get_statistics ( t_PXENV_UNDI_GET_STATISTICS
					 *undi_get_statistics ) {
	DBG ( "PXENV_UNDI_GET_STATISTICS" );
	/* ENSURE_READY ( undi_get_statistics ); */
	undi_get_statistics->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_CLEAR_STATISTICS
 *
 * Status: won't implement (would require driver API changes for no
 * real benefit)
 */
PXENV_EXIT_t pxenv_undi_clear_statistics ( t_PXENV_UNDI_CLEAR_STATISTICS
					   *undi_clear_statistics ) {
	DBG ( "PXENV_UNDI_CLEAR_STATISTICS" );
	/* ENSURE_READY ( undi_clear_statistics ); */
	undi_clear_statistics->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_INITIATE_DIAGS
 *
 * Status: won't implement (would require driver API changes for no
 * real benefit)
 */
PXENV_EXIT_t pxenv_undi_initiate_diags ( t_PXENV_UNDI_INITIATE_DIAGS
					 *undi_initiate_diags ) {
	DBG ( "PXENV_UNDI_INITIATE_DIAGS" );
	/* ENSURE_READY ( undi_initiate_diags ); */
	undi_initiate_diags->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_FORCE_INTERRUPT
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_force_interrupt ( t_PXENV_UNDI_FORCE_INTERRUPT
					  *undi_force_interrupt ) {
	DBG ( "PXENV_UNDI_FORCE_INTERRUPT" );
	ENSURE_READY ( undi_force_interrupt );

	eth_irq ( FORCE );
	undi_force_interrupt->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_MCAST_ADDRESS
 *
 * Status: stub (no PXE multicast support)
 */
PXENV_EXIT_t pxenv_undi_get_mcast_address ( t_PXENV_UNDI_GET_MCAST_ADDRESS
					    *undi_get_mcast_address ) {
	DBG ( "PXENV_UNDI_GET_MCAST_ADDRESS" );
	/* ENSURE_READY ( undi_get_mcast_address ); */
	undi_get_mcast_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_NIC_TYPE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_nic_type ( t_PXENV_UNDI_GET_NIC_TYPE
				       *undi_get_nic_type ) {
	struct dev *dev = &nic.dev;
	
	DBG ( "PXENV_UNDI_GET_NIC_TYPE" );
	ENSURE_READY ( undi_get_nic_type );
	
	if ( dev->to_probe == PROBE_PCI ) {
		struct pci_device *pci = &dev->state.pci.dev;

		undi_get_nic_type->NicType = PCI_NIC;
		undi_get_nic_type->info.pci.Vendor_ID = pci->vendor;
		undi_get_nic_type->info.pci.Dev_ID = pci->dev_id;
		undi_get_nic_type->info.pci.Base_Class = pci->class >> 8;
		undi_get_nic_type->info.pci.Sub_Class = pci->class & 0xff;
		undi_get_nic_type->info.pci.BusDevFunc =
			( pci->bus << 8 ) | pci->devfn;
		/* Cheat: these fields are probably unnecessary, and
		 * would require adding extra code to pci.c.
		 */
		undi_get_nic_type->info.pci.Prog_Intf = 0;
		undi_get_nic_type->info.pci.Rev = 0;
		undi_get_nic_type->info.pci.SubVendor_ID = 0xffff;
		undi_get_nic_type->info.pci.SubDevice_ID = 0xffff;
	} else if ( dev->to_probe == PROBE_ISA ) {
		/* const struct isa_driver *isa = dev->state.isa.driver; */

		undi_get_nic_type->NicType = PnP_NIC;
		/* Don't think anything fills these fields in, and
		 * probably no-one will ever be interested in them.
		 */
		undi_get_nic_type->info.pnp.EISA_Dev_ID = 0;
		undi_get_nic_type->info.pnp.Base_Class = 0;
		undi_get_nic_type->info.pnp.Sub_Class = 0;
		undi_get_nic_type->info.pnp.Prog_Intf = 0;
		undi_get_nic_type->info.pnp.CardSelNum = 0;
	} else {
		/* PXESPEC: There doesn't seem to be an "unknown type"
		 * defined.
		 */
		undi_get_nic_type->NicType = 0;
	}
	undi_get_nic_type->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_IFACE_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_iface_info ( t_PXENV_UNDI_GET_IFACE_INFO
					 *undi_get_iface_info ) {
	DBG ( "PXENV_UNDI_GET_IFACE_INFO" );
	ENSURE_READY ( undi_get_iface_info );

	/* Just hand back some info, doesn't really matter what it is.
	 * Most PXE stacks seem to take this approach.
	 */
	sprintf ( undi_get_iface_info->IfaceType, "Etherboot" );
	undi_get_iface_info->LinkSpeed = 10000000; /* 10 Mbps */
	undi_get_iface_info->ServiceFlags = 0;
	memset ( undi_get_iface_info->Reserved, 0,
		 sizeof(undi_get_iface_info->Reserved) );
	undi_get_iface_info->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_ISR
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_isr ( t_PXENV_UNDI_ISR *undi_isr ) {
	media_header_t *media_header = (media_header_t*)nic.packet;

	DBG ( "PXENV_UNDI_ISR" );
	/* We can't call ENSURE_READY, because this could be being
	 * called as part of an interrupt service routine.  Instead,
	 * we should simply die if we're not READY.
	 */
	if ( ( pxe_stack == NULL ) || ( pxe_stack->state < READY ) ) {
		undi_isr->Status = PXENV_STATUS_UNDI_INVALID_STATE;
		return PXENV_EXIT_FAILURE;
	}
	
	/* Just in case some idiot actually looks at these fields when
	 * we weren't meant to fill them in...
	 */
	undi_isr->BufferLength = 0;
	undi_isr->FrameLength = 0;
	undi_isr->FrameHeaderLength = 0;
	undi_isr->ProtType = 0;
	undi_isr->PktType = 0;

	switch ( undi_isr->FuncFlag ) {
	case PXENV_UNDI_ISR_IN_START :
		/* Is there a packet waiting?  If so, disable
		 * interrupts on the NIC and return "it's ours".  Do
		 * *not* necessarily acknowledge the interrupt; this
		 * can happen later when eth_poll(1) is called.  As
		 * long as the interrupt is masked off so that it
		 * doesn't immediately retrigger the 8259A then all
		 * should be well.
		 */
		DBG ( " START" );
		if ( eth_poll ( 0 ) ) {
			DBG ( " OURS" );
			eth_irq ( DISABLE );
			undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_OURS;
		} else {
			DBG ( " NOT_OURS" );
			undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_NOT_OURS;
		}
		break;
	case PXENV_UNDI_ISR_IN_PROCESS :
		/* Call poll(), return packet.  If no packet, return "done".
		 */
		DBG ( " PROCESS" );
		if ( eth_poll ( 1 ) ) {
			DBG ( " RECEIVE %d", nic.packetlen );
			if ( nic.packetlen > sizeof(pxe_stack->packet) ) {
				/* Should never happen */
				undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_DONE;
				undi_isr->Status =
					PXENV_STATUS_OUT_OF_RESOURCES;
				return PXENV_EXIT_FAILURE;
			}
			undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_RECEIVE;
			undi_isr->BufferLength = nic.packetlen;
			undi_isr->FrameLength = nic.packetlen;
			undi_isr->FrameHeaderLength = ETH_HLEN;
			memcpy ( pxe_stack->packet, nic.packet, nic.packetlen);
			PTR_TO_SEGOFF16 ( pxe_stack->packet, undi_isr->Frame );
			switch ( ntohs(media_header->nstype) ) {
			case IP :	undi_isr->ProtType = P_IP;	break;
			case ARP :	undi_isr->ProtType = P_ARP;	break;
			case RARP :	undi_isr->ProtType = P_RARP;	break;
			default :	undi_isr->ProtType = P_UNKNOWN;
			}
			if ( memcmp ( media_header->dest, broadcast_mac,
				      sizeof(broadcast_mac) ) ) {
				undi_isr->PktType = XMT_BROADCAST;
			} else {
				undi_isr->PktType = XMT_DESTADDR;
			}
			break;
		} else {
			/* No break - fall through to IN_GET_NEXT */
		}
	case PXENV_UNDI_ISR_IN_GET_NEXT :
		/* We only ever return one frame at a time */
		DBG ( " GET_NEXT DONE" );
		/* Re-enable interrupts */
		eth_irq ( ENABLE );
		/* Force an interrupt if there's a packet still
		 * waiting, since we only handle one packet per
		 * interrupt.
		 */
		if ( eth_poll ( 0 ) ) {
			DBG ( " (RETRIGGER)" );
			eth_irq ( FORCE );
		}
		undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_DONE;
		break;
	default :
		/* Should never happen */
		undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_DONE;
		undi_isr->Status = PXENV_STATUS_UNDI_INVALID_PARAMETER;
		return PXENV_EXIT_FAILURE;
	}

	undi_isr->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_STOP_UNDI
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_stop_undi ( t_PXENV_STOP_UNDI *stop_undi ) {
	DBG ( "PXENV_STOP_UNDI" );

	if ( ! ensure_pxe_state(CAN_UNLOAD) ) {
		stop_undi->Status = PXENV_STATUS_KEEP_UNDI;
		return PXENV_EXIT_FAILURE;
	}

	stop_undi->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_OPEN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_tftp_open ( t_PXENV_TFTP_OPEN *tftp_open ) {
	struct tftpreq_info_t request;
	struct tftpblk_info_t block;

	DBG ( "PXENV_TFTP_OPEN" );
	ENSURE_READY ( tftp_open );

	/* Change server address if different */
	if ( tftp_open->ServerIPAddress && 
	     tftp_open->ServerIPAddress!=arptable[ARP_SERVER].ipaddr.s_addr ) {
		memset(arptable[ARP_SERVER].node, 0, ETH_ALEN ); /* kill arp */
		arptable[ARP_SERVER].ipaddr.s_addr=tftp_open->ServerIPAddress;
	}
	/* Ignore gateway address; we can route properly */
	/* Fill in request structure */
	request.name = tftp_open->FileName;
	request.port = ntohs(tftp_open->TFTPPort);
#ifdef WORK_AROUND_BPBATCH_BUG        
	/* Force use of port 69; BpBatch tries to use port 4 for some         
	* bizarre reason.         */        
	request.port = TFTP_PORT;
#endif
	request.blksize = tftp_open->PacketSize;
	DBG ( " %@:%d/%s (%d)", tftp_open->ServerIPAddress,
	      request.port, request.name, request.blksize );
	if ( !request.blksize ) request.blksize = TFTP_DEFAULTSIZE_PACKET;
	/* Make request and get first packet */
	if ( !tftp_block ( &request, &block ) ) {
		tftp_open->Status = PXENV_STATUS_TFTP_FILE_NOT_FOUND;
		return PXENV_EXIT_FAILURE;
	}
	/* Fill in PacketSize */
	tftp_open->PacketSize = request.blksize;
	/* Store first block for later retrieval by TFTP_READ */
	pxe_stack->tftpdata.magic_cookie = PXE_TFTP_MAGIC_COOKIE;
	pxe_stack->tftpdata.len = block.len;
	pxe_stack->tftpdata.eof = block.eof;
	memcpy ( pxe_stack->tftpdata.data, block.data, block.len );

	tftp_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_CLOSE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_tftp_close ( t_PXENV_TFTP_CLOSE *tftp_close ) {
	DBG ( "PXENV_TFTP_CLOSE" );
	ENSURE_READY ( tftp_close );
	tftp_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_READ
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_tftp_read ( t_PXENV_TFTP_READ *tftp_read ) {
	struct tftpblk_info_t block;

	DBG ( "PXENV_TFTP_READ" );
	ENSURE_READY ( tftp_read );

	/* Do we have a block pending */
	if ( pxe_stack->tftpdata.magic_cookie == PXE_TFTP_MAGIC_COOKIE ) {
		block.data = pxe_stack->tftpdata.data;
		block.len = pxe_stack->tftpdata.len;
		block.eof = pxe_stack->tftpdata.eof;
		block.block = 1; /* Will be the first block */
		pxe_stack->tftpdata.magic_cookie = 0;
	} else {
		if ( !tftp_block ( NULL, &block ) ) {
			tftp_read->Status = PXENV_STATUS_TFTP_FILE_NOT_FOUND;
			return PXENV_EXIT_FAILURE;
		}
	}

	/* Return data */
	tftp_read->PacketNumber = block.block;
	tftp_read->BufferSize = block.len;
	memcpy ( SEGOFF16_TO_PTR(tftp_read->Buffer), block.data, block.len );
	DBG ( " %d to %hx:%hx", block.len, tftp_read->Buffer.segment,
	      tftp_read->Buffer.offset );
 
	tftp_read->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_READ_FILE
 *
 * Status: working
 */

int pxe_tftp_read_block ( unsigned char *data, unsigned int block __unused, unsigned int len, int eof ) {
	if ( pxe_stack->readfile.buffer ) {
		if ( pxe_stack->readfile.offset + len >=
		     pxe_stack->readfile.bufferlen ) return -1;
		memcpy ( pxe_stack->readfile.buffer +
			 pxe_stack->readfile.offset, data, len );
	}
	pxe_stack->readfile.offset += len;
	return eof ? 0 : 1;
}

PXENV_EXIT_t pxenv_tftp_read_file ( t_PXENV_TFTP_READ_FILE *tftp_read_file ) {
	int rc;

	DBG ( "PXENV_TFTP_READ_FILE %s to [%x,%x)", tftp_read_file->FileName,
	      tftp_read_file->Buffer, tftp_read_file->Buffer + tftp_read_file->BufferSize );
	ENSURE_READY ( tftp_read_file );

	/* inserted by Klaus Wittemeier */
	/* KERNEL_BUF stores the name of the last required file */
	/* This is a fix to make Microsoft Remote Install Services work (RIS) */
	memcpy(KERNEL_BUF, tftp_read_file->FileName, sizeof(KERNEL_BUF));
	/* end of insertion */

	pxe_stack->readfile.buffer = phys_to_virt ( tftp_read_file->Buffer );
	pxe_stack->readfile.bufferlen = tftp_read_file->BufferSize;
	pxe_stack->readfile.offset = 0;

	rc = tftp ( tftp_read_file->FileName, pxe_tftp_read_block );
	if ( rc ) {
		tftp_read_file->Status = PXENV_STATUS_FAILURE;
		return PXENV_EXIT_FAILURE;
	}
	tftp_read_file->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_TFTP_GET_FSIZE
 *
 * Status: working, though ugly (we actually read the whole file,
 * because it's too ugly to make Etherboot request the tsize option
 * and hand it to us).
 */
PXENV_EXIT_t pxenv_tftp_get_fsize ( t_PXENV_TFTP_GET_FSIZE *tftp_get_fsize ) {
	int rc;

	DBG ( "PXENV_TFTP_GET_FSIZE" );
	ENSURE_READY ( tftp_get_fsize );

	pxe_stack->readfile.buffer = NULL;
	pxe_stack->readfile.bufferlen = 0;
	pxe_stack->readfile.offset = 0;

	rc = tftp ( tftp_get_fsize->FileName, pxe_tftp_read_block );
	if ( rc ) {
		tftp_get_fsize->FileSize = 0;
		tftp_get_fsize->Status = PXENV_STATUS_FAILURE;
		return PXENV_EXIT_FAILURE;
	}
	tftp_get_fsize->FileSize = pxe_stack->readfile.offset;
	tftp_get_fsize->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UDP_OPEN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_udp_open ( t_PXENV_UDP_OPEN *udp_open ) {
	DBG ( "PXENV_UDP_OPEN" );
	ENSURE_READY ( udp_open );

	if ( udp_open->src_ip &&
	     udp_open->src_ip != arptable[ARP_CLIENT].ipaddr.s_addr ) {
		/* Overwrite our IP address */
		DBG ( " with new IP %@", udp_open->src_ip );
		arptable[ARP_CLIENT].ipaddr.s_addr = udp_open->src_ip;
	}

	udp_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UDP_CLOSE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_udp_close ( t_PXENV_UDP_CLOSE *udp_close ) {
	DBG ( "PXENV_UDP_CLOSE" );
	ENSURE_READY ( udp_close );
	udp_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UDP_READ
 *
 * Status: working
 */
int await_pxe_udp ( int ival __unused, void *ptr,
		    unsigned short ptype __unused,
		    struct iphdr *ip, struct udphdr *udp,
		    struct tcphdr *tcp __unused ) {
	t_PXENV_UDP_READ *udp_read = (t_PXENV_UDP_READ*)ptr;
	uint16_t d_port;
	size_t size;

	/* Ignore non-UDP packets */
	if ( !udp ) {
		DBG ( " non-UDP" );
		return 0;
	}
	
	/* Check dest_ip */
	if ( udp_read->dest_ip && ( udp_read->dest_ip != ip->dest.s_addr ) ) {
		DBG ( " wrong dest IP (got %@, wanted %@)",
		      ip->dest.s_addr, udp_read->dest_ip );
		return 0;
	}

	/* Check dest_port */
	d_port = ntohs ( udp_read->d_port );
	if ( d_port && ( d_port != ntohs(udp->dest) ) ) {
		DBG ( " wrong dest port (got %d, wanted %d)",
		      ntohs(udp->dest), d_port );
		return 0;
	}

	/* Copy packet to buffer and fill in information */
	udp_read->src_ip = ip->src.s_addr;
	udp_read->s_port = udp->src; /* Both in network order */
	size = ntohs(udp->len) - sizeof(*udp);
	/* Workaround: NTLDR expects us to fill these in, even though
	 * PXESPEC clearly defines them as input parameters.
	 */
	udp_read->dest_ip = ip->dest.s_addr;
	udp_read->d_port = udp->dest;
	DBG ( " %@:%d->%@:%d (%d)",
	      udp_read->src_ip, ntohs(udp_read->s_port),
	      udp_read->dest_ip, ntohs(udp_read->d_port), size );
	if ( udp_read->buffer_size < size ) {
		/* PXESPEC: what error code should we actually return? */
		DBG ( " buffer too small (%d)", udp_read->buffer_size );
		udp_read->Status = PXENV_STATUS_OUT_OF_RESOURCES;
		return 0;
	}
	memcpy ( SEGOFF16_TO_PTR ( udp_read->buffer ), &udp->payload, size );
	udp_read->buffer_size = size;

	return 1;
}

PXENV_EXIT_t pxenv_udp_read ( t_PXENV_UDP_READ *udp_read ) {
	DBG ( "PXENV_UDP_READ" );
	ENSURE_READY ( udp_read );

	/* Use await_reply with a timeout of zero */
	/* Allow await_reply to change Status if necessary */
	udp_read->Status = PXENV_STATUS_FAILURE;
	if ( ! await_reply ( await_pxe_udp, 0, udp_read, 0 ) ) {
		return PXENV_EXIT_FAILURE;
	}

	udp_read->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UDP_WRITE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_udp_write ( t_PXENV_UDP_WRITE *udp_write ) {
	uint16_t src_port;
	uint16_t dst_port;
	struct udppacket *packet = (struct udppacket *)nic.packet;
	int packet_size;

	DBG ( "PXENV_UDP_WRITE" );
	ENSURE_READY ( udp_write );

	/* PXE spec says source port is 2069 if not specified */
	src_port = ntohs(udp_write->src_port);
	if ( src_port == 0 ) src_port = 2069;
	dst_port = ntohs(udp_write->dst_port);
	DBG ( " %d->%@:%d (%d)", src_port, udp_write->ip, dst_port,
	      udp_write->buffer_size );
	
	/* FIXME: we ignore the gateway specified, since we're
	 * confident of being able to do our own routing.  We should
	 * probably allow for multiple gateways.
	 */
	
	/* Copy payload to packet buffer */
	packet_size = ( (void*)&packet->payload - (void*)packet )
		+ udp_write->buffer_size;
	if ( packet_size > ETH_FRAME_LEN ) {
		udp_write->Status = PXENV_STATUS_OUT_OF_RESOURCES;
		return PXENV_EXIT_FAILURE;
	}
	memcpy ( &packet->payload, SEGOFF16_TO_PTR(udp_write->buffer),
		 udp_write->buffer_size );

	/* Transmit packet */
	if ( ! udp_transmit ( udp_write->ip, src_port, dst_port,
			      packet_size, packet ) ) {
		udp_write->Status = PXENV_STATUS_FAILURE;
		return PXENV_EXIT_FAILURE;
	}

	udp_write->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNLOAD_STACK
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_unload_stack ( t_PXENV_UNLOAD_STACK *unload_stack ) {
	int success;

	DBG ( "PXENV_UNLOAD_STACK" );
	success = ensure_pxe_state ( CAN_UNLOAD );

	/* We need to call cleanup() at some point.  The network card
	 * has already been disabled by ENSURE_CAN_UNLOAD(), but for
	 * the sake of completeness we should call the console_fini()
	 * etc. that are part of cleanup().
	 *
	 * There seems to be a lack of consensus on which is the final
	 * PXE API call to make, but it's a fairly safe bet that all
	 * the potential shutdown sequences will include a call to
	 * PXENV_UNLOAD_STACK at some point, so we may as well do it
	 * here.
	 */
	cleanup();

	if ( ! success ) {
		unload_stack->Status = PXENV_STATUS_KEEP_ALL;
		return PXENV_EXIT_FAILURE;
	}

	unload_stack->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_GET_CACHED_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_get_cached_info ( t_PXENV_GET_CACHED_INFO
				     *get_cached_info ) {
	BOOTPLAYER *cached_info = &pxe_stack->cached_info;
	DBG ( "PXENV_GET_CACHED_INFO %d", get_cached_info->PacketType );
	ENSURE_READY ( get_cached_info );

	/* Fill in cached_info structure in our pxe_stack */

	/* I don't think there's actually any way we can be called in
	 * the middle of a DHCP request... 
	 */
	cached_info->opcode = BOOTP_REP;
	/* We only have Ethernet drivers */
	cached_info->Hardware = ETHER_TYPE;
	cached_info->Hardlen = ETH_ALEN;
	/* PXESPEC: "Client sets" says the spec, but who's filling in
	 * this structure?  It ain't the client.
	 */
	cached_info->Gatehops = 0;
	cached_info->ident = 0;
	cached_info->seconds = 0;
	cached_info->Flags = BOOTP_BCAST;
	/* PXESPEC: What do 'Client' and 'Your' IP address refer to? */
	cached_info->cip = arptable[ARP_CLIENT].ipaddr.s_addr;
	cached_info->yip = arptable[ARP_CLIENT].ipaddr.s_addr;
	cached_info->sip = arptable[ARP_SERVER].ipaddr.s_addr;
	/* PXESPEC: Does "GIP" mean "Gateway" or "Relay agent"? */
	cached_info->gip = arptable[ARP_GATEWAY].ipaddr.s_addr;
	memcpy ( cached_info->CAddr, arptable[ARP_CLIENT].node, ETH_ALEN );
	/* Nullify server name */
	cached_info->Sname[0] = '\0';
	memcpy ( cached_info->bootfile, KERNEL_BUF,
		 sizeof(cached_info->bootfile) );
	/* Copy DHCP vendor options */
	memcpy ( &cached_info->vendor.d, BOOTP_DATA_ADDR->bootp_reply.bp_vend,
		 sizeof(cached_info->vendor.d) );
	
	/* Copy to user-specified buffer, or set pointer to our buffer */
	get_cached_info->BufferLimit = sizeof(*cached_info);
	/* PXESPEC: says to test for Buffer == NULL *and* BufferSize =
	 * 0, but what are we supposed to do with a null buffer of
	 * non-zero size?!
	 */
	if ( IS_NULL_SEGOFF16 ( get_cached_info->Buffer ) ) {
		/* Point back to our buffer */
		PTR_TO_SEGOFF16 ( cached_info, get_cached_info->Buffer );
		get_cached_info->BufferSize = sizeof(*cached_info);
	} else {
		/* Copy to user buffer */
		size_t size = sizeof(*cached_info);
		void *buffer = SEGOFF16_TO_PTR ( get_cached_info->Buffer );
		if ( get_cached_info->BufferSize < size )
			size = get_cached_info->BufferSize;
		DBG ( " to %x", virt_to_phys ( buffer ) );
		memcpy ( buffer, cached_info, size );
		/* PXESPEC: Should we return an error if the user
		 * buffer is too small?  We do return the actual size
		 * of the buffer via BufferLimit, so the user does
		 * have a way to detect this already.
		 */
	}

	get_cached_info->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_RESTART_TFTP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_restart_tftp ( t_PXENV_RESTART_TFTP *restart_tftp ) {
	PXENV_EXIT_t tftp_exit;

	DBG ( "PXENV_RESTART_TFTP" );
	ENSURE_READY ( restart_tftp );

	/* Words cannot describe the complete mismatch between the PXE
	 * specification and any possible version of reality...
	 */
	restart_tftp->Buffer = PXE_LOAD_ADDRESS; /* Fixed by spec, apparently */
	restart_tftp->BufferSize = get_free_base_memory() - PXE_LOAD_ADDRESS; /* Near enough */
	DBG ( "(" );
	tftp_exit = pxe_api_call ( PXENV_TFTP_READ_FILE, (t_PXENV_ANY*)restart_tftp );
	DBG ( ")" );
	if ( tftp_exit != PXENV_EXIT_SUCCESS ) return tftp_exit;

	/* Fire up the new NBP */
	restart_tftp->Status = xstartpxe();

	/* Not sure what "SUCCESS" actually means, since we can only
	 * return if the new NBP failed to boot...
	 */
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_START_BASE
 *
 * Status: won't implement (requires major structural changes)
 */
PXENV_EXIT_t pxenv_start_base ( t_PXENV_START_BASE *start_base ) {
	DBG ( "PXENV_START_BASE" );
	/* ENSURE_READY ( start_base ); */
	start_base->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_STOP_BASE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_stop_base ( t_PXENV_STOP_BASE *stop_base ) {
	DBG ( "PXENV_STOP_BASE" );

	/* The only time we will be called is when the NBP is trying
	 * to shut down the PXE stack.  There's nothing we need to do
	 * in this call.
	 */

	stop_base->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_LOADER
 *
 * Status: working
 *
 * NOTE: This is not a genuine PXE API call; the loader has a separate
 * entry point.  However, to simplify the mapping of the PXE API to
 * the internal Etherboot API, both are directed through the same
 * interface.
 */
PXENV_EXIT_t pxenv_undi_loader ( undi_loader_t *loader ) {
	uint32_t loader_phys = virt_to_phys ( loader );

	DBG ( "PXENV_UNDI_LOADER" );
	
	/* Set UNDI DS as our real-mode stack */
	use_undi_ds_for_rm_stack ( loader->undi_ds );

	/* FIXME: These lines are borrowed from main.c.  There should
	 * probably be a single initialise() function that does all
	 * this, but it's currently split interestingly between main()
	 * and main_loop()...
	 */
	console_init();
	cpu_setup();
	setup_timers();
	gateA20_set();
	print_config();
	get_memsizes();
	cleanup();
	relocate();
	cleanup();
	console_init();
	init_heap();

	/* We have relocated; the loader pointer is now invalid */
	loader = phys_to_virt ( loader_phys );

	/* Install PXE stack to area specified by NBP */
	install_pxe_stack ( VIRTUAL ( loader->undi_cs, 0 ) );
	
	/* Call pxenv_start_undi to set parameters.  Why the hell PXE
	 * requires these parameters to be provided twice is beyond
	 * the wit of any sane man.  Don't worry if it fails; the NBP
	 * should call PXENV_START_UNDI separately anyway.
	 */
	pxenv_start_undi ( &loader->start_undi );
	/* Unhook stack; the loader is not meant to hook int 1a etc,
	 * but the call the pxenv_start_undi will cause it to happen.
	 */
	ENSURE_CAN_UNLOAD ( loader );

	/* Fill in addresses of !PXE and PXENV+ structures */
	PTR_TO_SEGOFF16 ( &pxe_stack->pxe, loader->pxe_ptr );
	PTR_TO_SEGOFF16 ( &pxe_stack->pxenv, loader->pxenv_ptr );
	
	loader->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* API call dispatcher
 *
 * Status: complete
 */
PXENV_EXIT_t pxe_api_call ( int opcode, t_PXENV_ANY *params ) {
	PXENV_EXIT_t ret = PXENV_EXIT_FAILURE;

	/* Set default status in case child routine fails to do so */
	params->Status = PXENV_STATUS_FAILURE;

	DBG ( "[" );

	/* Hand off to relevant API routine */
	switch ( opcode ) {
	case PXENV_START_UNDI:
		ret = pxenv_start_undi ( &params->start_undi );
		break;
	case PXENV_UNDI_STARTUP:
		ret = pxenv_undi_startup ( &params->undi_startup );
		break;
	case PXENV_UNDI_CLEANUP:
		ret = pxenv_undi_cleanup ( &params->undi_cleanup );
		break;
	case PXENV_UNDI_INITIALIZE:
		ret = pxenv_undi_initialize ( &params->undi_initialize );
		break;
	case PXENV_UNDI_RESET_ADAPTER:
		ret = pxenv_undi_reset_adapter ( &params->undi_reset_adapter );
		break;
	case PXENV_UNDI_SHUTDOWN:
		ret = pxenv_undi_shutdown ( &params->undi_shutdown );
		break;
	case PXENV_UNDI_OPEN:
		ret = pxenv_undi_open ( &params->undi_open );
		break;
	case PXENV_UNDI_CLOSE:
		ret = pxenv_undi_close ( &params->undi_close );
		break;
	case PXENV_UNDI_TRANSMIT:
		ret = pxenv_undi_transmit ( &params->undi_transmit );
		break;
	case PXENV_UNDI_SET_MCAST_ADDRESS:
		ret = pxenv_undi_set_mcast_address (
				             &params->undi_set_mcast_address );
		break;
	case PXENV_UNDI_SET_STATION_ADDRESS:
		ret = pxenv_undi_set_station_address (
					   &params->undi_set_station_address );
		break;
	case PXENV_UNDI_SET_PACKET_FILTER:
		ret = pxenv_undi_set_packet_filter (
					     &params->undi_set_packet_filter );
		break;
	case PXENV_UNDI_GET_INFORMATION:
		ret = pxenv_undi_get_information (
					       &params->undi_get_information );
		break;
	case PXENV_UNDI_GET_STATISTICS:
		ret = pxenv_undi_get_statistics (
					        &params->undi_get_statistics );
		break;
	case PXENV_UNDI_CLEAR_STATISTICS:
		ret = pxenv_undi_clear_statistics (
					      &params->undi_clear_statistics );
		break;
	case PXENV_UNDI_INITIATE_DIAGS:
		ret = pxenv_undi_initiate_diags (
						&params->undi_initiate_diags );
		break;
	case PXENV_UNDI_FORCE_INTERRUPT:
		ret = pxenv_undi_force_interrupt (
					       &params->undi_force_interrupt );
		break;
	case PXENV_UNDI_GET_MCAST_ADDRESS:
		ret = pxenv_undi_get_mcast_address (
					     &params->undi_get_mcast_address );
		break;
	case PXENV_UNDI_GET_NIC_TYPE:
		ret = pxenv_undi_get_nic_type ( &params->undi_get_nic_type );
		break;
	case PXENV_UNDI_GET_IFACE_INFO:
		ret = pxenv_undi_get_iface_info (
					        &params->undi_get_iface_info );
		break;
	case PXENV_UNDI_ISR:
		ret = pxenv_undi_isr ( &params->undi_isr );
		break;
	case PXENV_STOP_UNDI:
		ret = pxenv_stop_undi ( &params->stop_undi );
		break;
	case PXENV_TFTP_OPEN:
		ret = pxenv_tftp_open ( &params->tftp_open );
		break;
	case PXENV_TFTP_CLOSE:
		ret = pxenv_tftp_close ( &params->tftp_close );
		break;
	case PXENV_TFTP_READ:
		ret = pxenv_tftp_read ( &params->tftp_read );
		break;
	case PXENV_TFTP_READ_FILE:
		ret = pxenv_tftp_read_file ( &params->tftp_read_file );
		break;
	case PXENV_TFTP_GET_FSIZE:
		ret = pxenv_tftp_get_fsize ( &params->tftp_get_fsize );
		break;
	case PXENV_UDP_OPEN:
		ret = pxenv_udp_open ( &params->udp_open );
		break;
	case PXENV_UDP_CLOSE:
		ret = pxenv_udp_close ( &params->udp_close );
		break;
	case PXENV_UDP_READ:
		ret = pxenv_udp_read ( &params->udp_read );
		break;
	case PXENV_UDP_WRITE:
		ret = pxenv_udp_write ( &params->udp_write );
		break;
	case PXENV_UNLOAD_STACK:
		ret = pxenv_unload_stack ( &params->unload_stack );
		break;
	case PXENV_GET_CACHED_INFO:
		ret = pxenv_get_cached_info ( &params->get_cached_info );
		break;
	case PXENV_RESTART_TFTP:
		ret = pxenv_restart_tftp ( &params->restart_tftp );
		break;
	case PXENV_START_BASE:
		ret = pxenv_start_base ( &params->start_base );
		break;
	case PXENV_STOP_BASE:
		ret = pxenv_stop_base ( &params->stop_base );
		break;
	case PXENV_UNDI_LOADER:
		ret = pxenv_undi_loader ( &params->loader );
		break;
		
	default:
		DBG ( "PXENV_UNKNOWN_%hx", opcode );
		params->Status = PXENV_STATUS_UNSUPPORTED;
		ret = PXENV_EXIT_FAILURE;
		break;
	}

	if ( params->Status != PXENV_STATUS_SUCCESS ) {
		DBG ( " %hx", params->Status );
	}
	if ( ret != PXENV_EXIT_SUCCESS ) {
		DBG ( ret == PXENV_EXIT_FAILURE ? " err" : " ??" );
	}
	DBG ( "]" );

	return ret;
}

#endif /* PXE_EXPORT */
