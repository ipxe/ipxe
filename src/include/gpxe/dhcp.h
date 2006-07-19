#ifndef _GPXE_DHCP_H
#define _GPXE_DHCP_H

/** @file
 *
 * Dynamic Host Configuration Protocol
 *
 */

#include <stdint.h>
#include <gpxe/list.h>
#include <gpxe/in.h>

/** Construct a tag value for an encapsulated option
 *
 * This tag value can be passed to Etherboot functions when searching
 * for DHCP options in order to search for a tag within an
 * encapsulated options block.
 */
#define DHCP_ENCAP_OPT( encapsulator, encapsulated ) \
	( ( (encapsulator) << 8 ) | (encapsulated) )
/** Extract encapsulating option block tag from encapsulated tag value */
#define DHCP_ENCAPSULATOR( encap_opt ) ( (encap_opt) >> 8 )
/** Extract encapsulated option tag from encapsulated tag value */
#define DHCP_ENCAPSULATED( encap_opt ) ( (encap_opt) & 0xff )
/** Option is encapsulated */
#define DHCP_IS_ENCAP_OPT( opt ) DHCP_ENCAPSULATOR( opt )

/**
 * @defgroup dhcpopts DHCP option tags
 * @{
 */

/** Padding
 *
 * This tag does not have a length field; it is always only a single
 * byte in length.
 */
#define DHCP_PAD 0

/** Minimum normal DHCP option */
#define DHCP_MIN_OPTION 1

/** Vendor encapsulated options */
#define DHCP_VENDOR_ENCAP 43

/** Option overloading
 *
 * The value of this option is the bitwise-OR of zero or more
 * DHCP_OPTION_OVERLOAD_XXX constants.
 */
#define DHCP_OPTION_OVERLOAD 52

/** The "file" field is overloaded to contain extra DHCP options */
#define DHCP_OPTION_OVERLOAD_FILE 1

/** The "sname" field is overloaded to contain extra DHCP options */
#define DHCP_OPTION_OVERLOAD_SNAME 2

/** DHCP message type */
#define DHCP_MESSAGE_TYPE 53
#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPDECLINE 4
#define DHCPACK 5
#define DHCPNAK 6
#define DHCPRELEASE 7
#define DHCPINFORM 8

/** TFTP server name
 *
 * This option replaces the fixed "sname" field, when that field is
 * used to contain overloaded options.
 */
#define DHCP_TFTP_SERVER_NAME 66

/** Bootfile name
 *
 * This option replaces the fixed "file" field, when that field is
 * used to contain overloaded options.
 */
#define DHCP_BOOTFILE_NAME 67

/** Etherboot-specific encapsulated options
 *
 * This encapsulated options field is used to contain all options
 * specific to Etherboot (i.e. not assigned by IANA or other standards
 * bodies).
 */
#define DHCP_EB_ENCAP 175

/** Priority of this options block
 *
 * This is a signed 8-bit integer field indicating the priority of
 * this block of options.  It can be used to specify the relative
 * priority of multiple option blocks (e.g. options from non-volatile
 * storage versus options from a DHCP server).
 */
#define DHCP_EB_PRIORITY DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 1 )

/** "Your" IP address
 *
 * This option is used internally to contain the value of the "yiaddr"
 * field, in order to provide a consistent approach to storing and
 * processing options.  It should never be present in a DHCP packet.
 */
#define DHCP_EB_YIADDR DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 2 )

/** "Server" IP address
 *
 * This option is used internally to contain the value of the "siaddr"
 * field, in order to provide a consistent approach to storing and
 * processing options.  It should never be present in a DHCP packet.
 */
#define DHCP_EB_SIADDR DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 3 )

/** Maximum normal DHCP option */
#define DHCP_MAX_OPTION 254

/** End of options
 *
 * This tag does not have a length field; it is always only a single
 * byte in length.
 */
#define DHCP_END 255

/** @} */

/**
 * A DHCP option
 *
 * DHCP options consist of a mandatory tag, a length field that is
 * mandatory for all options except @c DHCP_PAD and @c DHCP_END, and a
 * payload.  
 */
struct dhcp_option {
	/** Tag
	 *
	 * Must be a @c DHCP_XXX value.
	 */
	uint8_t tag;
	/** Length
	 *
	 * This is the length of the data field (i.e. excluding the
	 * tag and length fields).  For the two tags @c DHCP_PAD and
	 * @c DHCP_END, the length field is implicitly zero and is
	 * also missing, i.e. these DHCP options are only a single
	 * byte in length.
	 */
	uint8_t len;
	/** Option data
	 *
	 * Interpretation of the content is entirely dependent upon
	 * the tag.  For fields containing a multi-byte integer, the
	 * field is defined to be in network-endian order (unless you
	 * are Intel and feel like violating the spec for fun).
	 */
	union {
		uint8_t byte;
		uint16_t word;
		uint32_t dword;
		uint8_t bytes[0];
	} data;
} __attribute__ (( packed ));

/**
 * Length of a DHCP option header
 *
 * The header is the portion excluding the data, i.e. the tag and the
 * length.
 */
#define DHCP_OPTION_HEADER_LEN ( offsetof ( struct dhcp_option, data ) )

