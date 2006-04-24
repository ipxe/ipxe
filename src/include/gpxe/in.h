#ifndef	_GPXE_IN_H
#define	_GPXE_IN_H

#include <stdint.h>

#define IP_ICMP		1
#define IP_IGMP		2
#define IP_TCP		6
#define IP_UDP		17

#define INADDR_NONE 0xffffffff

#define INADDR_BROADCAST 0xffffffff

#define IN_MULTICAST(addr) ( ( (addr) & 0xf0000000 ) == 0xe0000000 )

struct in_addr {
	uint32_t	s_addr;
};

typedef struct in_addr in_addr;

typedef uint16_t in_port_t;

struct sockaddr_in {
	struct in_addr	sin_addr;
	in_port_t	sin_port;
};

extern int inet_aton ( const char *cp, struct in_addr *inp );
extern char * inet_ntoa ( struct in_addr in );

#endif	/* _GPXE_IN_H */
