#ifndef	_UDP_H
#define	_UDP_H

#include "etherboot.h"
#include "ip.h"

struct udp_pseudo_hdr {
	in_addr  src;
	in_addr  dest;
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
