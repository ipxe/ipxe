#ifndef	_IN_H
#define	_IN_H

#include <if_ether.h>
#define IP		ETH_P_IP
#define ARP		ETH_P_ARP
#define	RARP		ETH_P_RARP

#define IP_ICMP		1
#define IP_IGMP		2
#define IP_TCP		6
#define IP_UDP		17

/* Same after going through htonl */
#define IP_BROADCAST	0xFFFFFFFF

typedef struct {
	uint32_t	s_addr;
} in_addr;

#endif	/* _IN_H */
