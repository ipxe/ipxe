#ifndef	_UDP_H
#define	_UDP_H

#include "stddef.h"
#include "stdint.h"
#include <gpxe/in.h>
#include "ip.h"

struct udp_pseudo_hdr {
	struct in_addr  src;
	struct in_addr  dest;
	uint8_t  unused;
	uint8_t  protocol;
	uint16_t len;
} PACKED;
struct udphdr {
	uint16_t src;
	uint16_t dest;
	uint16_t len;
	uint16_t chksum;
	struct {} payload;
} PACKED;
struct udppacket {
	struct iphdr	ip;
	struct udphdr	udp;
	struct {} payload;
} PACKED;

#endif	/* _UDP_H */
