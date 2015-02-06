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

/** Ring refill profiler */
static struct profiler ncm_refill_profiler __profiler =
	{ .name = "ncm.refill" };

/** Interrupt completion profiler */
static struct profiler ncm_intr_profiler __profiler =
	{ .name = "ncm.intr" };

/** Bulk IN completion profiler */
static struct profiler ncm_in_profiler __profiler =
	{ .name = "ncm.in" };

/** Bulk IN per-datagram profiler */
static struct profiler ncm_in_datagram_profiler __profiler =
	{ .name = "ncm.in_dgram" };

/** Bulk OUT profiler */
static struct profiler ncm_out_profiler __profiler =
	{ .name = "ncm.out" };

/******************************************************************************
 *
 * Ring management
 *
 ******************************************************************************
 */

/**
 * Transcribe receive ring name (for debugging)
 *
 * @v ncm		CDC-NCM device
 * @v ring		Receive ring
 * @ret name		Receive ring name
 */
static inline const char * ncm_rx_name ( struct ncm_device *ncm,
					 struct ncm_rx_ring *ring ) {
	if ( ring == &ncm->intr ) {
		return "interrupt";
	} else if ( ring == &ncm->in ) {
		return "bulk IN";
	} else {
		return "UNKNOWN";
	}
}

/**
 * Allocate receive ring buffers
 *
 * @v ncm		CDC-NCM device
 * @v ring		Receive ring
 * @v mtu		I/O buffer size
 * @v count		Number of I/O buffers
 * @ret rc		Return status code
 */
static int ncm_rx_alloc ( struct ncm_device *ncm, struct ncm_rx_ring *ring,
			  size_t mtu, unsigned int count ) {
	struct io_buffer *iobuf;
	struct io_buffer *tmp;
	unsigned int i;
	int rc;

	/* Initialise ring */
	ring->mtu = mtu;
	INIT_LIST_HEAD ( &ring->list );

	/* Allocate I/O buffers */
	for ( i = 0 ; i < count ; i++ ) {
		iobuf = alloc_iob ( mtu );
		if ( ! iobuf ) {
			DBGC ( ncm, "NCM %p could not allocate %dx %zd-byte "
			       "buffers for %s\n", ncm, count, mtu,
			       ncm_rx_name ( ncm, ring ) );
			rc = -ENOMEM;
			goto err_alloc;
		}
		list_add ( &iobuf->list, &ring->list );
	}

	return 0;

 err_alloc:
	list_for_each_entry_safe ( iobuf, tmp, &ring->list, list ) {
		list_del ( &iobuf->list );
		free_iob ( iobuf );
	}
	return rc;
}

/**
 * Refill receive ring
 *
 * @v ncm		CDC-NCM device
 * @v ring		Receive ring
 * @ret rc		Return status code
 */
static int ncm_rx_refill ( struct ncm_device *ncm, struct ncm_rx_ring *ring ) {
	struct io_buffer *iobuf;
	int rc;

	/* Enqueue any recycled I/O buffers */
	while ( ( iobuf = list_first_entry ( &ring->list, struct io_buffer,
					     list ) ) ) {

		/* Profile refill */
		profile_start ( &ncm_refill_profiler );

		/* Reset size */
		iob_put ( iobuf, ( ring->mtu - iob_len ( iobuf ) ) );

		/* Enqueue I/O buffer */
		if ( ( rc = usb_stream ( &ring->ep, iobuf ) ) != 0 ) {
			DBGC ( ncm, "NCM %p could not enqueue %s: %s\n", ncm,
			       ncm_rx_name ( ncm, ring ), strerror ( rc ) );
			/* Leave in recycled list and wait for next refill */
			return rc;
		}

		/* Remove from recycled list */
		list_del ( &iobuf->list );
		profile_stop ( &ncm_refill_profiler );
	}

	return 0;
}

/**
 * Recycle receive buffer
 *
 * @v ncm		CDC-NCM device
 * @v ring		Receive ring
 * @v iobuf		I/O buffer
 */
static inline void ncm_rx_recycle ( struct ncm_device *ncm __unused,
				    struct ncm_rx_ring *ring,
				    struct io_buffer *iobuf ) {

	/* Add to recycled list */
	list_add_tail ( &iobuf->list, &ring->list );
}

/**
 * Free receive ring
 *
 * @v ncm		CDC-NCM device
 * @v ring		Receive ring
 */
static void ncm_rx_free ( struct ncm_device *ncm __unused,
			  struct ncm_rx_ring *ring ) {
	struct io_buffer *iobuf;
	struct io_buffer *tmp;

	/* Free I/O buffers */
	list_for_each_entry_safe ( iobuf, tmp, &ring->list, list ) {
		list_del ( &iobuf->list );
		free_iob ( iobuf );
	}
}

/******************************************************************************
 *
 * CDC-NCM communications interface
 *
 ******************************************************************************
 */

