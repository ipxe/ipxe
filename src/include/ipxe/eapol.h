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
#include <ipxe/eap.h>

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

/** EAPoL start */
#define EAPOL_TYPE_START 1

/** EAPoL key */
#define EAPOL_TYPE_KEY 5

/** An EAPoL supplicant */
struct eapol_supplicant {
	/** EAP supplicant */
	struct eap_supplicant eap;
	/** EAPoL-Start retransmission timer */
	struct retry_timer timer;
	/** EAPoL-Start transmission count */
	unsigned int count;
};

/** Delay between EAPoL-Start packets */
#define EAPOL_START_INTERVAL ( 2 * TICKS_PER_SEC )

/** Maximum number of EAPoL-Start packets to transmit */
#define EAPOL_START_COUNT 3

/** An EAPoL handler */
struct eapol_handler {
	/** Type */
	uint8_t type;
	/**
	 * Process received packet
	 *
	 * @v supplicant	EAPoL supplicant
	 * @v iobuf		I/O buffer
	 * @v ll_source		Link-layer source address
	 * @ret rc		Return status code
	 *
	 * This method takes ownership of the I/O buffer.
	 */
	int ( * rx ) ( struct eapol_supplicant *supplicant,
		       struct io_buffer *iobuf, const void *ll_source );
};

/** EAPoL handler table */
#define EAPOL_HANDLERS __table ( struct eapol_handler, "eapol_handlers" )

/** Declare an EAPoL handler */
#define __eapol_handler __table_entry ( EAPOL_HANDLERS, 01 )

extern struct net_protocol eapol_protocol __net_protocol;

#endif /* _IPXE_EAPOL_H */
