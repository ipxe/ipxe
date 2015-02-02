/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <errno.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/profile.h>
#include <ipxe/usb.h>
#include "ecm.h"
#include "ncm.h"

/** @file
 *
 * CDC-NCM USB Ethernet driver
 *
 */

/** Interrupt completion profiler */
static struct profiler ncm_intr_profiler __profiler =
	{ .name = "ncm.intr" };

/** Interrupt refill profiler */
static struct profiler ncm_intr_refill_profiler __profiler =
	{ .name = "ncm.intr_refill" };

/** Bulk IN completion profiler */
static struct profiler ncm_in_profiler __profiler =
	{ .name = "ncm.in" };

/** Bulk IN per-datagram profiler */
static struct profiler ncm_in_datagram_profiler __profiler =
	{ .name = "ncm.in_dgram" };

/** Bulk IN refill profiler */
static struct profiler ncm_in_refill_profiler __profiler =
	{ .name = "ncm.in_refill" };

/** Bulk OUT profiler */
static struct profiler ncm_out_profiler __profiler =
	{ .name = "ncm.out" };

/******************************************************************************
 *
 * CDC-NCM communications interface
 *
 ******************************************************************************
 */

/**
 * Refill interrupt ring
 *
 * @v ncm		CDC-NCM device
 */
static void ncm_intr_refill ( struct ncm_device *ncm ) {
	struct io_buffer *iobuf;
	size_t mtu = ncm->intr.mtu;
	int rc;

	/* Enqueue any available I/O buffers */
	while ( ( iobuf = list_first_entry ( &ncm->intrs, struct io_buffer,
					     list ) ) ) {

		/* Profile refill */
		profile_start ( &ncm_intr_refill_profiler );

		/* Reset size */
		iob_put ( iobuf, ( mtu - iob_len ( iobuf ) ) );

		/* Enqueue I/O buffer */
		if ( ( rc = usb_stream ( &ncm->intr, iobuf ) ) != 0 ) {
			DBGC ( ncm, "NCM %p could not enqueue interrupt: %s\n",
			       ncm, strerror ( rc ) );
			/* Leave in available list and wait for next refill */
			return;
		}

		/* Remove from available list */
		list_del ( &iobuf->list );
		profile_stop ( &ncm_intr_refill_profiler );
	}
}

/**
 * Complete interrupt transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ncm_intr_complete ( struct usb_endpoint *ep,
				struct io_buffer *iobuf, int rc ) {
	struct ncm_device *ncm = container_of ( ep, struct ncm_device, intr );
	struct net_device *netdev = ncm->netdev;
	struct usb_setup_packet *message;
	size_t len = iob_len ( iobuf );

	/* Profile completions */
	profile_start ( &ncm_intr_profiler );

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open )
		goto done;

	/* Ignore packets with errors */
	if ( rc != 0 ) {
		DBGC ( ncm, "NCM %p interrupt failed: %s\n",
		       ncm, strerror ( rc ) );
		DBGC_HDA ( ncm, 0, iobuf->data, iob_len ( iobuf ) );
		goto done;
	}

	/* Extract message header */
	if ( len < sizeof ( *message ) ) {
		DBGC ( ncm, "NCM %p underlength interrupt:\n", ncm );
		DBGC_HDA ( ncm, 0, iobuf->data, iob_len ( iobuf ) );
		goto done;
	}
	message = iobuf->data;

	/* Parse message header */
	switch ( message->request ) {

	case cpu_to_le16 ( CDC_NETWORK_CONNECTION ) :
		if ( message->value ) {
			DBGC ( ncm, "NCM %p link up\n", ncm );
			netdev_link_up ( netdev );
		} else {
			DBGC ( ncm, "NCM %p link down\n", ncm );
			netdev_link_down ( netdev );
		}
		break;

	case cpu_to_le16 ( CDC_CONNECTION_SPEED_CHANGE ) :
		/* Ignore */
		break;

	default:
		DBGC ( ncm, "NCM %p unrecognised interrupt:\n", ncm );
		DBGC_HDA ( ncm, 0, iobuf->data, iob_len ( iobuf ) );
		break;
	}

 done:
	/* Return I/O buffer to available list */
	list_add_tail ( &iobuf->list, &ncm->intrs );
	profile_stop ( &ncm_intr_profiler );
}

