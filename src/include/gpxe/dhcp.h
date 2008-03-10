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
#include <gpxe/refcnt.h>
#include <gpxe/tables.h>

struct net_device;
struct job_interface;

/** BOOTP/DHCP server port */
#define BOOTPS_PORT 67

/** BOOTP/DHCP client port */
#define BOOTPC_PORT 68

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

/** Subnet mask */
#define DHCP_SUBNET_MASK 1

/** Routers */
#define DHCP_ROUTERS 3

/** DNS servers */
#define DHCP_DNS_SERVERS 6

/** Syslog servers */
#define DHCP_LOG_SERVERS 7

/** Host name */
#define DHCP_HOST_NAME 12

/** Domain name */
#define DHCP_DOMAIN_NAME 15

/** Root path */
#define DHCP_ROOT_PATH 17

/** Vendor encapsulated options */
#define DHCP_VENDOR_ENCAP 43

/** Requested IP address */
#define DHCP_REQUESTED_ADDRESS 50

/** Lease time */
#define DHCP_LEASE_TIME 51

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

/** DHCP server identifier */
#define DHCP_SERVER_IDENTIFIER 54

/** Parameter request list */
#define DHCP_PARAMETER_REQUEST_LIST 55

/** Maximum DHCP message size */
#define DHCP_MAX_MESSAGE_SIZE 57

/** Vendor class identifier */
#define DHCP_VENDOR_CLASS_ID 60

/** Client identifier */
#define DHCP_CLIENT_ID 61

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

/** Client system architecture */
#define DHCP_CLIENT_ARCHITECTURE 93

/** Client network device interface */
#define DHCP_CLIENT_NDI 94

/** UUID client identifier */
#define DHCP_CLIENT_UUID 97

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

/*
 * Tags in the range 0x10-0x7f are reserved for feature markers
 *
 */

/** Ignore ProxyDHCP
 *
 * If set to a non-zero value, gPXE will not wait for ProxyDHCP offers
 * and will ignore any ProxyDHCP offers that it receives.
 */
#define DHCP_EB_NO_PROXYDHCP DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 0xb0 )

/** Network device descriptor
 *
 * Byte 0 is the bus type ID; remaining bytes depend on the bus type.
 *
 * PCI devices:
 * Byte 0 : 1 (PCI)
 * Byte 1 : PCI vendor ID MSB
 * Byte 2 : PCI vendor ID LSB
 * Byte 3 : PCI device ID MSB
 * Byte 4 : PCI device ID LSB
 */
#define DHCP_EB_BUS_ID DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 0xb1 )

/** BIOS drive number
 *
 * This is the drive number for a drive emulated via INT 13.  0x80 is
 * the first hard disk, 0x81 is the second hard disk, etc.
 */
#define DHCP_EB_BIOS_DRIVE DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 0xbd )

/** Username
 *
 * This will be used as the username for any required authentication.
 * It is expected that this option's value will be held in
 * non-volatile storage, rather than transmitted as part of a DHCP
 * packet.
 */
#define DHCP_EB_USERNAME DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 0xbe )

/** Password
 *
 * This will be used as the password for any required authentication.
 * It is expected that this option's value will be held in
 * non-volatile storage, rather than transmitted as part of a DHCP
 * packet.
 */
#define DHCP_EB_PASSWORD DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 0xbf )

/** iSCSI primary target IQN */
#define DHCP_ISCSI_PRIMARY_TARGET_IQN 201

/** iSCSI secondary target IQN */
#define DHCP_ISCSI_SECONDARY_TARGET_IQN 202

/** iSCSI initiator IQN */
#define DHCP_ISCSI_INITIATOR_IQN 203

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
 * Count number of arguments to a variadic macro
 *
 * This rather neat, non-iterative solution is courtesy of Laurent
 * Deniau.
 *
 */
#define _VA_ARG_COUNT(  _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,		\
		        _9, _10, _11, _12, _13, _14, _15, _16,		\
		       _17, _18, _19, _20, _21, _22, _23, _24,		\
		       _25, _26, _27, _28, _29, _30, _31, _32,		\
		       _33, _34, _35, _36, _37, _38, _39, _40,		\
		       _41, _42, _43, _44, _45, _46, _47, _48,		\
		       _49, _50, _51, _52, _53, _54, _55, _56,		\
		       _57, _58, _59, _60, _61, _62, _63,   N, ... ) N
#define VA_ARG_COUNT( ... )						\
	_VA_ARG_COUNT ( __VA_ARGS__, 					\
			63, 62, 61, 60, 59, 58, 57, 56,			\
			55, 54, 53, 52, 51, 50, 49, 48,			\
			47, 46, 45, 44, 43, 42, 41, 40,			\
			39, 38, 37, 36, 35, 34, 33, 32,			\
			31, 30, 29, 28, 27, 26, 25, 24,			\
			23, 22, 21, 20, 19, 18, 17, 16,			\
			15, 14, 13, 12, 11, 10,  9,  8,			\
			 7,  6,  5,  4,  3,  2,  1,  0 )

/** Construct a DHCP option from a list of bytes */
#define DHCP_OPTION( ... ) VA_ARG_COUNT ( __VA_ARGS__ ), __VA_ARGS__

/** Construct a DHCP option from a list of characters */
#define DHCP_STRING( ... ) DHCP_OPTION ( __VA_ARGS__ )

/** Construct a byte-valued DHCP option */
#define DHCP_BYTE( value ) DHCP_OPTION ( value )

/** Construct a word-valued DHCP option */
#define DHCP_WORD( value ) DHCP_OPTION ( ( ( (value) >> 8 ) & 0xff ),   \
					 ( ( (value) >> 0  ) & 0xff ) )

