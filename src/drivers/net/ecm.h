#ifndef _ECM_H
#define _ECM_H

/** @file
 *
 * CDC-ECM USB Ethernet driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/usb.h>
#include <ipxe/cdc.h>

/** An Ethernet Functional Descriptor */
struct ecm_ethernet_descriptor {
	/** Descriptor header */
	struct usb_descriptor_header header;
	/** Descriptor subtype */
	uint8_t subtype;
	/** MAC addres string */
	uint8_t mac;
	/** Ethernet statistics bitmap */
	uint32_t statistics;
	/** Maximum segment size */
	uint16_t mtu;
	/** Multicast filter configuration */
	uint16_t mcast;
	/** Number of wake-on-LAN filters */
	uint8_t wol;
} __attribute__ (( packed ));

extern struct ecm_ethernet_descriptor *
ecm_ethernet_descriptor ( struct usb_configuration_descriptor *config,
			  struct usb_interface_descriptor *interface );
extern int ecm_fetch_mac ( struct usb_device *usb,
			   struct ecm_ethernet_descriptor *desc,
			   uint8_t *hw_addr );

#endif /* _ECM_H */
