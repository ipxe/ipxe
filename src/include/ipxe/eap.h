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
	unsigned int flags;
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

extern int eap_rx ( struct eap_supplicant *supplicant,
		    const void *data, size_t len );

#endif /* _IPXE_EAP_H */