/** Construct a dword-valued DHCP option */
#define DHCP_DWORD( value ) DHCP_OPTION ( ( ( (value) >> 24 ) & 0xff ), \
					  ( ( (value) >> 16 ) & 0xff ), \
					  ( ( (value) >> 8  ) & 0xff ), \
					  ( ( (value) >> 0  ) & 0xff ) )

/** Construct a DHCP encapsulated options field */
#define DHCP_ENCAP( ... ) DHCP_OPTION ( __VA_ARGS__, DHCP_END )

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
		struct in_addr in;
		uint8_t bytes[0];
		char string[0];
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
	/** Reference counter */
	struct refcnt refcnt;
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
	char sname[64];
	/** Boot file name (null terminated)
	 *
	 * This field may be overridden and contain DHCP options
	 */
	char file[128];
	/** DHCP magic cookie
	 *
	 * Must have the value @c DHCP_MAGIC_COOKIE.
	 */
	uint32_t magic;
	/** DHCP options
	 *
	 * Variable length; extends to the end of the packet.  Minimum
	 * length (for the sake of sanity) is 1, to allow for a single
	 * @c DHCP_END tag.
	 */
	uint8_t options[1];
};

/** Opcode for a request from client to server */
#define BOOTP_REQUEST 1

/** Opcode for a reply from server to client */
#define BOOTP_REPLY 2

/** BOOTP reply must be broadcast
 *
 * Clients that cannot accept unicast BOOTP replies must set this
 * flag.
 */
#define BOOTP_FL_BROADCAST 0x8000

/** DHCP magic cookie */
#define DHCP_MAGIC_COOKIE 0x63825363UL

/** DHCP minimum packet length
 *
 * This is the mandated minimum packet length that a DHCP participant
 * must be prepared to receive.
 */
#define DHCP_MIN_LEN 552

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
	/** DHCP options */
	struct dhcp_option_block options;
};

/** A DHCP option applicator */
struct dhcp_option_applicator {
	/** DHCP option tag */
	unsigned int tag;
	/** Applicator
	 *
	 * @v tag	DHCP option tag
	 * @v option	DHCP option
	 * @ret rc	Return status code
	 */
	int ( * apply ) ( unsigned int tag, struct dhcp_option *option );
};

/** Declare a DHCP option applicator */
#define __dhcp_applicator \
	__table ( struct dhcp_option_applicator, dhcp_applicators, 01 )

/**
 * Get reference to DHCP options block
 *
 * @v options		DHCP options block
 * @ret options		DHCP options block
 */
static inline __attribute__ (( always_inline )) struct dhcp_option_block *
dhcpopt_get ( struct dhcp_option_block *options ) {
	ref_get ( &options->refcnt );
	return options;
}

/**
 * Drop reference to DHCP options block
 *
 * @v options		DHCP options block, or NULL
 */
static inline __attribute__ (( always_inline )) void
dhcpopt_put ( struct dhcp_option_block *options ) {
	ref_put ( &options->refcnt );
}

/** Maximum time that we will wait for ProxyDHCP offers */
#define PROXYDHCP_WAIT_TIME ( TICKS_PER_SEC * 1 )

extern struct list_head dhcp_option_blocks;

extern unsigned long dhcp_num_option ( struct dhcp_option *option );
extern void dhcp_ipv4_option ( struct dhcp_option *option,
			       struct in_addr *inp );
extern int dhcp_snprintf ( char *buf, size_t size,
			   struct dhcp_option *option );
extern struct dhcp_option *
find_dhcp_option ( struct dhcp_option_block *options, unsigned int tag );
extern void register_dhcp_options ( struct dhcp_option_block *options );
extern void unregister_dhcp_options ( struct dhcp_option_block *options );
extern void init_dhcp_options ( struct dhcp_option_block *options,
				void *data, size_t max_len );
extern struct dhcp_option_block * __malloc alloc_dhcp_options ( size_t max_len );
extern struct dhcp_option *
set_dhcp_option ( struct dhcp_option_block *options, unsigned int tag,
		  const void *data, size_t len );
extern struct dhcp_option * find_global_dhcp_option ( unsigned int tag );
extern unsigned long find_dhcp_num_option ( struct dhcp_option_block *options,
					    unsigned int tag );
extern unsigned long find_global_dhcp_num_option ( unsigned int tag );
extern void find_dhcp_ipv4_option ( struct dhcp_option_block *options,
				    unsigned int tag, struct in_addr *inp );
extern void find_global_dhcp_ipv4_option ( unsigned int tag,
					   struct in_addr *inp );
extern void delete_dhcp_option ( struct dhcp_option_block *options,
				 unsigned int tag );

extern int apply_dhcp_options ( struct dhcp_option_block *options );
extern int apply_global_dhcp_options ( void );

extern int create_dhcp_request ( struct net_device *netdev, int msgtype,
				 struct dhcp_option_block *options,
				 void *data, size_t max_len,
				 struct dhcp_packet *dhcppkt );
extern int create_dhcp_response ( struct net_device *netdev, int msgtype,
				  struct dhcp_option_block *options,
				  void *data, size_t max_len,
				  struct dhcp_packet *dhcppkt );

extern int start_dhcp ( struct job_interface *job, struct net_device *netdev,
			int (*register_options) ( struct net_device *,
						  struct dhcp_option_block * ));
extern int dhcp_configure_netdev ( struct net_device *netdev,
				   struct dhcp_option_block *options );

#endif /* _GPXE_DHCP_H */
