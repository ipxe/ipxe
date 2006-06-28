#ifndef _GPXE_IP_H
#define _GPXE_IP_H

/** @file
 *
 * IP protocol
 *
 */

#include <ip.h>

/* IP constants */

#define IP_VER		4
#define IP_MASK_VER	0xf0
#define IP_MASK_HLEN 	0x0f
#define IP_PSHLEN 	12

/* IP header defaults */
#define IP_TOS		0
#define IP_TTL		64

/* IP4 pseudo header */
struct ipv4_pseudo_header {
	struct in_addr src;
	struct in_addr dest;
	uint8_t zero_padding;
	uint8_t protocol;
	uint16_t len;
};

struct pk_buff;
struct net_device;
struct net_protocol;

extern struct net_protocol ipv4_protocol;

extern int add_ipv4_address ( struct net_device *netdev,
			      struct in_addr address, struct in_addr netmask,
			      struct in_addr gateway );
extern void del_ipv4_address ( struct net_device *netdev );

extern int ipv4_uip_tx ( struct pk_buff *pkb );
extern int ipv4_tx ( struct pk_buff *pkb, uint16_t trans_proto, struct in_addr *dest );

#endif /* _GPXE_IP_H */
