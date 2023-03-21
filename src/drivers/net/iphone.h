#ifndef _IPHONE_H
#define _IPHONE_H

/** @file
 *
 * iPhone USB Ethernet driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/usb.h>
#include <ipxe/usbnet.h>
#include <ipxe/process.h>
#include <ipxe/timer.h>
#include <ipxe/retry.h>
#include <ipxe/tcp.h>
#include <ipxe/x509.h>
#include <ipxe/privkey.h>

/******************************************************************************
 *
 * iPhone pairing certificates
 *
 ******************************************************************************
 */

/** An iPhone pairing certificate set */
struct icert {
	/** "Private" key */
	struct private_key *key;
	/** Root certificate */
	struct x509_certificate *root;
	/** Host certificate */
	struct x509_certificate *host;
	/** Device certificate */
	struct x509_certificate *device;
};

/******************************************************************************
 *
 * iPhone USB multiplexer
 *
 ******************************************************************************
 */

/** An iPhone USB multiplexed packet header */
struct imux_header {
	/** Protocol */
	uint32_t protocol;
	/** Length (including this header) */
	uint32_t len;
	/** Reserved */
	uint32_t reserved;
	/** Output sequence number */
	uint16_t out_seq;
	/** Input sequence number */
	uint16_t in_seq;
} __attribute__ (( packed ));

/** iPhone USB multiplexer protocols */
enum imux_protocol {
	/** Version number */
	IMUX_VERSION = 0,
	/** Log message */
	IMUX_LOG = 1,
	/** TCP packet */
	IMUX_TCP = IP_TCP,
};

/** An iPhone USB multiplexed version message header */
struct imux_header_version {
	/** Multiplexed packet header */
	struct imux_header hdr;
	/** Reserved */
	uint32_t reserved;
} __attribute__ (( packed ));

/** An iPhone USB multiplexed log message header */
struct imux_header_log {
	/** Multiplexed packet header */
	struct imux_header hdr;
	/** Log level */
	uint8_t level;
	/** Message */
	char msg[0];
} __attribute__ (( packed ));

/** An iPhone USB multiplexed pseudo-TCP message header */
struct imux_header_tcp {
	/** Multiplexed packet header */
	struct imux_header hdr;
	/** Pseudo-TCP header */
	struct tcp_header tcp;
} __attribute__ (( packed ));

/** Local port number
 *
 * This is a policy decision.
 */
#define IMUX_PORT_LOCAL 0x18ae

/** Lockdown daemon port number */
#define IMUX_PORT_LOCKDOWND 62078

/** Advertised TCP window
 *
 * This is a policy decision.
 */
#define IMUX_WINDOW 0x0200

/** An iPhone USB multiplexer */
struct imux {
	/** Reference counter */
	struct refcnt refcnt;
	/** USB device */
	struct usb_device *usb;
	/** USB bus */
	struct usb_bus *bus;
	/** USB network device */
	struct usbnet_device usbnet;
	/** List of USB multiplexers */
	struct list_head list;

	/** Polling process */
	struct process process;
	/** Pending action
	 *
	 * @v imux		USB multiplexer
	 * @ret rc		Return status code
	 */
	int ( * action ) ( struct imux *imux );

	/** Input sequence */
	uint16_t in_seq;
	/** Output sequence */
	uint16_t out_seq;
	/** Pseudo-TCP sequence number */
	uint32_t tcp_seq;
	/** Pseudo-TCP acknowledgement number */
	uint32_t tcp_ack;
	/** Pseudo-TCP local port number */
	uint16_t port;

	/** Pseudo-TCP lockdown socket interface */
	struct interface tcp;
	/** Pairing flags */
	unsigned int flags;
	/** Pairing status */
	int rc;
};

/** Multiplexer bulk IN maximum fill level
 *
 * This is a policy decision.
 */
#define IMUX_IN_MAX_FILL 1

/** Multiplexer bulk IN buffer size
 *
 * This is a policy decision.
 */
#define IMUX_IN_MTU 4096

/******************************************************************************
 *
 * iPhone pairing client
 *
 ******************************************************************************
 */

/** An iPhone USB multiplexed pseudo-TCP XML message header */
struct ipair_header {
	/** Message length */
	uint32_t len;
	/** Message */
	char msg[0];
} __attribute__ (( packed ));

/** An iPhone pairing client */
struct ipair {
	/** Reference counter */
	struct refcnt refcnt;
	/** Data transfer interface */
	struct interface xfer;

	/** Pairing timer */
	struct retry_timer timer;
	/** Transmit message
	 *
	 * @v ipair		Pairing client
	 * @ret rc		Return status code
	 */
	int ( * tx ) ( struct ipair *ipair );
	/** Receive message
	 *
	 * @v ipair		Pairing client
	 * @v msg		XML message
	 * @ret rc		Return status code
	 */
	int ( * rx ) ( struct ipair *ipair, char *msg );
	/** State flags */
	unsigned int flags;

	/** Pairing certificates */
	struct icert icert;
};

/** Pairing client state flags */
enum ipair_flags {
	/** Request a new pairing */
	IPAIR_REQUEST = 0x0001,
	/** Standalone length has been received */
	IPAIR_RX_LEN = 0x0002,
	/** TLS session has been started */
	IPAIR_TLS = 0x0004,
};

/** Pairing retry delay
 *
 * This is a policy decision.
 */
#define IPAIR_RETRY_DELAY ( 1 * TICKS_PER_SEC )

/******************************************************************************
 *
 * iPhone USB networking
 *
 ******************************************************************************
 */

/** Get MAC address */
#define IPHONE_GET_MAC							\
	( USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE |		\
	  USB_REQUEST_TYPE ( 0x00 ) )

/** Get link status */
#define IPHONE_GET_LINK							\
	( USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE |		\
	  USB_REQUEST_TYPE ( 0x45 ) )

/** An iPhone link status */
enum iphone_link_status {
	/** Personal Hotspot is disabled */
	IPHONE_LINK_DISABLED = 0x03,
	/** Link up */
	IPHONE_LINK_UP = 0x04,
	/** Link not yet determined */
	IPHONE_LINK_UNKNOWN = -1U,
};

/** An iPhone network device */
struct iphone {
	/** USB device */
	struct usb_device *usb;
	/** USB bus */
	struct usb_bus *bus;
	/** Network device */
	struct net_device *netdev;
	/** USB network device */
	struct usbnet_device usbnet;

	/** List of iPhone network devices */
	struct list_head list;
	/** Link status check timer */
	struct retry_timer timer;
};

/** Bulk IN padding */
#define IPHONE_IN_PAD 2

/** Bulk IN buffer size
 *
 * This is a policy decision.
 */
#define IPHONE_IN_MTU ( ETH_FRAME_LEN + IPHONE_IN_PAD )

/** Bulk IN maximum fill level
 *
 * This is a policy decision.
 */
#define IPHONE_IN_MAX_FILL 8

/** Link check interval
 *
 * This is a policy decision.
 */
#define IPHONE_LINK_CHECK_INTERVAL ( 5 * TICKS_PER_SEC )

#endif /* _IPHONE_H */
