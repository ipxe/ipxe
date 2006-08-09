#ifndef _GPXE_NETDEVICE_H
#define _GPXE_NETDEVICE_H

/** @file
 *
 * Network device management
 *
 */

#include <stdint.h>
#include <gpxe/list.h>
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
	 * @v netdev	Network device
	 * @v ll_source	Link-layer source address
	 *
	 * This method takes ownership of the packet buffer.
	 */
	int ( * rx ) ( struct pk_buff *pkb, struct net_device *netdev,
		       const void *ll_source );
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
	 * @v pkb		Packet buffer
	 * @v netdev		Network device
	 * @v net_protocol	Network-layer protocol
	 * @v ll_dest		Link-layer destination address
	 * @ret rc		Return status code
	 *
	 * This method should prepend in the link-layer header
	 * (e.g. the Ethernet DIX header) and transmit the packet.
	 * This method takes ownership of the packet buffer.
	 */
	int ( * tx ) ( struct pk_buff *pkb, struct net_device *netdev,
		       struct net_protocol *net_protocol,
		       const void *ll_dest );
	/**
	 * Handle received packet
	 *
	 * @v pkb	Packet buffer
	 * @v netdev	Network device
	 *
	 * This method should strip off the link-layer header
	 * (e.g. the Ethernet DIX header) and pass the packet to
	 * net_rx().  This method takes ownership of the packet
	 * buffer.
	 */
	int ( * rx ) ( struct pk_buff *pkb, struct net_device *netdev );
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
	const char * ( * ntoa ) ( const void * ll_addr );
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
	/** List of network devices */
	struct list_head list;
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

	/** Received packet queue */
	struct list_head rx_queue;

	/** Driver private data */
	void *priv;
};

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
 * Get network device name
 *
 * @v netdev		Network device
 * @ret name		Network device name
 *
 * The name will be the device's link-layer address.
 */
static inline const char * netdev_name ( struct net_device *netdev ) {
	return netdev->ll_protocol->ntoa ( netdev->ll_addr );
}

extern int netdev_tx ( struct net_device *netdev, struct pk_buff *pkb );
extern void netdev_rx ( struct net_device *netdev, struct pk_buff *pkb );
extern int net_tx ( struct pk_buff *pkb, struct net_device *netdev,
		    struct net_protocol *net_protocol, const void *ll_dest );
extern int net_rx ( struct pk_buff *pkb, struct net_device *netdev,
		    uint16_t net_proto, const void *ll_source );
extern int netdev_poll ( struct net_device *netdev );
extern struct pk_buff * netdev_rx_dequeue ( struct net_device *netdev );
extern struct net_device * alloc_netdev ( size_t priv_size );
extern int register_netdev ( struct net_device *netdev );
extern void unregister_netdev ( struct net_device *netdev );
extern void free_netdev ( struct net_device *netdev );
extern struct net_device * next_netdev ( void );

#endif /* _GPXE_NETDEVICE_H */
