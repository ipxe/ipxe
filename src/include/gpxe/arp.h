#ifndef _ARP_H
#define _ARP_H

/** @file
 *
 * Address Resolution Protocol
 *
 */

struct net_device;
struct net_interface;
struct pk_buff;

extern int arp_resolve ( struct net_device *netdev, struct pk_buff *pkb,
			 void *ll_addr );

extern int arp_process ( struct net_interface *arp_netif,
			 struct pk_buff *pkb );

#endif /* _ARP_H */
