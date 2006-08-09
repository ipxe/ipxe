/** @file
 *
 * PXE UNDI API
 *
 */

/*
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

#include "pxe.h"

/* PXENV_UNDI_STARTUP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_startup ( struct s_PXENV_UNDI_STARTUP *undi_startup ) {
	DBG ( "PXENV_UNDI_STARTUP" );

	undi_startup->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_CLEANUP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_cleanup ( struct s_PXENV_UNDI_CLEANUP *undi_cleanup ) {
	DBG ( "PXENV_UNDI_CLEANUP" );

	undi_cleanup->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_INITIALIZE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_initialize ( struct s_PXENV_UNDI_INITIALIZE
				     *undi_initialize ) {
	DBG ( "PXENV_UNDI_INITIALIZE" );

	undi_initialize->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_RESET_ADAPTER
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_reset_adapter ( struct s_PXENV_UNDI_RESET
					*undi_reset_adapter ) {
	DBG ( "PXENV_UNDI_RESET_ADAPTER" );

	undi_reset_adapter->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_SHUTDOWN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_shutdown ( struct s_PXENV_UNDI_SHUTDOWN
				   *undi_shutdown ) {
	DBG ( "PXENV_UNDI_SHUTDOWN" );

	undi_shutdown->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_OPEN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_open ( struct s_PXENV_UNDI_OPEN *undi_open ) {
	DBG ( "PXENV_UNDI_OPEN" );

#if 0
	/* PXESPEC: This is where we choose to enable interrupts.
	 * Can't actually find where we're meant to in the PXE spec,
	 * but this should work.
	 */
	eth_irq ( ENABLE );
