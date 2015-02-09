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

#include <stdint.h>
#include <errno.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>
#include <ipxe/if_ether.h>
#include <ipxe/base16.h>
#include <ipxe/profile.h>
#include <ipxe/usb.h>
#include "ecm.h"

/** @file
 *
 * CDC-ECM USB Ethernet driver
 *
 */

/** Refill profiler */
static struct profiler ecm_refill_profiler __profiler =
	{ .name = "ecm.refill" };

/** Interrupt completion profiler */
static struct profiler ecm_intr_profiler __profiler =
	{ .name = "ecm.intr" };

/** Bulk IN completion profiler */
static struct profiler ecm_in_profiler __profiler =
	{ .name = "ecm.in" };

/** Bulk OUT profiler */
static struct profiler ecm_out_profiler __profiler =
	{ .name = "ecm.out" };

/******************************************************************************
 *
 * Ethernet functional descriptor
 *
 ******************************************************************************
 */

/**
 * Locate Ethernet functional descriptor
 *
 * @v config		Configuration descriptor
 * @v interface		Interface descriptor
 * @ret desc		Descriptor, or NULL if not found
 */
struct ecm_ethernet_descriptor *
ecm_ethernet_descriptor ( struct usb_configuration_descriptor *config,
			  struct usb_interface_descriptor *interface ) {
	struct ecm_ethernet_descriptor *desc;

	for_each_interface_descriptor ( desc, config, interface ) {
		if ( ( desc->header.type == USB_CS_INTERFACE_DESCRIPTOR ) &&
		     ( desc->subtype == CDC_SUBTYPE_ETHERNET ) )
			return desc;
	}
	return NULL;
}

/**
 * Get hardware MAC address
 *
 * @v usb		USB device
 * @v desc		Ethernet functional descriptor
 * @v hw_addr		Hardware address to fill in
 * @ret rc		Return status code
 */
int ecm_fetch_mac ( struct usb_device *usb,
		    struct ecm_ethernet_descriptor *desc, uint8_t *hw_addr ) {
	char buf[ base16_encoded_len ( ETH_ALEN ) + 1 /* NUL */ ];
	int len;
	int rc;

	/* Fetch MAC address string */
	len = usb_get_string_descriptor ( usb, desc->mac, 0, buf,
					  sizeof ( buf ) );
	if ( len < 0 ) {
		rc = len;
		return rc;
	}

	/* Sanity check */
	if ( len != ( ( int ) ( sizeof ( buf ) - 1 /* NUL */ ) ) )
		return -EINVAL;

	/* Decode MAC address */
	len = base16_decode ( buf, hw_addr );
	if ( len < 0 ) {
		rc = len;
		return rc;
	}

	return 0;
}

/******************************************************************************
 *
 * Ring management
 *
 ******************************************************************************
 */

/**
 * Transcribe receive ring name (for debugging)
 *
 * @v ecm		CDC-ECM device
 * @v ring		Receive ring
 * @ret name		Receive ring name
 */
static inline const char * ecm_rx_name ( struct ecm_device *ecm,
					 struct ecm_rx_ring *ring ) {
	if ( ring == &ecm->intr ) {
		return "interrupt";
	} else if ( ring == &ecm->in ) {
		return "bulk IN";
	} else {
		return "UNKNOWN";
	}
}

/**
 * Refill receive ring
 *
 * @v ecm		CDC-ECM device
 * @v ring		Receive ring
 */
static void ecm_rx_refill ( struct ecm_device *ecm, struct ecm_rx_ring *ring ) {
	struct net_device *netdev = ecm->netdev;
	struct io_buffer *iobuf;
	int rc;

	/* Refill ring */
	while ( ring->fill < ring->max ) {

		/* Profile refill */
		profile_start ( &ecm_refill_profiler );

		/* Allocate I/O buffer */
		iobuf = alloc_iob ( ring->mtu );
		if ( ! iobuf ) {
			/* Wait for next refill */
			break;
		}
		iob_put ( iobuf, ring->mtu );

		/* Enqueue I/O buffer */
		if ( ( rc = usb_stream ( &ring->ep, iobuf, 0 ) ) != 0 ) {
			netdev_rx_err ( netdev, iob_disown ( iobuf ), rc );
			break;
		}

		/* Increment fill level */
		ring->fill++;
		profile_stop ( &ecm_refill_profiler );
	}
}

