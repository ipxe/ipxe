#ifndef	_IP_H
#define	_IP_H

#include "stddef.h"
#include "stdint.h"
#include "in.h"

struct iphdr {
	uint8_t  verhdrlen;
	uint8_t  service;
	uint16_t len;
	uint16_t ident;
	uint16_t frags;
	uint8_t  ttl;
	uint8_t  protocol;
	uint16_t chksum;
	struct in_addr src;
	struct in_addr dest;
} PACKED;

extern uint16_t tcpudpchksum(struct iphdr *ip);

#endif	/* _IP_H */
