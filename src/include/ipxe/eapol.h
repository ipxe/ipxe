#ifndef _IPXE_EAPOL_H
#define _IPXE_EAPOL_H

/** @file
 *
 * Extensible Authentication Protocol over LAN (EAPoL)
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/netdevice.h>
#include <ipxe/tables.h>

/** EAPoL header */
struct eapol_header {
	/** Version */
	uint8_t version;
	/** Type */
	uint8_t type;
	/** Payload length */
	uint16_t len;
} __attribute__ (( packed ));

/** 802.1X-2001 */
#define EAPOL_VERSION_2001 1

/** EAPoL-encapsulated EAP packets */
#define EAPOL_TYPE_EAP 0

/** EAPoL key */
#define EAPOL_TYPE_KEY 5

/** An EAPoL handler */
struct eapol_handler {
	/** Type */
	uint8_t type;
	/**
	 * Process received packet
	 *
	 * @v iobuf		I/O buffer
	 * @v netdev		Network device
	 * @v ll_source		Link-layer source address
	 * @ret rc		Return status code
	 *
	 * This method takes ownership of the I/O buffer.
	 */
	int ( * rx ) ( struct io_buffer *iobuf, struct net_device *netdev,
		       const void *ll_source );
};

/** EAPoL handler table */
#define EAPOL_HANDLERS __table ( struct eapol_handler, "eapol_handlers" )

/** Declare an EAPoL handler */
#define __eapol_handler __table_entry ( EAPOL_HANDLERS, 01 )

extern struct net_protocol eapol_protocol __net_protocol;

#endif /* _IPXE_EAPOL_H */
