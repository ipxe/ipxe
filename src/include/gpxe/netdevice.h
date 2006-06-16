#ifndef _GPXE_NETDEVICE_H
#define _GPXE_NETDEVICE_H

/** @file
 *
 * Network device management
 *
 */

#include <stdint.h>
#include <gpxe/tables.h>

struct pk_buff;
struct net_device;
struct net_protocol;
struct ll_protocol;

/** Maximum length of a link-layer address */
#define MAX_LL_ADDR_LEN 6

/** Maximum length of a link-layer header */
#define MAX_LL_HEADER_LEN 16

/** Maximum length of a network-layer address */
#define MAX_NET_ADDR_LEN 4

/* Network-layer address may be required to hold a link-layer address
 * (if NETADDR_FL_RAW is set
 */
#if MAX_NET_ADDR_LEN < MAX_LL_ADDR_LEN
#undef MAX_NET_ADDR_LEN
#define MAX_NET_ADDR_LEN MAX_LL_ADDR_LEN
#endif

/** A generic network-layer header */
struct net_header {
	/** Network-layer protocol */
	struct net_protocol *net_protocol;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more PKT_FL_XXX
	 * values.
	 */
	int flags;
	/** Network-layer destination address */
	uint8_t dest_net_addr[MAX_NET_ADDR_LEN];
	/** Network-layer source address */
	uint8_t source_net_addr[MAX_NET_ADDR_LEN];
};

/** Packet is a broadcast packet */
#define PKT_FL_BROADCAST 0x01

/** Packet is a multicast packet */
#define PKT_FL_MULTICAST 0x02

/** Addresses are raw hardware addresses */
#define PKT_FL_RAW_ADDR 0x04

/** A generic link-layer header */
struct ll_header {
	/** Link-layer protocol */
	struct ll_protocol *ll_protocol;
	/** Flags
	 *
	 * This is the bitwise OR of zero or more PKT_FL_XXX
	 * values.
	 */
	int flags;
	/** Link-layer destination address */
	uint8_t dest_ll_addr[MAX_LL_ADDR_LEN];
	/** Link-layer source address */
	uint8_t source_ll_addr[MAX_LL_ADDR_LEN];
	/** Network-layer protocol
	 *
	 *
	 * This is an ETH_P_XXX constant, in network-byte order
	 */
	uint16_t net_proto;
};

/**
 * A network-layer protocol
 *
 */
struct net_protocol {
	/** Protocol name */
	const char *name;
	/**
	 * Process received packet
	 *
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method takes ownership of the packet buffer.
	 */
	int ( * rx_process ) ( struct pk_buff *pkb );
	/**
	 * Transcribe network-layer address
	 *
	 * @v net_addr	Network-layer address
	 * @ret string	Human-readable transcription of address
	 *
	 * This method should convert the network-layer address into a
	 * human-readable format (e.g. dotted quad notation for IPv4).
	 *
	 * The buffer used to hold the transcription is statically
	 * allocated.
	 */
	const char * ( *ntoa ) ( const void * net_addr );
	/** Network-layer protocol
	 *
	 * This is an ETH_P_XXX constant, in network-byte order
	 */
	uint16_t net_proto;
	/** Network-layer address length */
	uint8_t net_addr_len;
};

/**
 * A link-layer protocol
 *
 */
struct ll_protocol {
	/** Protocol name */
	const char *name;
	/**
	 * Transmit network-layer packet via network device
	 *
	 *
	 * @v pkb		Packet buffer
	 * @v netdev		Network device
	 * @v net_protocol	Network-layer protocol
	 * @v ll_dest		Link-layer destination address
	 * @ret rc		Return status code
	 *
	 * This method should prepend in the link-layer header
	 * (e.g. the Ethernet DIX header) and transmit the packet.
	 */
	int ( * transmit ) ( struct pk_buff *pkb, struct net_device *netdev,
			     struct net_protocol *net_protocol,
			     const void *ll_dest );
	/**
	 * Parse media-specific link-layer header
	 *
	 * @v pkb	Packet buffer
	 * @v llhdr	Generic link-layer header
	 *
	 * This method should fill in the generic link-layer header
	 * based on information in the link-layer header in the packet
	 * buffer.
	 */
	void ( * parse_llh ) ( const struct pk_buff *pkb,
			       struct ll_header *llhdr );

	/**
	 * Transcribe link-layer address
	 *
	 * @v ll_addr	Link-layer address
	 * @ret string	Human-readable transcription of address
	 *
	 * This method should convert the link-layer address into a
	 * human-readable format.
	 *
	 * The buffer used to hold the transcription is statically
	 * allocated.
	 */
	const char * ( *ntoa ) ( const void * ll_addr );
	/** Link-layer protocol
	 *
	 * This is an ARPHRD_XXX constant, in network byte order.
	 */
	uint16_t ll_proto;
	/** Link-layer address length */
	uint8_t ll_addr_len;
	/** Link-layer broadcast address */
	const uint8_t *ll_broadcast;
};

