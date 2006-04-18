#ifndef _NETDEVICE_H
#define _NETDEVICE_H

/** @file
 *
 * Net device interface
 *
 */

#include <stdint.h>
#include <gpxe/llh.h>

struct net_device;
struct pk_buff;

/**
 * A network device
 *
 * Note that this structure must represent a generic network device,
 * not just an Ethernet device.
 */
struct net_device {
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
	 * replace the contents of the packet buffer with an
	 * appropriate ARP request or equivalent, and return -ENOENT.
	 */
	int ( * make_media_header ) ( struct net_device *netdev,
				      struct pk_buff *pkb );
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
	/** Parse link-layer header
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
	 * This is an ARPHRD_XXX constant, in network-byte order.
	 */
	uint16_t ll_proto;
	/** Link-layer address length */
	uint8_t ll_addr_len;
	/** Link-layer address
	 *
	 * For Ethernet, this is the MAC address.
	 */
	uint8_t ll_addr[MAX_LLH_ADDR_LEN];
	/** Driver private data */
	void *priv;
};

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
