#ifndef _GPXE_ARP_H
#define _GPXE_ARP_H

/** @file
 *
 * Address Resolution Protocol
 *
 */

struct net_device;
struct net_protocol;

extern int arp_resolve ( struct net_device *netdev,
			 struct net_protocol *net_protocol,
			 const void *dest_net_addr,
			 const void *source_net_addr,
			 void *dest_ll_addr );

#endif /* _GPXE_ARP_H */
