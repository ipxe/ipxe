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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/usb.h>

/** @file
 *
 * Universal Serial Bus (USB)
 *
 */

/******************************************************************************
 *
 * Utility functions
 *
 ******************************************************************************
 */

/**
 * Get USB endpoint name (for debugging)
 *
 * @v address		Endpoint address
 * @ret name		Endpoint name
 */
static inline const char * usb_endpoint_name ( unsigned int address ) {
	static char buf[ 9 /* "EPxx OUT" + NUL */ ];

	snprintf ( buf, sizeof ( buf ), "EP%d%s",
		   ( address & USB_ENDPOINT_MAX ),
		   ( address ?
		     ( ( address & USB_ENDPOINT_IN ) ? " IN" : " OUT" ) : "" ));
	return buf;
}

/**
 * Get USB speed name (for debugging)
 *
 * @v speed		Speed
 * @ret name		Speed name
 */
static inline const char * usb_speed_name ( unsigned int speed ) {
	static const char *exponents[4] = { "", "k", "M", "G" };
	static char buf[ 10 /* "xxxxxXbps" + NUL */ ];
	unsigned int mantissa;
	unsigned int exponent;

	/* Extract mantissa and exponent */
	mantissa = USB_SPEED_MANTISSA ( speed );
	exponent = USB_SPEED_EXPONENT ( speed );

	/* Name speed */
	switch ( speed ) {
	case USB_SPEED_NONE:		return "DETACHED";
	case USB_SPEED_LOW:		return "low";
	case USB_SPEED_FULL:		return "full";
	case USB_SPEED_HIGH:		return "high";
	case USB_SPEED_SUPER:		return "super";
	default:
		snprintf ( buf, sizeof ( buf ), "%d%sbps",
			   mantissa, exponents[exponent] );
		return buf;
	}
}

/**
 * Transcribe USB BCD-coded value (for debugging)
 *
 * @v bcd		BCD-coded value
 * @ret string		Transcribed value
 */
static inline const char * usb_bcd ( uint16_t bcd ) {
	static char buf[ 6 /* "xx.xx" + NUL */ ];
	uint8_t high = ( bcd >> 8 );
	uint8_t low = ( bcd >> 0 );

	snprintf ( buf, sizeof ( buf ), "%x.%02x", high, low );
	return buf;
}

/******************************************************************************
 *
 * USB descriptors
 *
 ******************************************************************************
 */

/**
 * Locate USB interface association descriptor
 *
 * @v config		Configuraton descriptor
 * @v first		First interface number
 * @ret desc		Interface association descriptor, or NULL if not found
 */
static struct usb_interface_association_descriptor *
usb_interface_association_descriptor ( struct usb_configuration_descriptor
								       *config,
				       unsigned int first ) {
	struct usb_interface_association_descriptor *desc;

	/* Find a matching interface association descriptor */
	for_each_config_descriptor ( desc, config ) {
		if ( ( desc->header.type ==
		       USB_INTERFACE_ASSOCIATION_DESCRIPTOR ) &&
		     ( desc->first == first ) )
			return desc;
	}
	return NULL;
}

/**
 * Locate USB interface descriptor
 *
 * @v config		Configuraton descriptor
 * @v interface		Interface number
 * @v alternate		Alternate setting
 * @ret desc		Interface descriptor, or NULL if not found
 */
struct usb_interface_descriptor *
usb_interface_descriptor ( struct usb_configuration_descriptor *config,
			   unsigned int interface, unsigned int alternate ) {
	struct usb_interface_descriptor *desc;

	/* Find a matching interface descriptor */
	for_each_config_descriptor ( desc, config ) {
		if ( ( desc->header.type == USB_INTERFACE_DESCRIPTOR ) &&
		     ( desc->interface == interface ) &&
		     ( desc->alternate == alternate ) )
			return desc;
	}
	return NULL;
}

/**
 * Locate USB endpoint descriptor
 *
 * @v config		Configuration descriptor
 * @v interface		Interface descriptor
 * @v type		Endpoint (internal) type
 * @v index		Endpoint index
 * @ret desc		Descriptor, or NULL if not found
 */
struct usb_endpoint_descriptor *
usb_endpoint_descriptor ( struct usb_configuration_descriptor *config,
			  struct usb_interface_descriptor *interface,
			  unsigned int type, unsigned int index ) {
	struct usb_endpoint_descriptor *desc;
	unsigned int attributes = ( type & USB_ENDPOINT_ATTR_TYPE_MASK );
	unsigned int direction = ( type & USB_DIR_IN );

	/* Find a matching endpoint descriptor */
	for_each_interface_descriptor ( desc, config, interface ) {
		if ( ( desc->header.type == USB_ENDPOINT_DESCRIPTOR ) &&
		     ( ( desc->attributes &
			 USB_ENDPOINT_ATTR_TYPE_MASK ) == attributes ) &&
		     ( ( desc->endpoint & USB_DIR_IN ) == direction ) &&
		     ( index-- == 0 ) )
			return desc;
	}
	return NULL;
}

/**
 * Locate USB endpoint companion descriptor
 *
 * @v config		Configuration descriptor
 * @v desc		Endpoint descriptor
 * @ret descx		Companion descriptor, or NULL if not found
 */
struct usb_endpoint_companion_descriptor *
usb_endpoint_companion_descriptor ( struct usb_configuration_descriptor *config,
				    struct usb_endpoint_descriptor *desc ) {
	struct usb_endpoint_companion_descriptor *descx;

	/* Get companion descriptor, if present */
	descx = container_of ( usb_next_descriptor ( &desc->header ),
			       struct usb_endpoint_companion_descriptor,
			       header );
	return ( ( usb_is_within_config ( config, &descx->header ) &&
		   descx->header.type == USB_ENDPOINT_COMPANION_DESCRIPTOR )
		 ? descx : NULL );
}

