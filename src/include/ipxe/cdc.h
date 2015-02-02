#ifndef _IPXE_CDC_H
#define _IPXE_CDC_H

/** @file
 *
 * USB Communications Device Class (CDC)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/usb.h>

/** Class code for communications devices */
#define USB_CLASS_CDC 2

/** Ethernet descriptor subtype */
#define CDC_SUBTYPE_ETHERNET 15

/** Network connection notification */
#define CDC_NETWORK_CONNECTION						\
	( USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE |		\
	  USB_REQUEST_TYPE ( 0x00 ) )

/** Connection speed change notification */
#define CDC_CONNECTION_SPEED_CHANGE					\
	( USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE |		\
	  USB_REQUEST_TYPE ( 0x2a ) )

/** Connection speed change notification */
struct cdc_connection_speed_change {
	/** Downlink bit rate, in bits per second */
	uint32_t down;
	/** Uplink bit rate, in bits per second */
	uint32_t up;
} __attribute__ (( packed ));

#endif /* _IPXE_CDC_H */