/******************************************************************************
 *
 * CDC-ECM communications interface
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
static void ecm_intr_complete ( struct usb_endpoint *ep,
				struct io_buffer *iobuf, int rc ) {
	struct ecm_device *ecm = container_of ( ep, struct ecm_device, intr.ep);
	struct net_device *netdev = ecm->netdev;
	struct usb_setup_packet *message;
	size_t len = iob_len ( iobuf );

	/* Profile completions */
	profile_start ( &ecm_intr_profiler );

	/* Decrement fill level */
	assert ( ecm->intr.fill > 0 );
	ecm->intr.fill--;

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open )
		goto ignore;

	/* Drop packets with errors */
	if ( rc != 0 ) {
		DBGC ( ecm, "ECM %p interrupt failed: %s\n",
		       ecm, strerror ( rc ) );
		DBGC_HDA ( ecm, 0, iobuf->data, iob_len ( iobuf ) );
		goto error;
	}

	/* Extract message header */
	if ( len < sizeof ( *message ) ) {
		DBGC ( ecm, "ECM %p underlength interrupt:\n", ecm );
		DBGC_HDA ( ecm, 0, iobuf->data, iob_len ( iobuf ) );
		goto error;
	}
	message = iobuf->data;

	/* Parse message header */
	switch ( message->request ) {

	case cpu_to_le16 ( CDC_NETWORK_CONNECTION ) :
		if ( message->value && ! netdev_link_ok ( netdev ) ) {
			DBGC ( ecm, "ECM %p link up\n", ecm );
			netdev_link_up ( netdev );
		} else if ( netdev_link_ok ( netdev ) && ! message->value ) {
			DBGC ( ecm, "ECM %p link down\n", ecm );
			netdev_link_down ( netdev );
		}
		break;

	case cpu_to_le16 ( CDC_CONNECTION_SPEED_CHANGE ) :
		/* Ignore */
		break;

	default:
		DBGC ( ecm, "ECM %p unrecognised interrupt:\n", ecm );
		DBGC_HDA ( ecm, 0, iobuf->data, iob_len ( iobuf ) );
		goto error;
	}

	/* Free I/O buffer */
	free_iob ( iobuf );
	profile_stop ( &ecm_intr_profiler );

	return;

 error:
	netdev_rx_err ( netdev, iob_disown ( iobuf ), rc );
 ignore:
	free_iob ( iobuf );
	return;
}

/** Interrupt endpoint operations */
static struct usb_endpoint_driver_operations ecm_intr_operations = {
	.complete = ecm_intr_complete,
};

/**
 * Open communications interface
 *
 * @v ecm		CDC-ECM device
 * @ret rc		Return status code
 */