/******************************************************************************
 *
 * USB endpoint
 *
 ******************************************************************************
 */

/**
 * Describe USB endpoint from device configuration
 *
 * @v ep		USB endpoint
 * @v config		Configuration descriptor
 * @v interface		Interface descriptor
 * @v type		Endpoint (internal) type
 * @v index		Endpoint index
 * @ret rc		Return status code
 */
int usb_endpoint_described ( struct usb_endpoint *ep,
			     struct usb_configuration_descriptor *config,
			     struct usb_interface_descriptor *interface,
			     unsigned int type, unsigned int index ) {
	struct usb_endpoint_descriptor *desc;
	struct usb_endpoint_companion_descriptor *descx;
	unsigned int sizes;
	unsigned int burst;
	size_t mtu;

	/* Locate endpoint descriptor */
	desc = usb_endpoint_descriptor ( config, interface, type, index );
	if ( ! desc )
		return -ENOENT;

	/* Locate companion descriptor, if any */
	descx = usb_endpoint_companion_descriptor ( config, desc );

	/* Calculate MTU and burst size */
	sizes = le16_to_cpu ( desc->sizes );
	mtu = USB_ENDPOINT_MTU ( sizes );
	burst = ( descx ? descx->burst : USB_ENDPOINT_BURST ( sizes ) );

	/* Describe endpoint */
	usb_endpoint_describe ( ep, desc->endpoint, desc->attributes,
				mtu, burst );
	return 0;
}

/**
 * Open USB endpoint
 *
 * @v ep		USB endpoint
 * @ret rc		Return status code
 */
int usb_endpoint_open ( struct usb_endpoint *ep ) {
	struct usb_device *usb = ep->usb;
	unsigned int idx = USB_ENDPOINT_IDX ( ep->address );
	int rc;

	/* Populate host controller operations */
	ep->host = &usb->port->hub->bus->op->endpoint;

	/* Add to endpoint list */
	if ( usb->ep[idx] != NULL ) {
		DBGC ( usb, "USB %s %s is already open\n",
		       usb->name, usb_endpoint_name ( ep->address ) );
		rc = -EALREADY;
		goto err_already;
	}
	usb->ep[idx] = ep;

	/* Clear any stale error status */
	ep->rc = 0;

	/* Open endpoint */
	if ( ( rc = ep->host->open ( ep ) ) != 0 ) {
		DBGC ( usb, "USB %s %s could not open: %s\n", usb->name,
		       usb_endpoint_name ( ep->address ), strerror ( rc ) );
		goto err_open;
	}
	ep->open = 1;

	DBGC2 ( usb, "USB %s %s opened with MTU %zd (burst %d)\n", usb->name,
		usb_endpoint_name ( ep->address ), ep->mtu, ep->burst );
	return 0;

	ep->open = 0;
	ep->host->close ( ep );
 err_open:
	usb->ep[idx] = NULL;
 err_already:
	return rc;
}

/**
 * Close USB endpoint
 *
 * @v ep		USB endpoint
 */
void usb_endpoint_close ( struct usb_endpoint *ep ) {
	struct usb_device *usb = ep->usb;
	unsigned int idx = USB_ENDPOINT_IDX ( ep->address );

	/* Sanity checks */
	assert ( usb->ep[idx] == ep );

	/* Close endpoint */
	ep->open = 0;
	ep->host->close ( ep );

	/* Remove from endpoint list */
	usb->ep[idx] = NULL;
}

/**
 * Reset USB endpoint
 *
 * @v ep		USB endpoint
 * @ret rc		Return status code
 */
static int usb_endpoint_reset ( struct usb_endpoint *ep ) {
	struct usb_device *usb = ep->usb;
	unsigned int type;
	int rc;

	/* Reset endpoint */
	if ( ( rc = ep->host->reset ( ep ) ) != 0 ) {
		DBGC ( usb, "USB %s %s could not reset: %s\n",
		       usb->name, usb_endpoint_name ( ep->address ),
		       strerror ( rc ) );
		return rc;
	}

	/* Clear endpoint halt, if applicable */
	type = ( ep->attributes & USB_ENDPOINT_ATTR_TYPE_MASK );
	if ( ( type != USB_ENDPOINT_ATTR_CONTROL ) &&
	     ( ( rc = usb_clear_feature ( usb, USB_RECIP_ENDPOINT,
					  USB_ENDPOINT_HALT,
					  ep->address ) ) != 0 ) ) {
		DBGC ( usb, "USB %s %s could not clear endpoint halt: %s\n",
		       usb->name, usb_endpoint_name ( ep->address ),
		       strerror ( rc ) );
		return rc;
	}

	/* Clear recorded error */
	ep->rc = 0;

	DBGC ( usb, "USB %s %s reset\n",
	       usb->name, usb_endpoint_name ( ep->address ) );
	return 0;
}

/**
 * Update endpoint MTU
 *
 * @v ep		USB endpoint
 * @v mtu		New MTU
 * @ret rc		Return status code
 */