/** Maximum length for a single DHCP option */
#define DHCP_MAX_LEN 0xff

/** A DHCP options block */
struct dhcp_option_block {
	/** List of option blocks */
	struct list_head list;
	/** Option block raw data */
	void *data;
	/** Option block length */
	size_t len;
	/** Option block maximum length */
	size_t max_len;
	/** Block priority
	 *
	 * This is determined at the time of the call to
	 * register_options() by searching for the @c DHCP_EB_PRIORITY
	 * option.
	 */
	signed int priority;
};

/**
 * A DHCP header
 *
 */
struct dhcphdr {
	/** Operation
	 *
	 * This must be either @c BOOTP_REQUEST or @c BOOTP_REPLY.
	 */
	uint8_t op;
	/** Hardware address type
	 *
	 * This is an ARPHRD_XXX constant.  Note that ARPHRD_XXX
	 * constants are nominally 16 bits wide; this could be
	 * considered to be a bug in the BOOTP/DHCP specification.
	 */
	uint8_t htype;
	/** Hardware address length */
	uint8_t hlen;
	/** Number of hops from server */
	uint8_t hops;
	/** Transaction ID */
	uint32_t xid;
	/** Seconds since start of acquisition */
	uint16_t secs;
	/** Flags */
	uint16_t flags;
	/** "Client" IP address
	 *
	 * This is filled in if the client already has an IP address
	 * assigned and can respond to ARP requests.
	 */
	struct in_addr ciaddr;
	/** "Your" IP address
	 *
	 * This is the IP address assigned by the server to the client.
	 */
	struct in_addr yiaddr;
	/** "Server" IP address
	 *
	 * This is the IP address of the next server to be used in the
	 * boot process.
	 */
	struct in_addr siaddr;
	/** "Gateway" IP address
	 *
	 * This is the IP address of the DHCP relay agent, if any.
	 */
	struct in_addr giaddr;
	/** Client hardware address */
	uint8_t chaddr[16];
	/** Server host name (null terminated)
	 *
	 * This field may be overridden and contain DHCP options
	 */
	uint8_t sname[64];
	/** Boot file name (null terminated)
	 *
	 * This field may be overridden and contain DHCP options
	 */
	uint8_t file[128];
	/** DHCP magic cookie
	 *
	 * Must have the value @c DHCP_MAGIC_COOKIE.
	 */
	uint32_t magic;
	/** DHCP options
	 *
	 * Variable length; extends to the end of the packet.
	 */
	uint8_t options[0];
};

/** Opcode for a request from client to server */
#define BOOTP_REQUEST 1

/** Opcode for a reply from server to client */
#define BOOTP_REPLY 2

/** DHCP magic cookie */
#define DHCP_MAGIC_COOKIE 0x63825363UL

/** DHCP packet option block fill order
 *
 * This is the order in which option blocks are filled when
 * reassembling a DHCP packet.  We fill the smallest field ("sname")
 * first, to maximise the chances of being able to fit large options
 * within fields which are large enough to contain them.
 */
enum dhcp_packet_option_block_fill_order {
	OPTS_SNAME = 0,
	OPTS_FILE,
	OPTS_MAIN,
	NUM_OPT_BLOCKS
};

/**
 * A DHCP packet
 *
 */
struct dhcp_packet {
	/** The DHCP packet contents */
	struct dhcphdr *dhcphdr;
	/** Maximum length of the DHCP packet buffer */
	size_t max_len;
	/** Used length of the DHCP packet buffer */
	size_t len;
	/** DHCP option blocks within a DHCP packet
	 *
	 * A DHCP packet contains three fields which can be used to
	 * contain options: the actual "options" field plus the "file"
	 * and "sname" fields (which can be overloaded to contain
	 * options).
	 */
	struct dhcp_option_block options[NUM_OPT_BLOCKS];
};

/** A DHCP session */
struct dhcp_session {
	/** Network device being configured */
	struct net_device *netdev;
	/** Transaction ID, in network-endian order */
	uint32_t xid;
};

extern unsigned long dhcp_num_option ( struct dhcp_option *option );
extern struct dhcp_option *
find_dhcp_option ( struct dhcp_option_block *options, unsigned int tag );
extern void register_dhcp_options ( struct dhcp_option_block *options );
extern void unregister_dhcp_options ( struct dhcp_option_block *options );
extern void init_dhcp_options ( struct dhcp_option_block *options,
				void *data, size_t max_len );
extern struct dhcp_option_block * alloc_dhcp_options ( size_t max_len );
extern void free_dhcp_options ( struct dhcp_option_block *options );
extern struct dhcp_option *
set_dhcp_option ( struct dhcp_option_block *options, unsigned int tag,
		  const void *data, size_t len );
extern struct dhcp_option * find_global_dhcp_option ( unsigned int tag );
extern unsigned long find_dhcp_num_option ( struct dhcp_option_block *options,
					    unsigned int tag );
extern unsigned long find_global_dhcp_num_option ( unsigned int tag );
extern void delete_dhcp_option ( struct dhcp_option_block *options,
				 unsigned int tag );

#endif /* _GPXE_DHCP_H */