/** Interrupt endpoint operations */
static struct usb_endpoint_driver_operations ncm_intr_operations = {
	.complete = ncm_intr_complete,
};

/**
 * Open communications interface
 *
 * @v ncm		CDC-NCM device
 * @ret rc		Return status code
 */
static int ncm_comms_open ( struct ncm_device *ncm ) {
	struct io_buffer *iobuf;
	struct io_buffer *tmp;
	unsigned int i;
	int rc;

	/* Allocate I/O buffers */
	for ( i = 0 ; i < NCM_INTR_FILL ; i++ ) {
		iobuf = alloc_iob ( ncm->intr.mtu );
		if ( ! iobuf ) {
			rc = -ENOMEM;
			goto err_alloc_iob;
		}
		list_add ( &iobuf->list, &ncm->intrs );
	}

	/* Open interrupt endpoint */
	if ( ( rc = usb_endpoint_open ( &ncm->intr ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not open interrupt: %s\n",
		       ncm, strerror ( rc ) );
		goto err_open;
	}

	return 0;

	usb_endpoint_close ( &ncm->intr );
 err_open:
 err_alloc_iob:
	list_for_each_entry_safe ( iobuf, tmp, &ncm->intrs, list ) {
		list_del ( &iobuf->list );
		free_iob ( iobuf );
	}
	return rc;
}

/**
 * Close communications interface
 *
 * @v ncm		CDC-NCM device
 */
static void ncm_comms_close ( struct ncm_device *ncm ) {
	struct io_buffer *iobuf;
	struct io_buffer *tmp;

	/* Close interrupt endpoint */
	usb_endpoint_close ( &ncm->intr );

	/* Free I/O buffers */
	list_for_each_entry_safe ( iobuf, tmp, &ncm->intrs, list ) {
		list_del ( &iobuf->list );
		free_iob ( iobuf );
	}
}

/******************************************************************************
 *
 * CDC-NCM data interface
 *
 ******************************************************************************
 */

/**
 * Refill bulk IN ring
 *
 * @v ncm		CDC-NCM device
 */
static void ncm_in_refill ( struct ncm_device *ncm ) {
	struct net_device *netdev = ncm->netdev;
	struct io_buffer *iobuf;
	int rc;

	/* Refill ring */
	while ( ncm->fill < NCM_IN_FILL ) {

		/* Profile refill */
		profile_start ( &ncm_in_refill_profiler );

		/* Allocate I/O buffer */
		iobuf = alloc_iob ( NCM_NTB_INPUT_SIZE );
		if ( ! iobuf ) {
			/* Wait for next refill */
			break;
		}
		iob_put ( iobuf, NCM_NTB_INPUT_SIZE );

		/* Enqueue I/O buffer */
		if ( ( rc = usb_stream ( &ncm->in, iobuf ) ) != 0 ) {
			netdev_rx_err ( netdev, iobuf, rc );
			break;
		}

		/* Increment fill level */
		ncm->fill++;
		profile_stop ( &ncm_in_refill_profiler );
	}
}

/**
 * Complete bulk IN transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ncm_in_complete ( struct usb_endpoint *ep, struct io_buffer *iobuf,
			      int rc ) {
	struct ncm_device *ncm = container_of ( ep, struct ncm_device, in );
	struct net_device *netdev = ncm->netdev;
	struct ncm_transfer_header *nth;
	struct ncm_datagram_pointer *ndp;
	struct ncm_datagram_descriptor *desc;
	struct io_buffer *pkt;
	unsigned int remaining;
	size_t ndp_offset;
	size_t ndp_len;
	size_t pkt_offset;
	size_t pkt_len;
	size_t len;

	/* Profile overall bulk IN completion */
	profile_start ( &ncm_in_profiler );

	/* Decrement fill level */
	ncm->fill--;

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open )
		goto ignore;

	/* Record USB errors against the network device */
	if ( rc != 0 )
		goto drop;

	/* Locate transfer header */
	len = iob_len ( iobuf );
	if ( sizeof ( *nth ) > len ) {
		DBGC ( ncm, "NCM %p packet too short for NTH:\n", ncm );
		goto error;
	}
	nth = iobuf->data;

	/* Locate datagram pointer */
	ndp_offset = le16_to_cpu ( nth->offset );
	if ( ( ndp_offset + sizeof ( *ndp ) ) > len ) {
		DBGC ( ncm, "NCM %p packet too short for NDP:\n", ncm );
		goto error;
	}
	ndp = ( iobuf->data + ndp_offset );
	ndp_len = le16_to_cpu ( ndp->header_len );
	if ( ndp_len < offsetof ( typeof ( *ndp ), desc ) ) {
		DBGC ( ncm, "NCM %p NDP header length too short:\n", ncm );
		goto error;
	}
	if ( ( ndp_offset + ndp_len ) > len ) {
		DBGC ( ncm, "NCM %p packet too short for NDP:\n", ncm );
		goto error;
	}

	/* Process datagrams */
	remaining = ( ( ndp_len - offsetof ( typeof ( *ndp ), desc ) ) /
		      sizeof ( ndp->desc[0] ) );
	for ( desc = ndp->desc ; remaining && desc->offset ; remaining-- ) {

		/* Profile individual datagrams */
		profile_start ( &ncm_in_datagram_profiler );

		/* Locate datagram */
		pkt_offset = le16_to_cpu ( desc->offset );
		pkt_len = le16_to_cpu ( desc->len );
		if ( pkt_len < ETH_HLEN ) {
			DBGC ( ncm, "NCM %p underlength datagram:\n", ncm );
			goto error;
		}
		if ( ( pkt_offset + pkt_len ) > len ) {
			DBGC ( ncm, "NCM %p datagram exceeds packet:\n", ncm );
			goto error;
		}

		/* Move to next descriptor */
		desc++;

		/* Create new I/O buffer if necessary */
		if ( remaining && desc->offset ) {
			/* More packets remain: create new buffer */
			pkt = alloc_iob ( pkt_len );
			if ( ! pkt ) {
				/* Record error and continue */
				netdev_rx_err ( netdev, NULL, -ENOMEM );
				continue;
			}
			memcpy ( iob_put ( pkt, pkt_len ),
				 ( iobuf->data + pkt_offset ), pkt_len );
		} else {
			/* This is the last packet: use in situ */
			pkt = iob_disown ( iobuf );
			iob_pull ( pkt, pkt_offset );
			iob_unput ( pkt, ( iob_len ( pkt ) - pkt_len ) );
		}

		/* Strip CRC, if present */
		if ( ndp->magic & cpu_to_le32 ( NCM_DATAGRAM_POINTER_MAGIC_CRC))
			iob_unput ( pkt, 4 /* CRC32 */ );

		/* Hand off to network stack */
		netdev_rx ( netdev, pkt );
		profile_stop ( &ncm_in_datagram_profiler );
	}

	/* Free I/O buffer (if still present) */
	free_iob ( iobuf );

	profile_stop ( &ncm_in_profiler );
	return;

 error:
	rc = -EIO;
 drop:
	DBGC_HDA ( ncm, 0, iobuf->data, iob_len ( iobuf ) );
	netdev_rx_err ( netdev, iobuf, rc );
	return;

 ignore:
	free_iob ( iobuf );
	return;
}