static int usb_endpoint_mtu ( struct usb_endpoint *ep, size_t mtu ) {
	struct usb_device *usb = ep->usb;
	int rc;

	/* Update MTU */
	ep->mtu = mtu;
	if ( ( rc = ep->host->mtu ( ep ) ) != 0 ) {
		DBGC ( usb, "USB %s %s could not update MTU: %s\n",
		       usb->name, usb_endpoint_name ( ep->address ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Enqueue USB message transfer
 *
 * @v ep		USB endpoint
 * @v request		Request
 * @v value		Value parameter
 * @v index		Index parameter
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
int usb_message ( struct usb_endpoint *ep, unsigned int request,
		  unsigned int value, unsigned int index,
		  struct io_buffer *iobuf ) {
	struct usb_device *usb = ep->usb;
	struct usb_port *port = usb->port;
	struct usb_setup_packet packet;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Fail immediately if device has been unplugged */
	if ( port->speed == USB_SPEED_NONE )
		return -ENODEV;

	/* Reset endpoint if required */
	if ( ( ep->rc != 0 ) && ( ( rc = usb_endpoint_reset ( ep ) ) != 0 ) )
		return rc;

	/* Zero input data buffer (if applicable) */
	if ( request & USB_DIR_IN )
		memset ( iobuf->data, 0, len );

	/* Construct setup packet */
	packet.request = cpu_to_le16 ( request );
	packet.value = cpu_to_le16 ( value );
	packet.index = cpu_to_le16 ( index );
	packet.len = cpu_to_le16 ( len );

	/* Enqueue message transfer */
	if ( ( rc = ep->host->message ( ep, &packet, iobuf ) ) != 0 ) {
		DBGC ( usb, "USB %s %s could not enqueue message transfer: "
		       "%s\n", usb->name, usb_endpoint_name ( ep->address ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Enqueue USB stream transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
int usb_stream ( struct usb_endpoint *ep, struct io_buffer *iobuf ) {
	struct usb_device *usb = ep->usb;
	struct usb_port *port = usb->port;
	int rc;

	/* Fail immediately if device has been unplugged */
	if ( port->speed == USB_SPEED_NONE )
		return -ENODEV;

	/* Reset endpoint if required */
	if ( ( ep->rc != 0 ) && ( ( rc = usb_endpoint_reset ( ep ) ) != 0 ) )
		return rc;

	/* Enqueue stream transfer */
	if ( ( rc = ep->host->stream ( ep, iobuf ) ) != 0 ) {
		DBGC ( usb, "USB %s %s could not enqueue stream transfer: %s\n",
		       usb->name, usb_endpoint_name ( ep->address ),
		       strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Complete transfer (possibly with error)
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
void usb_complete_err ( struct usb_endpoint *ep, struct io_buffer *iobuf,
			int rc ) {
	struct usb_device *usb = ep->usb;

	/* Record error (if any) */
	ep->rc = rc;
	if ( ( rc != 0 ) && ep->open ) {
		DBGC ( usb, "USB %s %s completion failed: %s\n",
		       usb->name, usb_endpoint_name ( ep->address ),
		       strerror ( rc ) );
	}

	/* Report completion */
	ep->driver->complete ( ep, iobuf, rc );
}

/******************************************************************************
 *
 * Control endpoint
 *
 ******************************************************************************
 */

/**
 * Complete USB control transfer
 *
 * @v ep		USB endpoint
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void usb_control_complete ( struct usb_endpoint *ep,
				   struct io_buffer *iobuf, int rc ) {
	struct usb_device *usb = ep->usb;

	/* Check for failures */
	if ( rc != 0 ) {
		DBGC ( usb, "USB %s control transaction failed: %s\n",
		       usb->name, strerror ( rc ) );
		free_iob ( iobuf );
		return;
	}

	/* Add to list of completed I/O buffers */
	list_add_tail ( &iobuf->list, &usb->complete );
}

/** USB control endpoint driver operations */
static struct usb_endpoint_driver_operations usb_control_operations = {
	.complete = usb_control_complete,
};

/**
 * Issue USB control transaction
 *
 * @v usb		USB device
 * @v request		Request
 * @v value		Value parameter
 * @v index		Index parameter
 * @v data		Data buffer (if any)
 * @v len		Length of data
 * @ret rc		Return status code
 */
int usb_control ( struct usb_device *usb, unsigned int request,
		  unsigned int value, unsigned int index, void *data,
		  size_t len ) {
	struct usb_bus *bus = usb->port->hub->bus;
	struct usb_endpoint *ep = &usb->control;
	struct io_buffer *iobuf;
	struct io_buffer *cmplt;
	unsigned int i;
	int rc;

	/* Allocate I/O buffer */
	iobuf = alloc_iob ( len );
	if ( ! iobuf ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	iob_put ( iobuf, len );
	if ( request & USB_DIR_IN ) {
		memset ( data, 0, len );
	} else {
		memcpy ( iobuf->data, data, len );
	}

	/* Enqueue message */
	if ( ( rc = usb_message ( ep, request, value, index, iobuf ) ) != 0 )
		goto err_message;

	/* Wait for completion */
	for ( i = 0 ; i < USB_CONTROL_MAX_WAIT_MS ; i++ ) {

		/* Poll bus */
		usb_poll ( bus );

		/* Check for completion */
		while ( ( cmplt = list_first_entry ( &usb->complete,
						     struct io_buffer,
						     list ) ) ) {

			/* Remove from completion list */
			list_del ( &cmplt->list );

			/* Discard stale completions */
			if ( cmplt != iobuf ) {
				DBGC ( usb, "USB %s stale control "
				       "completion:\n", usb->name );
				DBGC_HDA ( usb, 0, cmplt->data,
					   iob_len ( cmplt ) );
				free_iob ( cmplt );
				continue;
			}

			/* Copy completion to data buffer, if applicable */
			assert ( iob_len ( cmplt ) <= len );
			if ( request & USB_DIR_IN )
				memcpy ( data, cmplt->data, iob_len ( cmplt ) );
			free_iob ( cmplt );
			return 0;
		}

		/* Fail immediately if endpoint is in an error state */
		if ( ep->rc )
			return ep->rc;

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( usb, "USB %s timed out waiting for control transaction\n",
	       usb->name );
	return -ETIMEDOUT;

 err_message:
	free_iob ( iobuf );
 err_alloc:
	return rc;
}

/**
 * Get USB string descriptor
 *
 * @v usb		USB device
 * @v index		String index
 * @v language		Language ID
 * @v buf		Data buffer
 * @v len		Length of buffer
 * @ret len		String length (excluding NUL), or negative error
 */
int usb_get_string_descriptor ( struct usb_device *usb, unsigned int index,
				unsigned int language, char *buf, size_t len ) {
	size_t max = ( len ? ( len - 1 /* NUL */ ) : 0 );
	struct {
		struct usb_descriptor_header header;
		uint16_t character[max];
	} __attribute__ (( packed )) *desc;
	unsigned int actual;
	unsigned int i;
	int rc;

	/* Allocate buffer for string */
	desc = malloc ( sizeof ( *desc ) );
	if ( ! desc ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Get descriptor */
	if ( ( rc = usb_get_descriptor ( usb, 0, USB_STRING_DESCRIPTOR, index,
					 language, &desc->header,
					 sizeof ( *desc ) ) ) != 0 )
		goto err_get_descriptor;

	/* Copy to buffer */
	actual = ( ( desc->header.len - sizeof ( desc->header ) ) /
		   sizeof ( desc->character[0] ) );
	for ( i = 0 ; ( ( i < actual ) && ( i < max ) ) ; i++ )
		buf[i] = le16_to_cpu ( desc->character[i] );
	if ( len )
		buf[i] = '\0';

	/* Free buffer */
	free ( desc );

	return actual;

 err_get_descriptor:
	free ( desc );
 err_alloc:
	return rc;
}

/******************************************************************************
 *
 * USB device driver
 *
 ******************************************************************************
 */

/**
 * Describe USB function
 *
 * @v func		USB function
 * @v config		Configuration descriptor
 * @v first		First interface number
 * @ret rc		Return status code
 */
static int usb_function ( struct usb_function *func,
			  struct usb_configuration_descriptor *config,
			  unsigned int first ) {
	struct usb_device *usb = func->usb;
	struct usb_interface_association_descriptor *association;
	struct usb_interface_descriptor *interface;
	unsigned int i;

	/* First, look for an interface association descriptor */
	association = usb_interface_association_descriptor ( config, first );
	if ( association ) {

		/* Sanity check */
		if ( ( association->first + association->count ) >
		     config->interfaces ) {
			DBGC ( usb, "USB %s has invalid association [%d-%d)\n",
			       func->name, association->first,
			       ( association->first + association->count ) );
			return -ERANGE;
		}

		/* Describe function */
		memcpy ( &func->class, &association->class,
			 sizeof ( func->class ) );
		func->count = association->count;
		for ( i = 0 ; i < association->count ; i++ )
			func->interface[i] = ( association->first + i );
		return 0;
	}

	/* Next, look for an interface descriptor */
	interface = usb_interface_descriptor ( config, first, 0 );
	if ( ! interface ) {
		DBGC ( usb, "USB %s has no interface descriptor\n",
		       func->name );
		return -ENOENT;
	}

	/* Describe function */
	memcpy ( &func->class, &interface->class, sizeof ( func->class ) );
	func->count = 1;
	func->interface[0] = first;
	return 0;
}

/**
 * Check for a USB device ID match
 *
 * @v func		USB function
 * @v id		Device ID
 * @ret matches		Device ID matches
 */
static int
usb_device_id_matches ( struct usb_function *func, struct usb_device_id *id ) {

	return ( ( ( id->vendor == func->dev.desc.vendor ) ||
		   ( id->vendor == USB_ANY_ID ) ) &&
		 ( ( id->product == func->dev.desc.device ) ||
		   ( id->product == USB_ANY_ID ) ) &&
		 ( id->class.class == func->class.class ) &&
		 ( id->class.subclass == func->class.subclass ) &&
		 ( id->class.protocol == func->class.protocol ) );
}

/**
 * Probe USB device driver
 *
 * @v func		USB function
 * @v config		Configuration descriptor
 * @ret rc		Return status code
 */
static int usb_probe ( struct usb_function *func,
		       struct usb_configuration_descriptor *config ) {
	struct usb_device *usb = func->usb;
	struct usb_driver *driver;
	struct usb_device_id *id;
	unsigned int i;
	int rc;

	/* Look for a matching driver */
	for_each_table_entry ( driver, USB_DRIVERS ) {
		for ( i = 0 ; i < driver->id_count ; i++ ) {

			/* Check for a matching ID */
			id = &driver->ids[i];
			if ( ! usb_device_id_matches ( func, id ) )
				continue;

			/* Probe driver */
			if ( ( rc = driver->probe ( func, config ) ) != 0 ) {
				DBGC ( usb, "USB %s failed to probe driver %s: "
				       "%s\n", func->name, id->name,
				       strerror ( rc ) );
				/* Continue trying other drivers */
				continue;
			}

			/* Record driver */
			func->driver = driver;
			func->dev.driver_name = id->name;
			return 0;
		}
	}

	/* No driver found */
	DBGC ( usb, "USB %s %04x:%04x class %d:%d:%d has no driver\n",
	       func->name, func->dev.desc.vendor, func->dev.desc.device,
	       func->class.class, func->class.subclass, func->class.protocol );
	return -ENOENT;
}

/**
 * Remove USB device driver
 *
 * @v func		USB function
 */
static void usb_remove ( struct usb_function *func ) {

	/* Remove driver */
	func->driver->remove ( func );
}

/**
 * Probe all USB device drivers
 *
 * @v usb		USB device
 * @v config		Configuration descriptor
 */
static void
usb_probe_all ( struct usb_device *usb,
		struct usb_configuration_descriptor *config ) {
	struct usb_bus *bus = usb->port->hub->bus;
	struct usb_function *func;
	uint8_t used[config->interfaces];
	unsigned int first;
	unsigned int i;
	int rc;

	/* Identify each function in turn */
	memset ( used, 0, sizeof ( used ) );
	for ( first = 0 ; first < config->interfaces ; first++ ) {

		/* Skip interfaces already used */
		if ( used[first] )
			continue;

		/* Allocate and initialise structure */
		func = zalloc ( sizeof ( *func ) +
				( config->interfaces *
				  sizeof ( func->interface[0] ) ) );
		if ( ! func )
			goto err_alloc;
		func->name = func->dev.name;
		func->usb = usb;
		func->dev.desc.bus_type = BUS_TYPE_USB;
		func->dev.desc.location = usb->address;
		func->dev.desc.vendor = le16_to_cpu ( usb->device.vendor );
		func->dev.desc.device = le16_to_cpu ( usb->device.product );
		snprintf ( func->dev.name, sizeof ( func->dev.name ),
			   "%s-%d", usb->name, first );
		INIT_LIST_HEAD ( &func->dev.children );
		func->dev.parent = bus->dev;

		/* Identify function */
		if ( ( rc = usb_function ( func, config, first ) ) != 0 )
			goto err_function;
		assert ( func->count <= config->interfaces );

		/* Mark interfaces as used */
		for ( i = 0 ; i < func->count ; i++ ) {
			assert ( func->interface[i] < config->interfaces );
			used[ func->interface[i] ] = 1;
		}

		/* Probe device driver */
		if ( ( rc = usb_probe ( func, config ) ) != 0 )
			goto err_probe;
		DBGC ( usb, "USB %s %04x:%04x class %d:%d:%d interfaces ",
		       func->name, func->dev.desc.vendor, func->dev.desc.device,
		       func->class.class, func->class.subclass,
		       func->class.protocol );
		for ( i = 0 ; i < func->count ; i++ )
			DBGC ( usb, "%s%d", ( i ? "," : "" ),
			       func->interface[i] );
		DBGC ( usb, " using driver %s\n", func->dev.driver_name );

		/* Add to list of functions */
		list_add ( &func->list, &usb->functions );

		/* Add to device hierarchy */
		list_add_tail ( &func->dev.siblings, &bus->dev->children );

		continue;

		list_del ( &func->dev.siblings );
		list_del ( &func->list );
		usb_remove ( func );
	err_probe:
		free ( func );
	err_alloc:
	err_function:
		/* Continue registering other functions */
		continue;
	}
}

/**
 * Remove all device drivers
 *
 * @v usb		USB device
 */
static void usb_remove_all ( struct usb_device *usb ) {
	struct usb_function *func;
	struct usb_function *tmp;

	/* Remove all functions */
	list_for_each_entry_safe ( func, tmp, &usb->functions, list ) {

		/* Remove device driver */
		usb_remove ( func );

		/* Remove from device hierarchy */
		assert ( list_empty ( &func->dev.children ) );
		list_del ( &func->dev.siblings );

		/* Remove from list of functions */
		list_del ( &func->list );

		/* Free function */
		free ( func );
	}
}

/******************************************************************************
 *
 * USB device
 *
 ******************************************************************************
 */

/**
 * Allocate USB device
 *
 * @v port		USB port
 * @ret usb		USB device, or NULL on allocation failure
 */
static struct usb_device * alloc_usb ( struct usb_port *port ) {
	struct usb_hub *hub = port->hub;
	struct usb_bus *bus = hub->bus;
	struct usb_device *usb;

	/* Allocate and initialise structure */
	usb = zalloc ( sizeof ( *usb ) );
	if ( ! usb )
		return NULL;
	snprintf ( usb->name, sizeof ( usb->name ), "%s%c%d", hub->name,
		   ( hub->usb ? '.' : '-' ), port->address );
	usb->port = port;
	INIT_LIST_HEAD ( &usb->functions );
	usb->host = &bus->op->device;
	usb_endpoint_init ( &usb->control, usb, &usb_control_operations );
	INIT_LIST_HEAD ( &usb->complete );

	return usb;
}

/**
 * Register USB device
 *
 * @v usb		USB device
 * @ret rc		Return status code
 */
static int register_usb ( struct usb_device *usb ) {
	struct usb_port *port = usb->port;
	struct usb_hub *hub = port->hub;
	struct usb_bus *bus = hub->bus;
	struct usb_configuration_descriptor partial;
	struct usb_configuration_descriptor *config;
	unsigned int protocol;
	size_t mtu;
	size_t len;
	int rc;

	/* Add to port */
	if ( port->usb != NULL ) {
		DBGC ( hub, "USB hub %s port %d is already registered to %s\n",
		       hub->name, port->address, port->usb->name );
		rc = -EALREADY;
		goto err_already;
	}
	port->usb = usb;

	/* Add to bus device list */
	list_add_tail ( &usb->list, &bus->devices );

	/* Enable device */
	if ( ( rc = hub->driver->enable ( hub, port ) ) != 0 ) {
		DBGC ( hub, "USB hub %s port %d could not enable: %s\n",
		       hub->name, port->address, strerror ( rc ) );
		goto err_enable;
	}

	/* Get device speed */
	if ( ( rc = hub->driver->speed ( hub, port ) ) != 0 ) {
		DBGC ( hub, "USB hub %s port %d could not get speed: %s\n",
		       hub->name, port->address, strerror ( rc ) );
		goto err_speed;
	}
	DBGC2 ( usb, "USB %s attached as %s-speed device\n",
		usb->name, usb_speed_name ( port->speed ) );

	/* Open device */
	if ( ( rc = usb->host->open ( usb ) ) != 0 ) {
		DBGC ( usb, "USB %s could not open: %s\n",
		       usb->name, strerror ( rc ) );
		goto err_open;
	}

	/* Describe control endpoint */
	mtu = USB_EP0_DEFAULT_MTU ( port->speed );
	usb_endpoint_describe ( &usb->control, USB_EP0_ADDRESS,
				USB_EP0_ATTRIBUTES, mtu, USB_EP0_BURST );

	/* Open control endpoint */
	if ( ( rc = usb_endpoint_open ( &usb->control ) ) != 0 )
		goto err_open_control;
	assert ( usb_endpoint ( usb, USB_EP0_ADDRESS ) == &usb->control );

	/* Assign device address */
	if ( ( rc = usb->host->address ( usb ) ) != 0 ) {
		DBGC ( usb, "USB %s could not set address: %s\n",
		       usb->name, strerror ( rc ) );
		goto err_address;
	}
	DBGC2 ( usb, "USB %s assigned address %d\n", usb->name, usb->address );

	/* Read first part of device descriptor to get EP0 MTU */
	if ( ( rc = usb_get_mtu ( usb, &usb->device ) ) != 0 ) {
		DBGC ( usb, "USB %s could not get MTU: %s\n",
		       usb->name, strerror ( rc ) );
		goto err_get_mtu;
	}

	/* Calculate EP0 MTU */
	protocol = le16_to_cpu ( usb->device.protocol );
	mtu = ( ( protocol < USB_PROTO_3_0 ) ?
		usb->device.mtu : ( 1 << usb->device.mtu ) );
	DBGC2 ( usb, "USB %s has control MTU %zd (guessed %zd)\n",
		usb->name, mtu, usb->control.mtu );

	/* Update MTU */
	if ( ( rc = usb_endpoint_mtu ( &usb->control, mtu ) ) != 0 )
		goto err_mtu;

	/* Read whole device descriptor */
	if ( ( rc = usb_get_device_descriptor ( usb, &usb->device ) ) != 0 ) {
		DBGC ( usb, "USB %s could not get device descriptor: %s\n",
		       usb->name, strerror ( rc ) );
		goto err_get_device_descriptor;
	}
	DBGC ( usb, "USB %s addr %d %04x:%04x class %d:%d:%d (v%s, %s-speed, "
	       "MTU %zd)\n", usb->name, usb->address,
	       le16_to_cpu ( usb->device.vendor ),
	       le16_to_cpu ( usb->device.product ), usb->device.class.class,
	       usb->device.class.subclass, usb->device.class.protocol,
	       usb_bcd ( le16_to_cpu ( usb->device.protocol ) ),
	       usb_speed_name ( port->speed ), usb->control.mtu );

	/* Read first part of configuration descriptor to get size */
	if ( ( rc = usb_get_config_descriptor ( usb, 0, &partial,
						sizeof ( partial ) ) ) != 0 ) {
		DBGC ( usb, "USB %s could not get configuration descriptor: "
		       "%s\n", usb->name, strerror ( rc ) );
		goto err_get_partial;
	}
	len = le16_to_cpu ( partial.len );
	if ( len < sizeof ( partial ) ) {
		DBGC ( usb, "USB %s underlength configuraton descriptor\n",
		       usb->name );
		rc = -EINVAL;
		goto err_partial_len;
	}

	/* Allocate buffer for whole configuration descriptor */
	config = malloc ( len );
	if ( ! config ) {
		rc = -ENOMEM;
		goto err_alloc_config;
	}

	/* Read whole configuration descriptor */
	if ( ( rc = usb_get_config_descriptor ( usb, 0, config, len ) ) != 0 ) {
		DBGC ( usb, "USB %s could not get configuration descriptor: "
		       "%s\n", usb->name, strerror ( rc ) );
		goto err_get_config_descriptor;
	}
	if ( config->len != partial.len ) {
		DBGC ( usb, "USB %s bad configuration descriptor length\n",
		       usb->name );
		rc = -EINVAL;
		goto err_config_len;
	}

	/* Set configuration */
	if ( ( rc = usb_set_configuration ( usb, config->config ) ) != 0){
		DBGC ( usb, "USB %s could not set configuration %#02x: %s\n",
		       usb->name, config->config, strerror ( rc ) );
		goto err_set_configuration;
	}

	/* Probe USB device drivers */
	usb_probe_all ( usb, config );

	/* Free configuration descriptor */
	free ( config );

	return 0;

	usb_remove_all ( usb );
	usb_set_configuration ( usb, 0 );
 err_set_configuration:
 err_config_len:
 err_get_config_descriptor:
	free ( config );
 err_alloc_config:
 err_partial_len:
 err_get_partial:
 err_get_device_descriptor:
 err_mtu:
 err_get_mtu:
 err_address:
	usb_endpoint_close ( &usb->control );
 err_open_control:
	usb->host->close ( usb );
 err_open:
 err_speed:
	hub->driver->disable ( hub, port );
 err_enable:
	list_del ( &usb->list );
	port->usb = NULL;
 err_already:
	return rc;
}

/**
 * Unregister USB device
 *
 * @v usb		USB device
 */
static void unregister_usb ( struct usb_device *usb ) {
	struct usb_port *port = usb->port;
	struct usb_hub *hub = port->hub;
	struct io_buffer *iobuf;
	struct io_buffer *tmp;
	unsigned int i;

	/* Remove device drivers */
	usb_remove_all ( usb );

	/* Sanity checks */
	for ( i = 0 ; i < ( sizeof ( usb->ep ) / sizeof ( usb->ep[0] ) ) ; i++){
		if ( i != USB_ENDPOINT_IDX ( USB_EP0_ADDRESS ) )
			assert ( usb->ep[i] == NULL );
	}
	assert ( port->usb == usb );

	/* Clear device configuration */
	usb_set_configuration ( usb, 0 );

	/* Close control endpoint */
	usb_endpoint_close ( &usb->control );

	/* Discard any stale control completions */
	list_for_each_entry_safe ( iobuf, tmp, &usb->complete, list ) {
		list_del ( &iobuf->list );
		free_iob ( iobuf );
	}

	/* Close device */
	usb->host->close ( usb );

	/* Disable port */
	hub->driver->disable ( hub, port );

	/* Remove from bus device list */
	list_del ( &usb->list );

	/* Remove from port */
	port->usb = NULL;
}

/**
 * Free USB device
 *
 * @v usb		USB device
 */
static void free_usb ( struct usb_device *usb ) {
	unsigned int i;

	/* Sanity checks */
	for ( i = 0 ; i < ( sizeof ( usb->ep ) / sizeof ( usb->ep[0] ) ) ; i++ )
		assert ( usb->ep[i] == NULL );
	assert ( list_empty ( &usb->functions ) );
	assert ( list_empty ( &usb->complete ) );

	/* Free device */
	free ( usb );
}

/******************************************************************************
 *
 * USB device hotplug event handling
 *
 ******************************************************************************
 */

/**
 * Handle newly attached USB device
 *
 * @v port		USB port
 * @ret rc		Return status code
 */
static int usb_attached ( struct usb_port *port ) {
	struct usb_device *usb;
	int rc;

	/* Sanity checks */
	assert ( port->usb == NULL );

	/* Allocate USB device */
	usb = alloc_usb ( port );
	if ( ! usb ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Register USB device */
	if ( ( rc = register_usb ( usb ) ) != 0 )
		goto err_register;

	return 0;

	unregister_usb ( usb );
 err_register:
	free_usb ( usb );
 err_alloc:
	return rc;
}

/**
 * Handle newly detached USB device
 *
 * @v port		USB port
 */
static void usb_detached ( struct usb_port *port ) {
	struct usb_device *usb = port->usb;

	/* Sanity checks */
	assert ( port->usb != NULL );

	/* Unregister USB device */
	unregister_usb ( usb );

	/* Free USB device */
	free_usb ( usb );
}

/**
 * Handle newly attached or detached USB devices
 *
 * @v port		USB port
 * @ret rc		Return status code
 */
static int usb_hotplug ( struct usb_port *port ) {
	struct usb_hub *hub = port->hub;
	int rc;

	/* Get current port speed */
	if ( ( rc = hub->driver->speed ( hub, port ) ) != 0 ) {
		DBGC ( hub, "USB hub %s port %d could not get speed: %s\n",
		       hub->name, port->address, strerror ( rc ) );
		return rc;
	}

	/* Handle attached/detached device as applicable */
	if ( port->speed && ! port->usb ) {
		/* Newly attached device */
		return usb_attached ( port );
	} else if ( port->usb && ! port->speed ) {
		/* Newly detached device */
		usb_detached ( port );
		return 0;
	} else {
		/* Ignore */
		return 0;
	}
}

/******************************************************************************
 *
 * USB process
 *
 ******************************************************************************
 */

/**
 * Report port status change
 *
 * @v port		USB port
 */
void usb_port_changed ( struct usb_port *port ) {
	struct usb_hub *hub = port->hub;
	struct usb_bus *bus = hub->bus;

	/* Record hub port status change */
	list_del ( &port->list );
	list_add_tail ( &port->list, &bus->changed );
}

/**
 * USB process
 *
 * @v bus		USB bus
 */
static void usb_step ( struct usb_bus *bus ) {
	struct usb_port *port;

	/* Poll bus */
	usb_poll ( bus );

	/* Handle any changed ports, allowing for the fact that the
	 * port list may change as we perform hotplug actions.
	 */
	while ( ! list_empty ( &bus->changed ) ) {

		/* Get first changed port */
		port = list_first_entry ( &bus->changed, struct usb_port, list);
		assert ( port != NULL );

		/* Remove from list of changed ports */
		list_del ( &port->list );
		INIT_LIST_HEAD ( &port->list );

		/* Perform appropriate hotplug action */
		usb_hotplug ( port );
	}
}

/** USB process */
static struct process_descriptor usb_process_desc =
	PROC_DESC ( struct usb_bus, process, usb_step );

/******************************************************************************
 *
 * USB hub
 *
 ******************************************************************************
 */

/**
 * Allocate USB hub
 *
 * @v bus		USB bus
 * @v usb		Underlying USB device, if any
 * @v ports		Number of ports
 * @v driver		Hub driver operations
 * @ret hub		USB hub, or NULL on allocation failure
 */
struct usb_hub * alloc_usb_hub ( struct usb_bus *bus, struct usb_device *usb,
				 unsigned int ports,
				 struct usb_hub_driver_operations *driver ) {
	struct usb_hub *hub;
	struct usb_port *port;
	unsigned int i;

	/* Allocate and initialise structure */
	hub = zalloc ( sizeof ( *hub ) + ( ports * sizeof ( hub->port[0] ) ) );
	if ( ! hub )
		return NULL;
	hub->name = ( usb ? usb->name : bus->name );
	hub->bus = bus;
	hub->usb = usb;
	if ( usb )
		hub->protocol = usb->port->protocol;
	hub->ports = ports;
	hub->driver = driver;

	/* Initialise port list */
	for ( i = 1 ; i <= hub->ports ; i++ ) {
		port = usb_port ( hub, i );
		port->hub = hub;
		port->address = i;
		if ( usb )
			port->protocol = usb->port->protocol;
		INIT_LIST_HEAD ( &port->list );
	}

	return hub;
}

/**
 * Register USB hub
 *
 * @v hub		USB hub
 * @ret rc		Return status code
 */
int register_usb_hub ( struct usb_hub *hub ) {
	struct usb_bus *bus = hub->bus;
	struct usb_port *port;
	unsigned int i;
	int rc;

	/* Add to hub list */
	list_add_tail ( &hub->list, &bus->hubs );

	/* Open hub */
	if ( ( rc = hub->driver->open ( hub ) ) != 0 ) {
		DBGC ( hub, "USB hub %s could not open: %s\n",
		       hub->name, strerror ( rc ) );
		goto err_open;
	}

	/* Delay to allow ports to stabilise */
	mdelay ( USB_PORT_DELAY_MS );

	/* Attach any devices already present */
	for ( i = 1 ; i <= hub->ports ; i++ ) {
		port = usb_port ( hub, i );
		usb_hotplug ( port );
	}

	/* Some hubs seem to defer reporting device connections until
	 * their interrupt endpoint is polled for the first time.
	 * Poll the bus once now in order to pick up any such
	 * connections.
	 */
	usb_step ( bus );

	return 0;

	hub->driver->close ( hub );
 err_open:
	list_del ( &hub->list );
	return rc;
}

/**
 * Unregister USB hub
 *
 * @v hub		USB hub
 */
void unregister_usb_hub ( struct usb_hub *hub ) {
	struct usb_port *port;
	unsigned int i;

	/* Detach all devices */
	for ( i = 1 ; i <= hub->ports ; i++ ) {
		port = usb_port ( hub, i );
		if ( port->usb )
			usb_detached ( port );
	}

	/* Close hub */
	hub->driver->close ( hub );

	/* Cancel any pending port status changes */
	for ( i = 1 ; i <= hub->ports ; i++ ) {
		port = usb_port ( hub, i );
		list_del ( &port->list );
		INIT_LIST_HEAD ( &port->list );
	}

	/* Remove from hub list */
	list_del ( &hub->list );
}

/**
 * Free USB hub
 *
 * @v hub		USB hub
 */
void free_usb_hub ( struct usb_hub *hub ) {
	struct usb_port *port;
	unsigned int i;

	/* Sanity checks */
	for ( i = 1 ; i <= hub->ports ; i++ ) {
		port = usb_port ( hub, i );
		assert ( port->usb == NULL );
		assert ( list_empty ( &port->list ) );
	}

	/* Free hub */
	free ( hub );
}

/******************************************************************************
 *
 * USB bus
 *
 ******************************************************************************
 */

/**
 * Allocate USB bus
 *
 * @v dev		Underlying hardware device
 * @v ports		Number of root hub ports
 * @v op		Host controller operations
 * @ret bus		USB bus, or NULL on allocation failure
 */
struct usb_bus * alloc_usb_bus ( struct device *dev, unsigned int ports,
				 struct usb_host_operations *op ) {
	struct usb_bus *bus;

	/* Allocate and initialise structure */
	bus = zalloc ( sizeof ( *bus ) );
	if ( ! bus )
		goto err_alloc_bus;
	bus->name = dev->name;
	bus->dev = dev;
	bus->op = op;
	INIT_LIST_HEAD ( &bus->devices );
	INIT_LIST_HEAD ( &bus->hubs );
	INIT_LIST_HEAD ( &bus->changed );
	process_init_stopped ( &bus->process, &usb_process_desc, NULL );
	bus->host = &bus->op->bus;

	/* Allocate root hub */
	bus->hub = alloc_usb_hub ( bus, NULL, ports, &op->hub );
	if ( ! bus->hub )
		goto err_alloc_hub;

	return bus;

	free_usb_hub ( bus->hub );
 err_alloc_hub:
	free ( bus );
 err_alloc_bus:
	return NULL;
}

/**
 * Register USB bus
 *
 * @v bus		USB bus
 * @ret rc		Return status code
 */
int register_usb_bus ( struct usb_bus *bus ) {
	int rc;

	/* Sanity checks */
	assert ( bus->hub != NULL );

	/* Open bus */
	if ( ( rc = bus->host->open ( bus ) ) != 0 )
		goto err_open;

	/* Register root hub */
	if ( ( rc = register_usb_hub ( bus->hub ) ) != 0 )
		goto err_register_hub;

	/* Start bus process */
	process_add ( &bus->process );

	return 0;

	unregister_usb_hub ( bus->hub );
 err_register_hub:
	bus->host->close ( bus );
 err_open:
	return rc;
}

/**
 * Unregister USB bus
 *
 * @v bus		USB bus
 */
void unregister_usb_bus ( struct usb_bus *bus ) {

	/* Sanity checks */
	assert ( bus->hub != NULL );
	assert ( process_running ( &bus->process ) );

	/* Stop bus process */
	process_del ( &bus->process );

	/* Unregister root hub */
	unregister_usb_hub ( bus->hub );

	/* Close bus */
	bus->host->close ( bus );

	/* Sanity checks */
	assert ( list_empty ( &bus->devices ) );
	assert ( list_empty ( &bus->hubs ) );
	assert ( ! process_running ( &bus->process ) );
}

/**
 * Free USB bus
 *
 * @v bus		USB bus
 */
void free_usb_bus ( struct usb_bus *bus ) {

	/* Sanity checks */
	assert ( list_empty ( &bus->devices ) );
	assert ( list_empty ( &bus->hubs ) );
	assert ( ! process_running ( &bus->process ) );

	/* Free root hub */
	free_usb_hub ( bus->hub );

	/* Free bus */
	free ( bus );
}

/******************************************************************************
 *
 * USB bus topology
 *
 ******************************************************************************
 */

/**
 * Get USB route string
 *
 * @v usb		USB device
 * @ret route		USB route string
 */
unsigned int usb_route_string ( struct usb_device *usb ) {
	unsigned int route;

	/* Navigate up to root hub, constructing route string as we go */
	for ( route = 0 ; usb->port->hub->usb ; usb = usb->port->hub->usb ) {
		route <<= 4;
		route |= ( ( usb->port->address > 0xf ) ?
			   0xf : usb->port->address );
	}
	return route;
}

/**
 * Get USB depth
 *
 * @v usb		USB device
 * @ret depth		Hub depth
 */
unsigned int usb_depth ( struct usb_device *usb ) {
	unsigned int depth;

	/* Navigate up to root hub, constructing depth as we go */
	for ( depth = 0 ; usb->port->hub->usb ; usb = usb->port->hub->usb )
		depth++;

	return depth;
}

/**
 * Get USB root hub port
 *
 * @v usb		USB device
 * @ret port		Root hub port
 */
struct usb_port * usb_root_hub_port ( struct usb_device *usb ) {

	/* Navigate up to root hub */
	while ( usb->port->hub->usb )
		usb = usb->port->hub->usb;

	return usb->port;
}

/* Drag in hub driver */
REQUIRE_OBJECT ( usbhub );
