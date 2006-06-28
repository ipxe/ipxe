#ifndef _GPXE_IP6_H
#define _GPXE_IP6_H

/** @file
 *
 * IP6 protocol
 *
 */

#include <ip.h>

/* IP6 constants */

#define IP6_VER		6

/* IP6 header */

struct ip6_header {
	uint32_t 	vers:4,
		 	traffic_class:8,
		 	flow_label:20;
	uint16_t 	payload_len;
	uint8_t 	nxt_hdr;
	uint8_t 	hop_limit;
	struct in6_addr src;
	struct in6_addr dest;
};

struct pk_buff;
struct net_device;
struct net_protocol;

extern struct net_protocol ipv6_protocol;

extern int ipv6_tx ( struct pk_buff *pkb, uint16_t trans_proto, struct in6_addr *dest );

#endif /* _GPXE_IP6_H */
