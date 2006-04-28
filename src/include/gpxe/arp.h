#ifndef _GPXE_ARP_H
#define _GPXE_ARP_H

/** @file
 *
 * Address Resolution Protocol
 *
 */

struct net_device;
struct net_header;
struct ll_header;

extern int arp_resolve ( struct net_device *netdev,
			 const struct net_header *nethdr,
			 struct ll_header *llhdr );

#endif /* _GPXE_ARP_H */