/**
 * A network-layer address assigned to a network device
 *
 */
struct net_address {
	/** Network-layer protocol */
	struct net_protocol *net_protocol;
	/** Network-layer address */
	uint8_t net_addr[MAX_NET_ADDR_LEN];
};

/**
 * A network device
 *
 * This structure represents a piece of networking hardware.  It has
 * properties such as a link-layer address and methods for
 * transmitting and receiving raw packets.
 *
 * Note that this structure must represent a generic network device,
 * not just an Ethernet device.
 */
struct net_device {
	/** Transmit packet
	 *
	 * @v netdev	Network device
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method should cause the hardware to initiate
	 * transmission of the packet buffer.
	 *
	 * Ownership of the packet buffer is transferred to the @c
	 * net_device, which must eventually call free_pkb() to
	 * release the buffer.
	 */
	int ( * transmit ) ( struct net_device *netdev, struct pk_buff *pkb );
	/** Poll for received packet
	 *
	 * @v netdev	Network device
	 *
	 * This method should cause the hardware to check for received
	 * packets.  Any received packets should be delivered via
	 * netdev_rx().
	 */
	void ( * poll ) ( struct net_device *netdev );

	/** Link-layer protocol */
	struct ll_protocol *ll_protocol;
	/** Link-layer address
	 *
	 * For Ethernet, this is the MAC address.
	 */
	uint8_t ll_addr[MAX_LL_ADDR_LEN];

	/** Driver private data */
	void *priv;
};

extern struct net_device static_single_netdev;

/**
 * Allocate network device
 *
 * @v priv_size		Size of private data area (net_device::priv)
 * @ret netdev		Network device, or NULL
 *
 * Allocates space for a network device and its private data area.
 *
 * This macro allows for a very efficient implementation in the case
 * of a single static network device; it neatly avoids dynamic
 * allocation and can never return failure, meaning that the failure
 * path will be optimised away.  However, driver writers should not
 * rely on this feature; the drivers should be written to allow for
 * multiple instances of network devices.
 */
#define alloc_netdev( priv_size ) ( {		\
	static char priv_data[priv_size];	\
	static_single_netdev.priv = priv_data;	\
	&static_single_netdev; } )

/**
 * Free network device
 *
 * @v netdev		Network device
 */
static inline void
free_netdev ( struct net_device *netdev __attribute__ (( unused )) ) {
	/* Nothing to do */
}

/**
 * Transmit raw packet via network device
 *
 * @v netdev		Network device
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 * Transmits the packet via the specified network device.  The
 * link-layer header must already have been filled in.  This function
 * takes ownership of the packet buffer.
 */
static inline int netdev_transmit ( struct net_device *netdev,
				    struct pk_buff *pkb ) {
	return netdev->transmit ( netdev, pkb );
}

/**
 * Transmit network-layer packet
 *
 * @v pkb		Packet buffer
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v ll_dest		Destination link-layer address
 * @ret rc		Return status code
 *
 * Prepends link-layer headers to the packet buffer and transmits the
 * packet via the specified network device.  This function takes
 * ownership of the packet buffer.
 */
static inline int net_transmit ( struct pk_buff *pkb,
				 struct net_device *netdev,
				 struct net_protocol *net_protocol,
				 const void *ll_dest ) {
	return netdev->ll_protocol->transmit ( pkb, netdev, net_protocol,
					       ll_dest );
}

/**
 * Register a link-layer protocol
 *
 * @v protocol		Link-layer protocol
 */
#define LL_PROTOCOL( protocol ) \
	struct ll_protocol protocol __table ( ll_protocols, 01 )

/**
 * Register a network-layer protocol
 *
 * @v protocol		Network-layer protocol
 */
#define NET_PROTOCOL( protocol ) \
	struct net_protocol protocol __table ( net_protocols, 01 )

/**
 * Register a network-layer address for the static single network device
 *
 * @v net_address	Network-layer address
 */
#define STATIC_SINGLE_NETDEV_ADDRESS( address ) \
	struct net_address address __table ( sgl_netdev_addresses, 01 )

extern int register_netdev ( struct net_device *netdev );
extern void unregister_netdev ( struct net_device *netdev );
extern void netdev_rx ( struct net_device *netdev, struct pk_buff *pkb );

extern struct net_protocol *find_net_protocol ( uint16_t net_proto );
extern struct net_device *
find_netdev_by_net_addr ( struct net_protocol *net_protocol, void *net_addr );

extern int net_poll ( void );
extern struct pk_buff * net_rx_dequeue ( void );
extern int net_rx_process ( struct pk_buff *pkb );

#endif /* _GPXE_NETDEVICE_H */
