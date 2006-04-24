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
struct net_protocol;
struct ll_protocol;

/** Maximum length of a link-layer address */
#define MAX_LL_ADDR_LEN 6

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
	/** Destination address flags
	 *
	 * This is the bitwise OR of zero or more NETADDR_FL_XXX
	 * values.
	 */
	int dest_flags;
	/** Network-layer destination address */
	uint8_t dest_net_addr[MAX_NET_ADDR_LEN];
	/** Network-layer source address */
	uint8_t source_net_addr[MAX_NET_ADDR_LEN];
};

/** Address is a broadcast address */
#define NETADDR_FL_BROADCAST 0x01

/** Address is a multicast address */
#define NETADDR_FL_MULTICAST 0x02

/** Address is a raw hardware address */
#define NETADDR_FL_RAW 0x04

/** A generic link-layer header */
struct ll_header {
	/** Link-layer protocol */
	struct ll_protocol *ll_protocol;
	/** Destination address flags
	 *
	 * This is the bitwise OR of zero or more NETADDR_FL_XXX
	 * values.
	 */
	int dest_flags;
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
	/**
	 * Perform network-layer routing
	 *
	 * @v pkb	Packet buffer
	 * @ret source	Network-layer source address
	 * @ret dest	Network-layer destination address
	 * @ret rc	Return status code
	 *
	 * This method should fill in the source and destination
	 * addresses with enough information to allow the link layer
	 * to route the packet.
	 *
	 * For example, in the case of IPv4, this method should fill
	 * in @c source with the IP addresses of the local adapter and
	 * @c dest with the next hop destination (e.g. the gateway).
	 */
	int ( * route ) ( const struct pk_buff *pkb,
			  struct net_header *nethdr );
	/**
	 * Handle received packets
	 *
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * If this method returns success, it has taken ownership of
	 * the packet buffer.
	 */
	int ( * rx ) ( struct pk_buff *pkb );
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
	/**
	 * Perform link-layer routing
	 *
	 * @v nethdr	Generic network-layer header
	 * @ret llhdr	Generic link-layer header
	 * @ret rc	Return status code
	 *
	 * This method should construct the generic link-layer header
	 * based on the generic network-layer header.
	 *
	 * If a link-layer header cannot be constructed (e.g. because
	 * of a missing ARP cache entry), then this method should
	 * return an error (after transmitting an ARP request, if
	 * applicable).
	 */
	int ( * route ) ( const struct net_header *nethdr,
			  struct ll_header *llhdr );
	/**
	 * Fill media-specific link-layer header
	 *
	 * @v llhdr	Generic link-layer header
	 * @v pkb	Packet buffer
	 *
	 * This method should fill in the link-layer header in the
	 * packet buffer based on information in the generic
	 * link-layer header.
	 */
	void ( * fill_llh ) ( const struct ll_header *llhdr,
			      struct pk_buff *pkb );
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

	/** Link-layer protocol
	 *
	 * This is an ARPHRD_XXX constant, in network byte order.
	 */
	uint16_t ll_proto;
	/** Link-layer address length */
	uint8_t ll_addr_len;
	/** Link-layer header length */
	uint8_t ll_header_len;
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
	 * If the method returns success, ownership of the packet
	 * buffer is transferred to the @c net_device, which must
	 * eventually call free_pkb() to release the buffer.
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
 * Register network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 *
 * Adds the network device to the list of network devices.
 */
static inline int
register_netdev ( struct net_device *netdev __attribute__ (( unused )) ) {
	return 0;
}

/**
 * Unregister network device
 *
 * @v netdev		Network device
 *
 * Removes the network device from the list of network devices.
 */
static inline void 
unregister_netdev ( struct net_device *netdev __attribute__ (( unused )) ) {
	/* Nothing to do */
}

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
 * Register a link-layer protocol
 *
 * @v protocol		Link-layer protocol
 */
#define LL_PROTOCOL( protocol ) \
	struct ll_protocol protocol __table ( ll_protocols, 00 )

/**
 * Register a network-layer protocol
 *
 * @v protocol		Network-layer protocol
 */
#define NET_PROTOCOL( protocol ) \
	struct net_protocol protocol __table ( net_protocols, 00 )

/**
 * Register a network-layer address for the static single network device
 *
 * @v net_address	Network-layer address
 */
#define STATIC_SINGLE_NETDEV_ADDRESS( address ) \
	struct net_address address __table ( sgl_netdev_addresses, 00 )

extern struct net_protocol *net_find_protocol ( uint16_t net_proto );
extern struct net_device * net_find_address ( struct net_protocol *net_proto,
					      void *net_addr );

extern int net_transmit ( struct pk_buff *pkb );
extern int net_poll ( void );
extern void netdev_rx ( struct net_device *netdev, struct pk_buff *pkb );
extern struct pk_buff * net_rx_dequeue ( void );

#endif /* _GPXE_NETDEVICE_H */
