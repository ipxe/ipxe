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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <basemem_packet.h>
#include <gpxe/netdevice.h>
#include <gpxe/iobuf.h>
#include <gpxe/device.h>
#include <gpxe/pci.h>
#include <gpxe/if_ether.h>
#include <gpxe/ip.h>
#include <gpxe/arp.h>
#include <gpxe/rarp.h>
#include "pxe.h"

/**
 * Count of outstanding transmitted packets
 *
 * This is incremented each time PXENV_UNDI_TRANSMIT is called, and
 * decremented each time that PXENV_UNDI_ISR is called with the TX
 * queue empty, stopping when the count reaches zero.  This allows us
 * to provide a pessimistic approximation of TX completion events to
 * the PXE NBP simply by monitoring the netdev's TX queue.
 */
static int undi_tx_count = 0;

struct net_device *pxe_netdev = NULL;

/**
 * Set network device as current PXE network device
 *
 * @v netdev		Network device, or NULL
 */
void pxe_set_netdev ( struct net_device *netdev ) {
	if ( pxe_netdev )
		netdev_put ( pxe_netdev );
	pxe_netdev = NULL;
	if ( netdev )
		pxe_netdev = netdev_get ( netdev );
}

/**
 * Open PXE network device
 *
 * @ret rc		Return status code
 */
static int pxe_netdev_open ( void ) {
	return netdev_open ( pxe_netdev );
}

/**
 * Close PXE network device
 *
 */