/** Bulk IN endpoint operations */
static struct usb_endpoint_driver_operations ncm_in_operations = {
	.complete = ncm_in_complete,
};

/**
 * Transmit packet
 *
 * @v ncm		CDC-NCM device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int ncm_out_transmit ( struct ncm_device *ncm,
			      struct io_buffer *iobuf ) {
	struct ncm_ntb_header *header;
	size_t len = iob_len ( iobuf );
	size_t header_len = ( sizeof ( *header ) + ncm->padding );
	int rc;

	/* Profile transmissions */
	profile_start ( &ncm_out_profiler );

	/* Prepend header */
	if ( ( rc = iob_ensure_headroom ( iobuf, header_len ) ) != 0 )
		return rc;
	header = iob_push ( iobuf, header_len );

	/* Populate header */
	header->nth.magic = cpu_to_le32 ( NCM_TRANSFER_HEADER_MAGIC );
	header->nth.header_len = cpu_to_le16 ( sizeof ( header->nth ) );
	header->nth.sequence = cpu_to_le16 ( ncm->sequence );
	header->nth.len = cpu_to_le16 ( iob_len ( iobuf ) );
	header->nth.offset =
		cpu_to_le16 ( offsetof ( typeof ( *header ), ndp ) );
	header->ndp.magic = cpu_to_le32 ( NCM_DATAGRAM_POINTER_MAGIC );
	header->ndp.header_len = cpu_to_le16 ( sizeof ( header->ndp ) +
					       sizeof ( header->desc ) );
	header->ndp.offset = cpu_to_le16 ( 0 );
	header->desc[0].offset = cpu_to_le16 ( header_len );
	header->desc[0].len = cpu_to_le16 ( len );
	memset ( &header->desc[1], 0, sizeof ( header->desc[1] ) );

	/* Enqueue I/O buffer */
	if ( ( rc = usb_stream ( &ncm->out, iobuf ) ) != 0 )
		return rc;

	/* Increment sequence number */
	ncm->sequence++;

	profile_stop ( &ncm_out_profiler );
	return 0;
}