/**
 * Complete interrupt transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ncm_intr_complete ( struct usb_endpoint *ep,
				struct io_buffer *iobuf, int rc ) {
	struct ncm_device *ncm = container_of ( ep, struct ncm_device, intr.ep);
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
	/* Recycle buffer */
	ncm_rx_recycle ( ncm, &ncm->intr, iobuf );
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
	int rc;

	/* Allocate I/O buffers */
	if ( ( rc = ncm_rx_alloc ( ncm, &ncm->intr, ncm->intr.ep.mtu,
				   NCM_INTR_COUNT ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not allocate RX buffers: %s\n",
		       ncm, strerror ( rc ) );
		goto err_alloc;
	}

	/* Open interrupt endpoint */
	if ( ( rc = usb_endpoint_open ( &ncm->intr.ep ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not open interrupt: %s\n",
		       ncm, strerror ( rc ) );
		goto err_open;
	}

	return 0;

	usb_endpoint_close ( &ncm->intr.ep );
 err_open:
	ncm_rx_free ( ncm, &ncm->intr );
 err_alloc:
	return rc;
}

/**
 * Close communications interface
 *
 * @v ncm		CDC-NCM device
 */
static void ncm_comms_close ( struct ncm_device *ncm ) {

	/* Close interrupt endpoint */
	usb_endpoint_close ( &ncm->intr.ep );

	/* Free I/O buffers */
	ncm_rx_free ( ncm, &ncm->intr );
}

/******************************************************************************
 *
 * CDC-NCM data interface
 *
 ******************************************************************************
 */

/**
 * Allocate bulk IN receive ring buffers
 *
 * @v ncm		CDC-NCM device
 * @ret rc		Return status code
 */
static int ncm_in_alloc ( struct ncm_device *ncm ) {
	size_t mtu;
	unsigned int count;
	int rc;

	/* Some devices have a very small number of internal buffers,
	 * and rely on being able to pack multiple packets into each
	 * buffer.  We therefore want to use large buffers if
	 * possible.  However, large allocations have a reasonable
	 * chance of failure, especially if this is not the first or
	 * only device to be opened.
	 *
	 * We therefore attempt to find a usable buffer size, starting
	 * large and working downwards until allocation succeeds.
	 * Smaller buffers will still work, albeit with a higher
	 * chance of packet loss and so lower overall throughput.
	 */
	for ( mtu = ncm->mtu ; mtu >= NCM_MIN_NTB_INPUT_SIZE ; mtu >>= 1 ) {

		/* Attempt allocation at this MTU */
		if ( mtu > NCM_MAX_NTB_INPUT_SIZE )
			continue;
		count = ( NCM_IN_MIN_SIZE / mtu );
		if ( count < NCM_IN_MIN_COUNT )
			count = NCM_IN_MIN_COUNT;
		if ( ( count * mtu ) > NCM_IN_MAX_SIZE )
			continue;
		if ( ( rc = ncm_rx_alloc ( ncm, &ncm->in, mtu, count ) ) != 0 )
			continue;

		DBGC ( ncm, "NCM %p using %dx %zd-byte buffers for bulk IN\n",
		       ncm, count, mtu );
		return 0;
	}

	DBGC ( ncm, "NCM %p could not allocate bulk IN buffers\n", ncm );
	return -ENOMEM;
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
	struct ncm_device *ncm = container_of ( ep, struct ncm_device, in.ep );
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

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open )
		goto ignore;

	/* Record USB errors against the network device */
	if ( rc != 0 ) {
		DBGC ( ncm, "NCM %p bulk IN failed: %s\n",
		       ncm, strerror ( rc ) );
		goto drop;
	}

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

		/* Copy data to a new I/O buffer.  Our USB buffers may
		 * be very large and so we choose to recycle the
		 * buffers directly rather than attempt reallocation
		 * while the device is running.  We therefore copy the
		 * data to a new I/O buffer even if this is the only
		 * (or last) packet within the buffer.
		 */
		pkt = alloc_iob ( pkt_len );
		if ( ! pkt ) {
			/* Record error and continue */
			netdev_rx_err ( netdev, NULL, -ENOMEM );
			continue;
		}
		memcpy ( iob_put ( pkt, pkt_len ),
			 ( iobuf->data + pkt_offset ), pkt_len );

		/* Strip CRC, if present */
		if ( ndp->magic & cpu_to_le32 ( NCM_DATAGRAM_POINTER_MAGIC_CRC))
			iob_unput ( pkt, 4 /* CRC32 */ );

		/* Hand off to network stack */
		netdev_rx ( netdev, pkt );
		profile_stop ( &ncm_in_datagram_profiler );
	}

	/* Recycle I/O buffer */
	ncm_rx_recycle ( ncm, &ncm->in, iobuf );
	profile_stop ( &ncm_in_profiler );

	return;

 error:
	rc = -EIO;
 drop:
	/* Record error against network device */
	DBGC_HDA ( ncm, 0, iobuf->data, iob_len ( iobuf ) );
	netdev_rx_err ( netdev, NULL, rc );
 ignore:
	ncm_rx_recycle ( ncm, &ncm->in, iobuf );
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
	size_t header_len = ( sizeof ( *header ) + ncm->out.padding );
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
	header->nth.sequence = cpu_to_le16 ( ncm->out.sequence );
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
	if ( ( rc = usb_stream ( &ncm->out.ep, iobuf ) ) != 0 )
		return rc;

	/* Increment sequence number */
	ncm->out.sequence++;

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
	struct ncm_device *ncm = container_of ( ep, struct ncm_device, out.ep );
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

	/* Allocate I/O buffers */
	if ( ( rc = ncm_in_alloc ( ncm ) ) != 0 )
		goto err_alloc;

	/* Set maximum input size */
	memset ( &size, 0, sizeof ( size ) );
	size.mtu = cpu_to_le32 ( ncm->in.mtu );
	if ( ( rc = usb_control ( usb, NCM_SET_NTB_INPUT_SIZE, 0, ncm->comms,
				  &size, sizeof ( size ) ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not set input size to %zd: %s\n",
		       ncm, ncm->in.mtu, strerror ( rc ) );
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
	if ( ( rc = usb_endpoint_open ( &ncm->in.ep ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not open bulk IN: %s\n",
		       ncm, strerror ( rc ) );
		goto err_open_in;
	}

	/* Open bulk OUT endpoint */
	if ( ( rc = usb_endpoint_open ( &ncm->out.ep ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not open bulk OUT: %s\n",
		       ncm, strerror ( rc ) );
		goto err_open_out;
	}

	/* Reset transmit sequence number */
	ncm->out.sequence = 0;

	return 0;

	usb_endpoint_close ( &ncm->out.ep );
 err_open_out:
	usb_endpoint_close ( &ncm->in.ep );
 err_open_in:
	usb_set_interface ( usb, ncm->data, 0 );
 err_set_interface:
 err_set_ntb_input_size:
	ncm_rx_free ( ncm, &ncm->in );
 err_alloc:
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
	usb_endpoint_close ( &ncm->out.ep );
	usb_endpoint_close ( &ncm->in.ep );

	/* Reset data interface */
	usb_set_interface ( usb, ncm->data, 0 );

	/* Free I/O buffers */
	ncm_rx_free ( ncm, &ncm->in );
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
	ncm->out.sequence = 0;

	/* Open communications interface */
	if ( ( rc = ncm_comms_open ( ncm ) ) != 0 )
		goto err_comms_open;

	/* Refill interrupt ring */
	if ( ( rc = ncm_rx_refill ( ncm, &ncm->intr ) ) != 0 )
		goto err_intr_refill;

	/* Open data interface */
	if ( ( rc = ncm_data_open ( ncm ) ) != 0 )
		goto err_data_open;

	/* Refill bulk IN ring */
	if ( ( rc = ncm_rx_refill ( ncm, &ncm->in ) ) != 0 )
		goto err_in_refill;

	return 0;

 err_in_refill:
	ncm_data_close ( ncm );
 err_data_open:
 err_intr_refill:
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
	int rc;

	/* Poll USB bus */
	usb_poll ( ncm->bus );

	/* Refill interrupt ring */
	if ( ( rc = ncm_rx_refill ( ncm, &ncm->intr ) ) != 0 )
		netdev_rx_err ( netdev, NULL, rc );

	/* Refill bulk IN ring */
	if ( ( rc = ncm_rx_refill ( ncm, &ncm->in ) ) != 0 )
		netdev_rx_err ( netdev, NULL, rc );
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
	usb_endpoint_init ( &ncm->intr.ep, usb, &ncm_intr_operations );
	usb_endpoint_init ( &ncm->in.ep, usb, &ncm_in_operations );
	usb_endpoint_init ( &ncm->out.ep, usb, &ncm_out_operations );
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
	if ( ( rc = usb_endpoint_described ( &ncm->intr.ep, config, comms,
					     USB_INTERRUPT, 0 ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not describe interrupt endpoint: "
		       "%s\n", ncm, strerror ( rc ) );
		goto err_interrupt;
	}

	/* Describe bulk IN endpoint */
	if ( ( rc = usb_endpoint_described ( &ncm->in.ep, config, data,
					     USB_BULK_IN, 0 ) ) != 0 ) {
		DBGC ( ncm, "NCM %p could not describe bulk IN endpoint: "
		       "%s\n", ncm, strerror ( rc ) );
		goto err_bulk_in;
	}

	/* Describe bulk OUT endpoint */
	if ( ( rc = usb_endpoint_described ( &ncm->out.ep, config, data,
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

	/* Get maximum supported input size */
	ncm->mtu = le32_to_cpu ( params.in.mtu );
	DBGC2 ( ncm, "NCM %p maximum IN size is %zd bytes\n", ncm, ncm->mtu );

	/* Calculate transmit padding */
	ncm->out.padding = ( ( le16_to_cpu ( params.out.remainder ) -
			       sizeof ( struct ncm_ntb_header ) - ETH_HLEN ) &
			     ( le16_to_cpu ( params.out.divisor ) - 1 ) );
	DBGC2 ( ncm, "NCM %p using %zd-byte transmit padding\n",
		ncm, ncm->out.padding );
	assert ( ( ( sizeof ( struct ncm_ntb_header ) + ncm->out.padding +
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