static void pxe_netdev_close ( void ) {
	netdev_close ( pxe_netdev );
	undi_tx_count = 0;
}

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

	pxe_netdev_close();

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
	int rc;

	DBG ( "PXENV_UNDI_RESET_ADAPTER" );

	pxe_netdev_close();
	if ( ( rc = pxe_netdev_open() ) != 0 ) {
		undi_reset_adapter->Status = PXENV_STATUS ( rc );
		return PXENV_EXIT_FAILURE;
	}

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

	pxe_netdev_close();

	undi_shutdown->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_OPEN
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_open ( struct s_PXENV_UNDI_OPEN *undi_open ) {
	int rc;

	DBG ( "PXENV_UNDI_OPEN" );

	if ( ( rc = pxe_netdev_open() ) != 0 ) {
		undi_open->Status = PXENV_STATUS ( rc );
		return PXENV_EXIT_FAILURE;
	}

	undi_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_CLOSE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_close ( struct s_PXENV_UNDI_CLOSE *undi_close ) {
	DBG ( "PXENV_UNDI_CLOSE" );

	pxe_netdev_close();

	undi_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_TRANSMIT
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_transmit ( struct s_PXENV_UNDI_TRANSMIT
				   *undi_transmit ) {
	struct s_PXENV_UNDI_TBD tbd;
	struct DataBlk *datablk;
	struct io_buffer *iobuf;
	struct net_protocol *net_protocol;
	char destaddr[MAX_LL_ADDR_LEN];
	const void *ll_dest;
	size_t ll_hlen = pxe_netdev->ll_protocol->ll_header_len;
	size_t len;
	unsigned int i;
	int rc;

	DBG ( "PXENV_UNDI_TRANSMIT" );

	/* Identify network-layer protocol */
	switch ( undi_transmit->Protocol ) {
	case P_IP:	net_protocol = &ipv4_protocol;	break;
	case P_ARP:	net_protocol = &arp_protocol;	break;
	case P_RARP:	net_protocol = &rarp_protocol;	break;
	case P_UNKNOWN:
		net_protocol = NULL;
		ll_hlen = 0;
		break;
	default:
		undi_transmit->Status = PXENV_STATUS_UNDI_INVALID_PARAMETER;
		return PXENV_EXIT_FAILURE;
	}
	DBG ( " %s", ( net_protocol ? net_protocol->name : "UNKNOWN" ) );

	/* Calculate total packet length */
	copy_from_real ( &tbd, undi_transmit->TBD.segment,
			 undi_transmit->TBD.offset, sizeof ( tbd ) );
	len = tbd.ImmedLength;
	DBG ( " %zd", tbd.ImmedLength );
	for ( i = 0 ; i < tbd.DataBlkCount ; i++ ) {
		datablk = &tbd.DataBlock[i];
		len += datablk->TDDataLen;
		DBG ( "+%zd", datablk->TDDataLen );
	}

	/* Allocate and fill I/O buffer */
	iobuf = alloc_iob ( ll_hlen + len );
	if ( ! iobuf ) {
		undi_transmit->Status = PXENV_STATUS_OUT_OF_RESOURCES;
		return PXENV_EXIT_FAILURE;
	}
	iob_reserve ( iobuf, ll_hlen );
	copy_from_real ( iob_put ( iobuf, tbd.ImmedLength ), tbd.Xmit.segment,
			 tbd.Xmit.offset, tbd.ImmedLength );
	for ( i = 0 ; i < tbd.DataBlkCount ; i++ ) {
		datablk = &tbd.DataBlock[i];
		copy_from_real ( iob_put ( iobuf, datablk->TDDataLen ),
				 datablk->TDDataPtr.segment,
				 datablk->TDDataPtr.offset,
				 datablk->TDDataLen );
	}

	/* Transmit packet */
	if ( net_protocol == NULL ) {
		/* Link-layer header already present */
		rc = netdev_tx ( pxe_netdev, iobuf );
	} else {
		/* Calculate destination address */
		if ( undi_transmit->XmitFlag == XMT_DESTADDR ) {
			copy_from_real ( destaddr,
					 undi_transmit->DestAddr.segment,
					 undi_transmit->DestAddr.offset,
					 pxe_netdev->ll_protocol->ll_addr_len );
			ll_dest = destaddr;
		} else {
			ll_dest = pxe_netdev->ll_protocol->ll_broadcast;
		}
		rc = net_tx ( iobuf, pxe_netdev, net_protocol, ll_dest );
	}

	/* Flag transmission as in-progress */
	undi_tx_count++;

	undi_transmit->Status = PXENV_STATUS ( rc );
	return ( ( rc == 0 ) ? PXENV_EXIT_SUCCESS : PXENV_EXIT_FAILURE );
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
 * Status: working
 */
PXENV_EXIT_t 
pxenv_undi_set_station_address ( struct s_PXENV_UNDI_SET_STATION_ADDRESS
				 *undi_set_station_address ) {

	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS" );

	/* If adapter is open, the change will have no effect; return
	 * an error
	 */
	if ( pxe_netdev->state & NETDEV_OPEN ) {
		undi_set_station_address->Status =
			PXENV_STATUS_UNDI_INVALID_STATE;
		return PXENV_EXIT_FAILURE;
	}

	/* Update MAC address */
	memcpy ( pxe_netdev->ll_addr,
		 &undi_set_station_address->StationAddress,
		 pxe_netdev->ll_protocol->ll_addr_len );

	undi_set_station_address->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
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
	struct device *dev = pxe_netdev->dev;
	struct ll_protocol *ll_protocol = pxe_netdev->ll_protocol;

	DBG ( "PXENV_UNDI_GET_INFORMATION" );

	undi_get_information->BaseIo = dev->desc.ioaddr;
	undi_get_information->IntNumber = dev->desc.irq;
	/* Cheat: assume all cards can cope with this */
	undi_get_information->MaxTranUnit = ETH_MAX_MTU;
	undi_get_information->HwType = ntohs ( ll_protocol->ll_proto );
	undi_get_information->HwAddrLen = ll_protocol->ll_addr_len;
	/* Cheat: assume card is always configured with its permanent
	 * node address.  This is a valid assumption within Etherboot
	 * at the time of writing.
	 */
	memcpy ( &undi_get_information->CurrentNodeAddress,
		 pxe_netdev->ll_addr,
		 sizeof ( undi_get_information->CurrentNodeAddress ) );
	memcpy ( &undi_get_information->PermNodeAddress,
		 pxe_netdev->ll_addr,
		 sizeof ( undi_get_information->PermNodeAddress ) );
	undi_get_information->ROMAddress = 0;
		/* nic.rom_info->rom_segment; */
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
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_statistics ( struct s_PXENV_UNDI_GET_STATISTICS
					 *undi_get_statistics ) {
	DBG ( "PXENV_UNDI_GET_STATISTICS" );

	undi_get_statistics->XmtGoodFrames = pxe_netdev->stats.tx_count;
	undi_get_statistics->RcvGoodFrames = pxe_netdev->stats.rx_count;
	undi_get_statistics->RcvCRCErrors = 0;
	undi_get_statistics->RcvResourceErrors = 0;

	undi_get_statistics->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_CLEAR_STATISTICS
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_clear_statistics ( struct s_PXENV_UNDI_CLEAR_STATISTICS
					   *undi_clear_statistics ) {
	DBG ( "PXENV_UNDI_CLEAR_STATISTICS" );

	memset ( &pxe_netdev->stats, 0, sizeof ( pxe_netdev->stats ) );

	undi_clear_statistics->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
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
 * Status: won't implement (would require driver API changes for no
 * perceptible benefit)
 */
PXENV_EXIT_t pxenv_undi_force_interrupt ( struct s_PXENV_UNDI_FORCE_INTERRUPT
					  *undi_force_interrupt ) {
	DBG ( "PXENV_UNDI_FORCE_INTERRUPT" );

	undi_force_interrupt->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
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
	struct device *dev = pxe_netdev->dev;

	DBG ( "PXENV_UNDI_GET_NIC_TYPE" );

	memset ( &undi_get_nic_type->info, 0,
		 sizeof ( undi_get_nic_type->info ) );

	switch ( dev->desc.bus_type ) {
	case BUS_TYPE_PCI: {
		struct pci_nic_info *info = &undi_get_nic_type->info.pci;

		undi_get_nic_type->NicType = PCI_NIC;
		info->Vendor_ID = dev->desc.vendor;
		info->Dev_ID = dev->desc.device;
		info->Base_Class = PCI_BASE_CLASS ( dev->desc.class );
		info->Sub_Class = PCI_SUB_CLASS ( dev->desc.class );
		info->Prog_Intf = PCI_PROG_INTF ( dev->desc.class );
		info->BusDevFunc = dev->desc.location;
		/* Cheat: remaining fields are probably unnecessary,
		 * and would require adding extra code to pci.c.
		 */
		undi_get_nic_type->info.pci.SubVendor_ID = 0xffff;
		undi_get_nic_type->info.pci.SubDevice_ID = 0xffff;
		break; }
	case BUS_TYPE_ISAPNP: {
		struct pnp_nic_info *info = &undi_get_nic_type->info.pnp;

		undi_get_nic_type->NicType = PnP_NIC;
		info->EISA_Dev_ID = ( ( dev->desc.vendor << 16 ) |
				      dev->desc.device );
		info->CardSelNum = dev->desc.location;
		/* Cheat: remaining fields are probably unnecessary,
		 * and would require adding extra code to isapnp.c.
		 */
		break; }
	default:
		undi_get_nic_type->Status = PXENV_STATUS_FAILURE;
		return PXENV_EXIT_FAILURE;
	}

	undi_get_nic_type->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_UNDI_GET_IFACE_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_undi_get_iface_info ( struct s_PXENV_UNDI_GET_IFACE_INFO
					 *undi_get_iface_info ) {
	DBG ( "PXENV_UNDI_GET_IFACE_INFO" );

	/* Just hand back some info, doesn't really matter what it is.
	 * Most PXE stacks seem to take this approach.
	 */
	snprintf ( ( char * ) undi_get_iface_info->IfaceType,
		   sizeof ( undi_get_iface_info->IfaceType ), "gPXE" );
	undi_get_iface_info->LinkSpeed = 10000000; /* 10 Mbps */
	undi_get_iface_info->ServiceFlags = 0;
	memset ( undi_get_iface_info->Reserved, 0,
		 sizeof(undi_get_iface_info->Reserved) );

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
	struct io_buffer *iobuf;
	size_t len;

	DBG ( "PXENV_UNDI_ISR" );

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
		DBG ( " START" );

		/* Call poll().  This should acknowledge the device
		 * interrupt and queue up any received packet.
		 */
		if ( netdev_poll ( pxe_netdev, -1U ) ) {
			/* Packet waiting in queue */
			DBG ( " OURS" );
			undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_OURS;
		} else {
			DBG ( " NOT_OURS" );
			undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_NOT_OURS;
		}
		break;
	case PXENV_UNDI_ISR_IN_PROCESS :
	case PXENV_UNDI_ISR_IN_GET_NEXT :
		DBG ( " PROCESS/GET_NEXT" );

		/* If we have not yet marked a TX as complete, and the
		 * netdev TX queue is empty, report the TX completion.
		 */
		if ( undi_tx_count && list_empty ( &pxe_netdev->tx_queue ) ) {
			undi_tx_count--;
			undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_TRANSMIT;
			break;
		}

		/* Remove first packet from netdev RX queue */
		iobuf = netdev_rx_dequeue ( pxe_netdev );
		if ( ! iobuf ) {
			/* No more packets remaining */
			undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_DONE;
			break;
		}

		/* Copy packet to base memory buffer */
		len = iob_len ( iobuf );
		DBG ( " RECEIVE %zd", len );
		if ( len > sizeof ( basemem_packet ) ) {
			/* Should never happen */
			len = sizeof ( basemem_packet );
		}
		memcpy ( basemem_packet, iobuf->data, len );

		/* Fill in UNDI_ISR structure */
		undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_RECEIVE;
		undi_isr->BufferLength = len;
		undi_isr->FrameLength = len;
		undi_isr->FrameHeaderLength =
			pxe_netdev->ll_protocol->ll_header_len;
		undi_isr->Frame.segment = rm_ds;
		undi_isr->Frame.offset =
			( ( unsigned ) & __from_data16 ( basemem_packet ) );
		/* Probably ought to fill in packet type */
		undi_isr->ProtType = P_UNKNOWN;
		undi_isr->PktType = XMT_DESTADDR;

		/* Free packet */
		free_iob ( iobuf );
		break;
	default :
		DBG ( " INVALID(%04x)", undi_isr->FuncFlag );

		/* Should never happen */
		undi_isr->FuncFlag = PXENV_UNDI_ISR_OUT_DONE;
		undi_isr->Status = PXENV_STATUS_UNDI_INVALID_PARAMETER;
		return PXENV_EXIT_FAILURE;
	}

	undi_isr->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}