/**
 * Complete bulk OUT transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ncm_out_complete ( struct usb_endpoint *ep, struct io_buffer *iobuf,
			       int rc ) {
	struct ncm_device *ncm = container_of ( ep, struct ncm_device, out );
	struct net_device *netdev = ncm->netdev;

	/* Report TX completion */
	netdev_tx_complete_err ( netdev, iobuf, rc );
}

/** Bulk OUT endpoint operations */
static struct usb_endpoint_driver_operations ncm_out_operations = {
	.complete = ncm_out_complete,
};

/**
 * Open data interface
 *
 * @v ncm		CDC-NCM device
 * @ret rc		Return status code
 */
static int ncm_data_open ( struct ncm_device *ncm ) {
	struct usb_device *usb = ncm->usb;
	struct ncm_set_ntb_input_size size;
	int rc;

	/* Set maximum input size */
	memset ( &size, 0, sizeof ( size ) );
	size.mtu = cpu_to_le32 ( NCM_NTB_INPUT_SIZE );
	if ( ( rc = usb_control ( usb, NCM_SET_NTB_INPUT_SIZE, 0, ncm->comms,
				  &size, sizeof ( size ) ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not set input size: %s\n",
		       ncm, strerror ( rc ) );
		goto err_set_ntb_input_size;
	}

	/* Select alternate setting for data interface */
	if ( ( rc = usb_set_interface ( usb, ncm->data,
					NCM_DATA_ALTERNATE ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not set alternate interface: %s\n",
		       ncm, strerror ( rc ) );
		goto err_set_interface;
	}

	/* Open bulk IN endpoint */
	if ( ( rc = usb_endpoint_open ( &ncm->in ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not open bulk IN: %s\n",
		       ncm, strerror ( rc ) );
		goto err_open_in;
	}

	/* Open bulk OUT endpoint */
	if ( ( rc = usb_endpoint_open ( &ncm->out ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not open bulk OUT: %s\n",
		       ncm, strerror ( rc ) );
		goto err_open_out;
	}

	return 0;

	usb_endpoint_close ( &ncm->out );
 err_open_out:
	usb_endpoint_close ( &ncm->in );
 err_open_in:
	usb_set_interface ( usb, ncm->data, 0 );
 err_set_interface:
 err_set_ntb_input_size:
	return rc;
}

/**
 * Close data interface
 *
 * @v ncm		CDC-NCM device
 */
static void ncm_data_close ( struct ncm_device *ncm ) {
	struct usb_device *usb = ncm->usb;

	/* Close endpoints */
	usb_endpoint_close ( &ncm->out );
	usb_endpoint_close ( &ncm->in );

	/* Reset data interface */
	usb_set_interface ( usb, ncm->data, 0 );
}

/******************************************************************************
 *
 * Network device interface
 *
 ******************************************************************************
 */

/**
 * Open network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ncm_open ( struct net_device *netdev ) {
	struct ncm_device *ncm = netdev->priv;
	int rc;

	/* Reset sequence number */
	ncm->sequence = 0;

	/* Open communications interface */
	if ( ( rc = ncm_comms_open ( ncm ) ) != 0 )
		goto err_comms_open;

	/* Refill interrupt ring */
	ncm_intr_refill ( ncm );

	/* Open data interface */
	if ( ( rc = ncm_data_open ( ncm ) ) != 0 )
		goto err_data_open;

	/* Refill bulk IN ring */
	ncm_in_refill ( ncm );

	return 0;

	ncm_data_close ( ncm );
 err_data_open:
	ncm_comms_close ( ncm );
 err_comms_open:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void ncm_close ( struct net_device *netdev ) {
	struct ncm_device *ncm = netdev->priv;

	/* Close data interface */
	ncm_data_close ( ncm );

	/* Close communications interface */
	ncm_comms_close ( ncm );

	/* Sanity check */
	assert ( ncm->fill == 0 );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int ncm_transmit ( struct net_device *netdev,
			  struct io_buffer *iobuf ) {
	struct ncm_device *ncm = netdev->priv;
	int rc;

	/* Transmit packet */
	if ( ( rc = ncm_out_transmit ( ncm, iobuf ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void ncm_poll ( struct net_device *netdev ) {
	struct ncm_device *ncm = netdev->priv;

	/* Poll USB bus */
	usb_poll ( ncm->bus );

	/* Refill interrupt ring */
	ncm_intr_refill ( ncm );

	/* Refill bulk IN ring */
	ncm_in_refill ( ncm );
}

/** CDC-NCM network device operations */
static struct net_device_operations ncm_operations = {
	.open		= ncm_open,
	.close		= ncm_close,
	.transmit	= ncm_transmit,
	.poll		= ncm_poll,
};

/******************************************************************************
 *
 * USB interface
 *
 ******************************************************************************
 */

/**
 * Probe device
 *
 * @v func		USB function
 * @v config		Configuration descriptor
 * @ret rc		Return status code
 */
static int ncm_probe ( struct usb_function *func,
		       struct usb_configuration_descriptor *config ) {
	struct usb_device *usb = func->usb;
	struct net_device *netdev;
	struct ncm_device *ncm;
	struct usb_interface_descriptor *comms;
	struct usb_interface_descriptor *data;
	struct ecm_ethernet_descriptor *ethernet;
	struct ncm_ntb_parameters params;
	int rc;

	/* Allocate and initialise structure */
	netdev = alloc_etherdev ( sizeof ( *ncm ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &ncm_operations );
	netdev->dev = &func->dev;
	ncm = netdev->priv;
	memset ( ncm, 0, sizeof ( *ncm ) );
	ncm->usb = usb;
	ncm->bus = usb->port->hub->bus;
	ncm->netdev = netdev;
	usb_endpoint_init ( &ncm->intr, usb, &ncm_intr_operations );
	usb_endpoint_init ( &ncm->in, usb, &ncm_in_operations );
	usb_endpoint_init ( &ncm->out, usb, &ncm_out_operations );
	INIT_LIST_HEAD ( &ncm->intrs );
	DBGC ( ncm, "NCM %p on %s\n", ncm, func->name );

	/* Identify interfaces */
	if ( func->count < NCM_INTERFACE_COUNT ) {
		DBGC ( ncm, "NCM %p has only %d interfaces\n",
		       ncm, func->count );
		rc = -EINVAL;
		goto err_count;
	}
	ncm->comms = func->interface[NCM_INTERFACE_COMMS];
	ncm->data = func->interface[NCM_INTERFACE_DATA];

	/* Locate communications interface descriptor */
	comms = usb_interface_descriptor ( config, ncm->comms, 0 );
	if ( ! comms ) {
		DBGC ( ncm, "NCM %p has no communications interface\n", ncm );
		rc = -EINVAL;
		goto err_comms;
	}

	/* Locate data interface descriptor */
	data = usb_interface_descriptor ( config, ncm->data,
					  NCM_DATA_ALTERNATE );
	if ( ! data ) {
		DBGC ( ncm, "NCM %p has no data interface\n", ncm );
		rc = -EINVAL;
		goto err_data;
	}

	/* Describe interrupt endpoint */
	if ( ( rc = usb_endpoint_described ( &ncm->intr, config, comms,
					     USB_INTERRUPT, 0 ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not describe interrupt endpoint: "
		       "%s\n", ncm, strerror ( rc ) );
		goto err_interrupt;
	}

	/* Describe bulk IN endpoint */
	if ( ( rc = usb_endpoint_described ( &ncm->in, config, data,
					     USB_BULK_IN, 0 ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not describe bulk IN endpoint: "
		       "%s\n", ncm, strerror ( rc ) );
		goto err_bulk_in;
	}

	/* Describe bulk OUT endpoint */
	if ( ( rc = usb_endpoint_described ( &ncm->out, config, data,
					     USB_BULK_OUT, 0 ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not describe bulk OUT endpoint: "
		       "%s\n", ncm, strerror ( rc ) );
		goto err_bulk_out;
	}

	/* Locate Ethernet descriptor */
	ethernet = ecm_ethernet_descriptor ( config, comms );
	if ( ! ethernet ) {
		DBGC ( ncm, "NCM %p has no Ethernet descriptor\n", ncm );
		rc = -EINVAL;
		goto err_ethernet;
	}

	/* Fetch MAC address */
	if ( ( rc = ecm_fetch_mac ( usb, ethernet, netdev->hw_addr ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not fetch MAC address: %s\n",
		       ncm, strerror ( rc ) );
		goto err_fetch_mac;
	}

	/* Get NTB parameters */
	if ( ( rc = usb_control ( usb, NCM_GET_NTB_PARAMETERS, 0, ncm->comms,
				  &params, sizeof ( params ) ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not get NTB parameters: %s\n",
		       ncm, strerror ( rc ) );
		goto err_ntb_parameters;
	}

	/* Calculate transmit padding */
	ncm->padding = ( ( le16_to_cpu ( params.out.remainder ) -
			   sizeof ( struct ncm_ntb_header ) - ETH_HLEN ) &
			 ( le16_to_cpu ( params.out.divisor ) - 1 ) );
	DBGC2 ( ncm, "NCM %p using %zd-byte transmit padding\n",
		ncm, ncm->padding );
	assert ( ( ( sizeof ( struct ncm_ntb_header ) + ncm->padding +
		     ETH_HLEN ) % le16_to_cpu ( params.out.divisor ) ) ==
		 le16_to_cpu ( params.out.remainder ) );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;

	usb_func_set_drvdata ( func, ncm );
	return 0;

	unregister_netdev ( netdev );
 err_register:
 err_ntb_parameters:
 err_fetch_mac:
 err_ethernet:
 err_bulk_out:
 err_bulk_in:
 err_interrupt:
 err_data:
 err_comms:
 err_count:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
 err_alloc:
	return rc;
}

/**
 * Remove device
 *
 * @v func		USB function
 */
static void ncm_remove ( struct usb_function *func ) {
	struct ncm_device *ncm = usb_func_get_drvdata ( func );
	struct net_device *netdev = ncm->netdev;

	unregister_netdev ( netdev );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** CDC-NCM device IDs */
static struct usb_device_id ncm_ids[] = {
	{
		.name = "cdc-ncm",
		.vendor = USB_ANY_ID,
		.product = USB_ANY_ID,
		.class = {
			.class = USB_CLASS_CDC,
			.subclass = USB_SUBCLASS_CDC_NCM,
			.protocol = 0,
		},
	},
};

/** CDC-NCM driver */
struct usb_driver ncm_driver __usb_driver = {
	.ids = ncm_ids,
	.id_count = ( sizeof ( ncm_ids ) / sizeof ( ncm_ids[0] ) ),
	.probe = ncm_probe,
	.remove = ncm_remove,
};
