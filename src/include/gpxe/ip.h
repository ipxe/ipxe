#ifndef _GPXE_IP_H
#define _GPXE_IP_H

/** @file
 *
 * IP protocol
 *
 */

#include <ip.h>

struct net_protocol;

extern struct net_protocol ipv4_protocol;

extern int add_ipv4_address ( struct net_device *netdev,
			      struct in_addr address, struct in_addr netmask,
			      struct in_addr gateway );
extern void del_ipv4_address ( struct net_device *netdev );
extern int ipv4_uip_tx ( struct pk_buff *pkb );

#endif /* _GPXE_IP_H */
