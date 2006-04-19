#ifndef _NETDEVICE_H
#define _NETDEVICE_H

/** @file
 *
 * Network device and network interface
 *
 */

#include <stdint.h>
#include <gpxe/llh.h>
#include <gpxe/list.h>

struct net_device;
struct net_interface;
struct pk_buff;

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
	 * @v retrieve	Flag indicating whether or not to retrieve packet
	 * @v pkb	Packet buffer to contain received packet
	 * @ret rc	Return status code
	 *
	 * This method should cause the hardware to check for a
	 * received packet.  If no packet is available, the method
	 * should return -EAGAIN (i.e. this method is *always*
	 * considered to be a non-blocking read).  If a packet is
	 * available, but @c retrieve is false, the method should
	 * return zero for success.  If a packet is available and @c
	 * retrieve is true, the method should fill the packet buffer
	 * and return zero for success.
	 */
	int ( * poll ) ( struct net_device *netdev, int retrieve,
			 struct pk_buff *pkb );
	/** Build media-specific link-layer header
	 *
	 * @v netdev	Network device
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method should convert the packet buffer's generic
	 * link-layer header (a struct gpxehdr) into a media-specific
	 * link-layer header (e.g. a struct ethhdr).  The generic
	 * header should be removed from the buffer (via pkb_pull())
	 * and the media-specific header should be prepended (via
	 * pkb_push()) in its place.
	 *
	 * If a link-layer header cannot be constructed (e.g. because
	 * of a missing ARP cache entry), then this method should
	 * return an error (after transmitting an ARP request, if
	 * applicable).
	 */
	int ( * make_media_header ) ( struct net_device *netdev,
				      struct pk_buff *pkb );
	/** Build media-independent link-layer header
	 *
	 * @v netdev	Network device
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method should convert the packet buffer's
	 * media-specific link-layer header (e.g. a struct ethhdr)
	 * into a generic link-layer header (a struct gpxehdr).  It
	 * performs the converse function of make_media_header().
	 *
	 * Note that the gpxehdr::addr and gpxehdr::addrlen fields
	 * will not be filled in by this function, since doing so
	 * would require understanding the network-layer header.
	 */
	int ( * make_generic_header ) ( struct net_device *netdev,
					struct pk_buff *pkb );
	/** Link-layer protocol
	 *
	 * This is an ARPHRD_XXX constant, in network byte order.
	 */
	uint16_t ll_proto;
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
	/** Add media-independent link-layer header
	 *
	 * @v netif	Network interface
	 * @v pkb	Packet buffer
	 * @ret rc	Return status code
	 *
	 * This method should prepend a generic link-layer header (a
	 * struct @c gpxehdr) to the packet buffer using pkb_push().
	 */
	int ( * add_generic_header ) ( struct net_interface *netif,
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

static inline void unregister_netdevice ( struct net_device *netdev __unused ){
	/* Do nothing */
}

static inline void free_netdevice ( struct net_device *netdev __unused ) {
	/* Do nothing */
}

#endif /* _NETDEVICE_H */
