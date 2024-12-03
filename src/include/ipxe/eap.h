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
#include <ipxe/tables.h>

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

/** EAP response */
#define EAP_CODE_RESPONSE 2

/** EAP request/response message */
struct eap_message {
	/** Header */
	struct eap_header hdr;
	/** Type */
	uint8_t type;
	/** Type data */
	uint8_t data[0];
} __attribute__ (( packed ));

/** EAP "no available types" marker */
#define EAP_TYPE_NONE 0

/** EAP identity */
#define EAP_TYPE_IDENTITY 1

/** EAP NAK */
#define EAP_TYPE_NAK 3

/** EAP MD5 challenge request/response */
#define EAP_TYPE_MD5 4

/** EAP MD5 challenge request/response type data */
struct eap_md5 {
	/** Value length */
	uint8_t len;
	/** Value */
	uint8_t value[0];
} __attribute__ (( packed ));

/** EAP MS-CHAPv2 request/response */
#define EAP_TYPE_MSCHAPV2 26

/** EAP MS-CHAPv2 request/response type data */
struct eap_mschapv2 {
	/** Code
	 *
	 * This is in the same namespace as the EAP header's code
	 * field, but is used to extend the handshake by allowing for
	 * "success request" and "success response" packets.
	 */
	uint8_t code;
	/** Identifier
	 *
	 * This field serves no purposes: it always has the same value
	 * as the EAP header's identifier field (located 5 bytes
	 * earlier in the same packet).
	 */
	uint8_t id;
	/** Length
	 *
	 * This field serves no purpose: it always has the same value
	 * as the EAP header's length field (located 5 bytes earlier
	 * in the same packet), minus the 5 byte length of the EAP
	 * header.
	 */
	uint16_t len;
} __attribute__ (( packed ));

/** EAP success */
#define EAP_CODE_SUCCESS 3

/** EAP failure */
#define EAP_CODE_FAILURE 4

/** EAP packet */
union eap_packet {
	/** Header */
	struct eap_header hdr;
	/** Request/response message */
	struct eap_message msg;
};

/** EAP link block timeout
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

/** EAP protocol wait timeout
 *
 * In the EAP model, the supplicant is a pure responder.  The model
 * also defines no acknowledgement response for the final Success or
 * Failure "requests".  This leaves open the possibility that the
 * final Success or Failure packet is lost, with the supplicant having
 * no way to determine the final authentication status.
 *
 * Sideband mechanisms such as EAPoL-Start may be used to restart the
 * entire EAP process, as a (crude) workaround for this protocol flaw.
 * When expecting to receive a further EAP request (e.g. an
 * authentication challenge), we may wait for some length of time
 * before triggering this restart.  Choose a duration that is shorter
 * than the link block timeout, so that there is no period during
 * which we erroneously leave the link marked as not blocked.
 */
#define EAP_WAIT_TIMEOUT ( EAP_BLOCK_TIMEOUT * 7 / 8 )

/** An EAP supplicant */
struct eap_supplicant {
	/** Network device */
	struct net_device *netdev;
	/** Flags */
	uint16_t flags;
	/** ID for current request/response */
	uint8_t id;
	/** Type for current request/response */
	uint8_t type;
	/**
	 * Transmit EAP response
	 *
	 * @v supplicant	EAP supplicant
	 * @v data		Response data
	 * @v len		Length of response data
	 * @ret rc		Return status code
	 */
	int ( * tx ) ( struct eap_supplicant *supplicant,
		       const void *data, size_t len );
};

/** EAP authentication is in progress
 *
 * This indicates that we have received an EAP Request-Identity, but
 * have not yet received a final EAP Success or EAP Failure.
 */
#define EAP_FL_ONGOING 0x0001

/** EAP supplicant is passive
 *
 * This indicates that the supplicant should not transmit any futher
 * unsolicited packets (e.g. EAPoL-Start for a supplicant running over
 * EAPoL).  This could be because authentication has already
 * completed, or because we are relying upon MAC Authentication Bypass
 * (MAB) which may have a very long timeout.
 */
#define EAP_FL_PASSIVE 0x0002

/** An EAP method */
struct eap_method {
	/** Type */
	uint8_t type;
	/**
	 * Handle EAP request
	 *
	 * @v supplicant	EAP supplicant
	 * @v req		Request type data
	 * @v req_len		Length of request type data
	 * @ret rc		Return status code
	 */
	int ( * rx ) ( struct eap_supplicant *supplicant,
		       const void *req, size_t req_len );
};

/** EAP method table */
#define EAP_METHODS __table ( struct eap_method, "eap_methods" )

/** Declare an EAP method */
#define __eap_method __table_entry ( EAP_METHODS, 01 )

extern int eap_tx_response ( struct eap_supplicant *supplicant,
			     const void *rsp, size_t rsp_len );
extern int eap_rx ( struct eap_supplicant *supplicant,
		    const void *data, size_t len );

#endif /* _IPXE_EAP_H */
