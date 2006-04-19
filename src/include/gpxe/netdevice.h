#ifndef _NETDEVICE_H
#define _NETDEVICE_H

/** @file
 *
 * Network device and network interface
 *
 */

#include <stdint.h>
#include <gpxe/list.h>

struct net_device;
struct net_interface;
struct pk_buff;

/** Maximum length of a link-layer address */
#define MAX_LLH_ADDR_LEN 6

/** Maximum length of a network-layer address */
#define MAX_NET_ADDR_LEN 4

/**
 * A network device
 *
 * This structure represents a piece of networking hardware.  It has
 * properties such as a link-layer address and methods for
 * transmitting and receiving raw packets.  It does not know anything
 * about network-layer protocols (e.g. IP) or their addresses; these
 * are handled by struct @c net_interface instead.
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
	 * transmission of the packet buffer.  The buffer may be
	 * reused immediately after the method returns, and so the
	 * method should either wait for packet transmission to
	 * complete, or take a copy of the buffer contents.
	 */
	int ( * transmit ) ( struct net_device *netdev,
			     struct pk_buff *pkb );
	/** Poll for received packet
	 *
	 * @v netdev	Network device
	 * @v pkb	Packet buffer to contain received packet
	 * @ret rc	Return status code
	 *
	 * This method should cause the hardware to check for a
	 * received packet.  If no packet is available, the method
	 * should return -EAGAIN (i.e. this method is *always*
	 * considered to be a non-blocking read).  If a packet is
	 * available, the method should fill the packet buffer and
	 * return zero for success.
	 */
	int ( * poll ) ( struct net_device *netdev, struct pk_buff *pkb );
	/** Build link-layer header
	 *
	 * @v netdev	Network device
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method should fill in the link-layer header based on
	 * the metadata contained in @c pkb.
	 *
	 * If a link-layer header cannot be constructed (e.g. because
	 * of a missing ARP cache entry), then this method should
	 * return an error (after transmitting an ARP request, if
	 * applicable).
	 */
	int ( * build_llh ) ( struct net_device *netdev, struct pk_buff *pkb );
	/** Parse link-layer header
	 *
	 * @v netdev	Network device
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method should parse the link-layer header and fill in
	 * the metadata in @c pkb.
	 */
	int ( * parse_llh ) ( struct net_device *netdev, struct pk_buff *pkb );
	/** Link-layer protocol
	 *
	 * This is an ARPHRD_XXX constant, in network byte order.
	 */
	uint16_t ll_proto;
	/** Link-layer header length */
	uint8_t ll_hlen;
	/** Link-layer address length */
	uint8_t ll_addr_len;
	/** Link-layer address
	 *
	 * For Ethernet, this is the MAC address.
	 */
	uint8_t ll_addr[MAX_LLH_ADDR_LEN];
	/** List of network interfaces */
	struct list_head interfaces;
	/** Driver private data */
	void *priv;
};

/**
 * A network interface
 *
 * This structure represents a particular network layer protocol's
 * interface to a piece of network hardware (a struct @c net_device).
 *
 */
struct net_interface {
	/** Underlying net device */
	struct net_device *netdev;
	/** Linked list of interfaces for this device */
	struct list_head list;
	/** Network-layer protocol
	 *
	 * This is an ETH_P_XXX constant, in network byte order.
	 */
	uint16_t net_proto;
	/** Network-layer address length */
	uint8_t net_addr_len;
	/** Network-layer address */
	uint8_t net_addr[MAX_NET_ADDR_LEN];
	/** Packet processor
	 *
	 * @v netif	Network interface
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method is called for packets arriving on the
	 * associated network device that match this interface's
	 * network-layer protocol.
	 *
	 * When this method is called, the link-layer header will
	 * already have been stripped from the packet.
	 */
	int ( * process ) ( struct net_interface *netif,
			    struct pk_buff *pkb );
	/** Fill in packet metadata
	 *
	 * @v netif	Network interface
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method should fill in the @c pkb metadata with enough
	 * information to enable net_device::build_llh to construct
	 * the link-layer header.
	 */
	int ( * add_llh_metadata ) ( struct net_interface *netif,
				     struct pk_buff *pkb );
};

/**
 * Find interface for a specific protocol
 *
 * @v netdev	Network device
 * @v net_proto	Network-layer protocol, in network byte order
 * @ret netif	Network interface, or NULL if none found
 *
 */
static inline struct net_interface *
netdev_find_netif ( const struct net_device *netdev, uint16_t net_proto ) {
	struct net_interface *netif;

	list_for_each_entry ( netif, &netdev->interfaces, list ) {
		if ( netif->net_proto == net_proto )
			return netif;
	}
	return NULL;
}

extern int netdev_send ( struct net_device *netdev, struct pk_buff *pkb );
extern int netif_send ( struct net_interface *netif, struct pk_buff *pkb );

extern struct net_device static_single_netdev;

/* Must be a macro because priv_data[] is of variable size */
#define alloc_netdevice( priv_size ) ( {		\
	static char priv_data[priv_size];		\
	static_single_netdev.priv = priv_data;	\
	&static_single_netdev; } )

extern int register_netdevice ( struct net_device *netdev );

extern void unregister_netdevice ( struct net_device *netdev );

static inline void free_netdevice ( struct net_device *netdev __unused ) {
	/* Do nothing */
}

#endif /* _NETDEVICE_H */
