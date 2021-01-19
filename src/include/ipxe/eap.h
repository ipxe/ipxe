#ifndef _IPXE_EAP_H
#define _IPXE_EAP_H

/** @file
 *
 * Extensible Authentication Protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/netdevice.h>
#include <ipxe/timer.h>

/** EAP header */
struct eap_header {
	/** Code */
	uint8_t code;
	/** Identifier */
	uint8_t id;
	/** Length */
	uint16_t len;
} __attribute__ (( packed ));

/** EAP request */
#define EAP_CODE_REQUEST 1

/** EAP request */
struct eap_request {
	/** Header */
	struct eap_header hdr;
	/** Type */
	uint8_t type;
} __attribute__ (( packed ));

/** EAP identity */
#define EAP_TYPE_IDENTITY 1

/** EAP success */
#define EAP_CODE_SUCCESS 3

/** EAP failure */
#define EAP_CODE_FAILURE 4

/** EAP packet */
union eap_packet {
	/** Header */
	struct eap_header hdr;
	/** Request */
	struct eap_request req;
};

/** Link block timeout
 *
 * We mark the link as blocked upon receiving a Request-Identity, on
 * the basis that this most likely indicates that the switch will not
 * yet be forwarding packets.
 *
 * There is no way to tell how frequently the Request-Identity packet
 * will be retransmitted by the switch.  The default value for Cisco
 * switches seems to be 30 seconds, so treat the link as blocked for
 * 45 seconds.
 */
#define EAP_BLOCK_TIMEOUT ( 45 * TICKS_PER_SEC )

extern int eap_rx ( struct net_device *netdev, const void *data, size_t len );

#endif /* _IPXE_EAP_H */