#endif

	undi_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_CLOSE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_close ( struct s_PXENV_UNDI_CLOSE *undi_close ) {
	DBG ( "PXENV_UNDI_CLOSE" );

	undi_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_TRANSMIT
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_transmit ( struct s_PXENV_UNDI_TRANSMIT
				   *undi_transmit ) {
	struct s_PXENV_UNDI_TBD *tbd;
	const char *dest;
	unsigned int type;
	unsigned int length;
	const char *data;

	DBG ( "PXENV_UNDI_TRANSMIT" );

#if 0
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
	case P_IP:	type = ETH_P_IP;	break;
	case P_ARP:	type = ETH_P_ARP;	break;
	case P_RARP:	type = ETH_P_RARP;	break;
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
#endif
	
	undi_transmit->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_SET_MCAST_ADDRESS
 *
 * Status: stub (no PXE multicast support)
 */
PXENV_EXIT_t
pxenv_undi_set_mcast_address ( struct s_PXENV_UNDI_SET_MCAST_ADDRESS
			       *undi_set_mcast_address ) {
	DBG ( "PXENV_UNDI_SET_MCAST_ADDRESS" );

	undi_set_mcast_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SET_STATION_ADDRESS
 *
 * Status: working (deliberately incomplete)
 */
PXENV_EXIT_t 
pxenv_undi_set_station_address ( struct s_PXENV_UNDI_SET_STATION_ADDRESS
				 *undi_set_station_address ) {
	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS" );

#if 0
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
#endif

	undi_set_station_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_SET_PACKET_FILTER
 *
 * Status: won't implement (would require driver API changes for no
 * real benefit)
 */
PXENV_EXIT_t
pxenv_undi_set_packet_filter ( struct s_PXENV_UNDI_SET_PACKET_FILTER
			       *undi_set_packet_filter ) {
	DBG ( "PXENV_UNDI_SET_PACKET_FILTER" );

	undi_set_packet_filter->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_INFORMATION
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_information ( struct s_PXENV_UNDI_GET_INFORMATION
					  *undi_get_information ) {
	DBG ( "PXENV_UNDI_GET_INFORMATION" );

#if 0
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
	undi_get_information->ROMAddress = 0;
		/* nic.rom_info->rom_segment; */
	/* We only provide the ability to receive or transmit a single
	 * packet at a time.  This is a bootloader, not an OS.
	 */
	undi_get_information->RxBufCt = 1;
	undi_get_information->TxBufCt = 1;
#endif

	undi_get_information->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_STATISTICS
 *
 * Status: won't implement (would require driver API changes for no
 * real benefit)
 */
PXENV_EXIT_t pxenv_undi_get_statistics ( struct s_PXENV_UNDI_GET_STATISTICS
					 *undi_get_statistics ) {
	DBG ( "PXENV_UNDI_GET_STATISTICS" );

	undi_get_statistics->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_CLEAR_STATISTICS
 *
 * Status: won't implement (would require driver API changes for no
 * real benefit)
 */
PXENV_EXIT_t pxenv_undi_clear_statistics ( struct s_PXENV_UNDI_CLEAR_STATISTICS
					   *undi_clear_statistics ) {
	DBG ( "PXENV_UNDI_CLEAR_STATISTICS" );

	undi_clear_statistics->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_INITIATE_DIAGS
 *
 * Status: won't implement (would require driver API changes for no
 * real benefit)
 */
PXENV_EXIT_t pxenv_undi_initiate_diags ( struct s_PXENV_UNDI_INITIATE_DIAGS
					 *undi_initiate_diags ) {
	DBG ( "PXENV_UNDI_INITIATE_DIAGS" );

	undi_initiate_diags->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_FORCE_INTERRUPT
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_force_interrupt ( struct s_PXENV_UNDI_FORCE_INTERRUPT
					  *undi_force_interrupt ) {
	DBG ( "PXENV_UNDI_FORCE_INTERRUPT" );

#if 0
	eth_irq ( FORCE );
#endif

	undi_force_interrupt->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_MCAST_ADDRESS
 *
 * Status: stub (no PXE multicast support)
 */
PXENV_EXIT_t
pxenv_undi_get_mcast_address ( struct s_PXENV_UNDI_GET_MCAST_ADDRESS
			       *undi_get_mcast_address ) {
	DBG ( "PXENV_UNDI_GET_MCAST_ADDRESS" );

	undi_get_mcast_address->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_UNDI_GET_NIC_TYPE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_nic_type ( struct s_PXENV_UNDI_GET_NIC_TYPE
				       *undi_get_nic_type ) {
	DBG ( "PXENV_UNDI_GET_NIC_TYPE" );

#warning "device probing mechanism has changed completely"
#if 0
	struct dev *dev = &dev;	
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

#endif
}

/* PXENV_UNDI_GET_IFACE_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_iface_info ( struct s_PXENV_UNDI_GET_IFACE_INFO
					 *undi_get_iface_info ) {
	DBG ( "PXENV_UNDI_GET_IFACE_INFO" );

#if 0
	/* Just hand back some info, doesn't really matter what it is.
	 * Most PXE stacks seem to take this approach.
	 */
	sprintf ( undi_get_iface_info->IfaceType, "Etherboot" );
	undi_get_iface_info->LinkSpeed = 10000000; /* 10 Mbps */
	undi_get_iface_info->ServiceFlags = 0;
	memset ( undi_get_iface_info->Reserved, 0,
		 sizeof(undi_get_iface_info->Reserved) );
#endif

	undi_get_iface_info->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_STATE
 *
 * Status: impossible
 */
PXENV_EXIT_t pxenv_undi_get_state ( struct s_PXENV_UNDI_GET_STATE
				    *undi_get_state ) {
	DBG ( "PXENV_UNDI_GET_STATE" );

	undi_get_state->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
};

/* PXENV_UNDI_ISR
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_isr ( struct s_PXENV_UNDI_ISR *undi_isr ) {
	DBG ( "PXENV_UNDI_ISR" );

#if 0
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
			case ETH_P_IP:	undi_isr->ProtType = P_IP;	break;
			case ETH_P_ARP:	undi_isr->ProtType = P_ARP;	break;
			case ETH_P_RARP: undi_isr->ProtType = P_RARP;	break;
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
#endif

	undi_isr->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}
