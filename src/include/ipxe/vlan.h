#ifndef _IPXE_VLAN_H
#define _IPXE_VLAN_H

/**
 * @file
 *
 * Virtual LANs
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/netdevice.h>

/** A VLAN header */
struct vlan_header {
	/** Tag control information */
	uint16_t tci;
	/** Encapsulated protocol */
	uint16_t net_proto;
} __attribute__ (( packed ));

/**
 * Extract VLAN tag from tag control information
 *
 * @v tci		Tag control information
 * @ret tag		VLAN tag
 */
#define VLAN_TAG( tci ) ( (tci) & 0x0fff )

/**
 * Extract VLAN priority from tag control information
 *
 * @v tci		Tag control information
 * @ret priority	Priority
 */
#define VLAN_PRIORITY( tci ) ( (tci) >> 13 )

/**
 * Construct VLAN tag control information
 *
 * @v tag		VLAN tag
 * @v priority		Priority
 * @ret tci		Tag control information
 */
#define VLAN_TCI( tag, priority ) ( ( (priority) << 13 ) | (tag) )

/**
 * Check VLAN tag is valid
 *
 * @v tag		VLAN tag
 * @ret is_valid	VLAN tag is valid
 */
#define VLAN_TAG_IS_VALID( tag ) ( (tag) < 0xfff )

/**
 * Check VLAN priority is valid
 *
 * @v priority		VLAN priority
 * @ret is_valid	VLAN priority is valid
 */
#define VLAN_PRIORITY_IS_VALID( priority ) ( (priority) <= 7 )

extern unsigned int vlan_tag ( struct net_device *netdev );
extern int vlan_can_be_trunk ( struct net_device *trunk );
extern int vlan_create ( struct net_device *trunk, unsigned int tag,
			 unsigned int priority );
extern int vlan_destroy ( struct net_device *netdev );
extern void vlan_netdev_rx ( struct net_device *netdev, unsigned int tag,
			     struct io_buffer *iobuf );
extern void vlan_netdev_rx_err ( struct net_device *netdev, unsigned int tag,
				 struct io_buffer *iobuf, int rc );

#endif /* _IPXE_VLAN_H */