static int ecm_comms_open ( struct ecm_device *ecm ) {
	int rc;

	/* Open interrupt endpoint */
	if ( ( rc = usb_endpoint_open ( &ecm->intr.ep ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not open interrupt: %s\n",
		       ecm, strerror ( rc ) );
		goto err_open;
	}

	/* Refill interrupt ring */
	ecm_rx_refill ( ecm, &ecm->intr );

	return 0;

	usb_endpoint_close ( &ecm->intr.ep );
	assert ( ecm->intr.fill == 0 );
 err_open:
	return rc;
}

/**
 * Close communications interface
 *
 * @v ecm		CDC-ECM device
 */
static void ecm_comms_close ( struct ecm_device *ecm ) {

	/* Close interrupt endpoint */
	usb_endpoint_close ( &ecm->intr.ep );
	assert ( ecm->intr.fill == 0 );
}

/******************************************************************************
 *
 * CDC-ECM data interface
 *
 ******************************************************************************
 */

/**
 * Complete bulk IN transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ecm_in_complete ( struct usb_endpoint *ep, struct io_buffer *iobuf,
			      int rc ) {
	struct ecm_device *ecm = container_of ( ep, struct ecm_device, in.ep );
	struct net_device *netdev = ecm->netdev;

	/* Profile receive completions */
	profile_start ( &ecm_in_profiler );

	/* Decrement fill level */
	assert ( ecm->in.fill > 0 );
	ecm->in.fill--;

	/* Ignore packets cancelled when the endpoint closes */
	if ( ! ep->open )
		goto ignore;

	/* Record USB errors against the network device */
	if ( rc != 0 ) {
		DBGC ( ecm, "ECM %p bulk IN failed: %s\n",
		       ecm, strerror ( rc ) );
		goto error;
	}

	/* Hand off to network stack */
	netdev_rx ( netdev, iob_disown ( iobuf ) );

	profile_stop ( &ecm_in_profiler );
	return;

 error:
	netdev_rx_err ( netdev, iob_disown ( iobuf ), rc );
 ignore:
	free_iob ( iobuf );
}

/** Bulk IN endpoint operations */
static struct usb_endpoint_driver_operations ecm_in_operations = {
	.complete = ecm_in_complete,
};

/**
 * Transmit packet
 *
 * @v ecm		CDC-ECM device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int ecm_out_transmit ( struct ecm_device *ecm,
			      struct io_buffer *iobuf ) {
	int rc;

	/* Profile transmissions */
	profile_start ( &ecm_out_profiler );

	/* Enqueue I/O buffer */
	if ( ( rc = usb_stream ( &ecm->out.ep, iobuf, 1 ) ) != 0 )
		return rc;

	profile_stop ( &ecm_out_profiler );
	return 0;
}

/**
 * Complete bulk OUT transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ecm_out_complete ( struct usb_endpoint *ep, struct io_buffer *iobuf,
			       int rc ) {
	struct ecm_device *ecm = container_of ( ep, struct ecm_device, out.ep );
	struct net_device *netdev = ecm->netdev;

	/* Report TX completion */
	netdev_tx_complete_err ( netdev, iobuf, rc );
}

/** Bulk OUT endpoint operations */
static struct usb_endpoint_driver_operations ecm_out_operations = {
	.complete = ecm_out_complete,
};

/**
 * Open data interface
 *
 * @v ecm		CDC-ECM device
 * @ret rc		Return status code
 */
static int ecm_data_open ( struct ecm_device *ecm ) {
	struct usb_device *usb = ecm->usb;
	int rc;

	/* Select alternate setting for data interface */
	if ( ( rc = usb_set_interface ( usb, ecm->data,
					ECM_DATA_ALTERNATE ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not set alternate interface: %s\n",
		       ecm, strerror ( rc ) );
		goto err_set_interface;
	}

	/* Open bulk IN endpoint */
	if ( ( rc = usb_endpoint_open ( &ecm->in.ep ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not open bulk IN: %s\n",
		       ecm, strerror ( rc ) );
		goto err_open_in;
	}

	/* Open bulk OUT endpoint */
	if ( ( rc = usb_endpoint_open ( &ecm->out.ep ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not open bulk OUT: %s\n",
		       ecm, strerror ( rc ) );
		goto err_open_out;
	}

	/* Refill bulk IN ring */
	ecm_rx_refill ( ecm, &ecm->in );

	return 0;

	usb_endpoint_close ( &ecm->out.ep );
 err_open_out:
	usb_endpoint_close ( &ecm->in.ep );
	assert ( ecm->in.fill == 0 );
 err_open_in:
	usb_set_interface ( usb, ecm->data, 0 );
 err_set_interface:
	return rc;
}

/**
 * Close data interface
 *
 * @v ecm		CDC-ECM device
 */
static void ecm_data_close ( struct ecm_device *ecm ) {
	struct usb_device *usb = ecm->usb;

	/* Close endpoints */
	usb_endpoint_close ( &ecm->out.ep );
	usb_endpoint_close ( &ecm->in.ep );
	assert ( ecm->in.fill == 0 );

	/* Reset data interface */
	usb_set_interface ( usb, ecm->data, 0 );
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
static int ecm_open ( struct net_device *netdev ) {
	struct ecm_device *ecm = netdev->priv;
	struct usb_device *usb = ecm->usb;
	unsigned int filter;
	int rc;

	/* Open communications interface */
	if ( ( rc = ecm_comms_open ( ecm ) ) != 0 )
		goto err_comms_open;

	/* Open data interface */
	if ( ( rc = ecm_data_open ( ecm ) ) != 0 )
		goto err_data_open;

	/* Set packet filter */
	filter = ( ECM_PACKET_TYPE_PROMISCUOUS |
		   ECM_PACKET_TYPE_ALL_MULTICAST |
		   ECM_PACKET_TYPE_DIRECTED |
		   ECM_PACKET_TYPE_BROADCAST );
	if ( ( rc = usb_control ( usb, ECM_SET_ETHERNET_PACKET_FILTER,
				  filter, ecm->comms, NULL, 0 ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not set packet filter: %s\n",
		       ecm, strerror ( rc ) );
		goto err_set_filter;
	}

	return 0;

 err_set_filter:
	ecm_data_close ( ecm );
 err_data_open:
	ecm_comms_close ( ecm );
 err_comms_open:
	return rc;
}

/**
 * Close network device
 *
 * @v netdev		Network device
 */
static void ecm_close ( struct net_device *netdev ) {
	struct ecm_device *ecm = netdev->priv;

	/* Close data interface */
	ecm_data_close ( ecm );

	/* Close communications interface */
	ecm_comms_close ( ecm );
}

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int ecm_transmit ( struct net_device *netdev,
			  struct io_buffer *iobuf ) {
	struct ecm_device *ecm = netdev->priv;
	int rc;

	/* Transmit packet */
	if ( ( rc = ecm_out_transmit ( ecm, iobuf ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Poll for completed and received packets
 *
 * @v netdev		Network device
 */
static void ecm_poll ( struct net_device *netdev ) {
	struct ecm_device *ecm = netdev->priv;

	/* Poll USB bus */
	usb_poll ( ecm->bus );

	/* Refill interrupt ring */
	ecm_rx_refill ( ecm, &ecm->intr );

	/* Refill bulk IN ring */
	ecm_rx_refill ( ecm, &ecm->in );
}

/** CDC-ECM network device operations */
static struct net_device_operations ecm_operations = {
	.open		= ecm_open,
	.close		= ecm_close,
	.transmit	= ecm_transmit,
	.poll		= ecm_poll,
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
static int ecm_probe ( struct usb_function *func,
		       struct usb_configuration_descriptor *config ) {
	struct usb_device *usb = func->usb;
	struct net_device *netdev;
	struct ecm_device *ecm;
	struct usb_interface_descriptor *comms;
	struct usb_interface_descriptor *data;
	struct ecm_ethernet_descriptor *ethernet;
	int rc;

	/* Allocate and initialise structure */
	netdev = alloc_etherdev ( sizeof ( *ecm ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	netdev_init ( netdev, &ecm_operations );
	netdev->dev = &func->dev;
	ecm = netdev->priv;
	memset ( ecm, 0, sizeof ( *ecm ) );
	ecm->usb = usb;
	ecm->bus = usb->port->hub->bus;
	ecm->netdev = netdev;
	usb_endpoint_init ( &ecm->intr.ep, usb, &ecm_intr_operations );
	usb_endpoint_init ( &ecm->in.ep, usb, &ecm_in_operations );
	usb_endpoint_init ( &ecm->out.ep, usb, &ecm_out_operations );
	DBGC ( ecm, "ECM %p on %s\n", ecm, func->name );

	/* Identify interfaces */
	if ( func->count < ECM_INTERFACE_COUNT ) {
		DBGC ( ecm, "ECM %p has only %d interfaces\n",
		       ecm, func->count );
		rc = -EINVAL;
		goto err_count;
	}
	ecm->comms = func->interface[ECM_INTERFACE_COMMS];
	ecm->data = func->interface[ECM_INTERFACE_DATA];

	/* Locate communications interface descriptor */
	comms = usb_interface_descriptor ( config, ecm->comms, 0 );
	if ( ! comms ) {
		DBGC ( ecm, "ECM %p has no communications interface\n", ecm );
		rc = -EINVAL;
		goto err_comms;
	}

	/* Locate data interface descriptor */
	data = usb_interface_descriptor ( config, ecm->data,
					  ECM_DATA_ALTERNATE );
	if ( ! data ) {
		DBGC ( ecm, "ECM %p has no data interface\n", ecm );
		rc = -EINVAL;
		goto err_data;
	}

	/* Describe interrupt endpoint */
	if ( ( rc = usb_endpoint_described ( &ecm->intr.ep, config, comms,
					     USB_INTERRUPT, 0 ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not describe interrupt endpoint: "
		       "%s\n", ecm, strerror ( rc ) );
		goto err_interrupt;
	}
	ecm->intr.mtu = ecm->intr.ep.mtu;
	ecm->intr.max = ECM_INTR_MAX_FILL;

	/* Describe bulk IN endpoint */
	if ( ( rc = usb_endpoint_described ( &ecm->in.ep, config, data,
					     USB_BULK_IN, 0 ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not describe bulk IN endpoint: "
		       "%s\n", ecm, strerror ( rc ) );
		goto err_bulk_in;
	}
	ecm->in.mtu = ECM_IN_MTU;
	ecm->in.max = ECM_IN_MAX_FILL;

	/* Describe bulk OUT endpoint */
	if ( ( rc = usb_endpoint_described ( &ecm->out.ep, config, data,
					     USB_BULK_OUT, 0 ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not describe bulk OUT endpoint: "
		       "%s\n", ecm, strerror ( rc ) );
		goto err_bulk_out;
	}

	/* Locate Ethernet descriptor */
	ethernet = ecm_ethernet_descriptor ( config, comms );
	if ( ! ethernet ) {
		DBGC ( ecm, "ECM %p has no Ethernet descriptor\n", ecm );
		rc = -EINVAL;
		goto err_ethernet;
	}

	/* Fetch MAC address */
	if ( ( rc = ecm_fetch_mac ( usb, ethernet, netdev->hw_addr ) ) != 0 ) {
		DBGC ( ecm, "ECM %p could not fetch MAC address: %s\n",
		       ecm, strerror ( rc ) );
		goto err_fetch_mac;
	}

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;

	usb_func_set_drvdata ( func, ecm );
	return 0;

	unregister_netdev ( netdev );
 err_register:
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
static void ecm_remove ( struct usb_function *func ) {
	struct ecm_device *ecm = usb_func_get_drvdata ( func );
	struct net_device *netdev = ecm->netdev;

	unregister_netdev ( netdev );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** CDC-ECM device IDs */
static struct usb_device_id ecm_ids[] = {
	{
		.name = "cdc-ecm",
		.vendor = USB_ANY_ID,
		.product = USB_ANY_ID,
		.class = {
			.class = USB_CLASS_CDC,
			.subclass = USB_SUBCLASS_CDC_ECM,
			.protocol = 0,
		},
	},
};

/** CDC-ECM driver */
struct usb_driver ecm_driver __usb_driver = {
	.ids = ecm_ids,
	.id_count = ( sizeof ( ecm_ids ) / sizeof ( ecm_ids[0] ) ),
	.probe = ecm_probe,
	.remove = ecm_remove,
};
