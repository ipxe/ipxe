#ifndef	_GPXE_IN_H
#define	_GPXE_IN_H

#include <stdint.h>

/* Protocol numbers */

#define IP_ICMP		1
#define IP_IGMP		2
#define IP_TCP		6
#define IP_UDP		17

/* Network address family numbers */

#define AF_INET		1
#define AF_INET6	2
#define AF_802          6
#define AF_IPX          11

typedef uint16_t sa_family_t;

/* IP address constants */

#define INADDR_NONE 0xffffffff

#define INADDR_BROADCAST 0xffffffff

#define IN_MULTICAST(addr) ( ( (addr) & 0xf0000000 ) == 0xe0000000 )

/**
 * IP address structure
 */
struct in_addr {
	uint32_t	s_addr;
};

typedef struct in_addr in_addr;

/**
 * IP6 address structure
 */
struct in6_addr {
        union {
                uint8_t u6_addr8[16];
                uint16_t u6_addr16[8];
                uint32_t u6_addr32[4];
        } in16_u;
#define s6_addr         in6_u.u6_addr8
#define s6_addr16       in6_u.u6_addr16
#define s6_addr32       in6_u.u6_addr32
};

typedef uint16_t in_port_t;

/**
 * IP socket address
 */
struct sockaddr_in {
	struct in_addr	sin_addr;
	in_port_t	sin_port;
};

/**
 * IPv6 socket address
 */
struct sockaddr_in6 {
        in_port_t       sin6_port;      /* Destination port */
        uint32_t        sin6_flowinfo;  /* Flow number */
        struct in6_addr sin6_addr;      /* 128-bit destination address */
        uint32_t        sin6_scope_id;  /* Scope ID */
};

/**
 * Generalized socket address structure
 */
struct sockaddr {
        sa_family_t             sa_family;      /* Socket address family */
        struct sockaddr_in      sin;            /* IP4 socket address */
        struct sockaddr_in6     sin6;           /* IP6 socket address */
};

extern int inet_aton ( const char *cp, struct in_addr *inp );
extern char * inet_ntoa ( struct in_addr in );

/* Adding the following for IP6 support
 *

extern int inet6_aton ( const char *cp, struct in6_addr *inp );
extern char * inet6_ntoa ( struct in_addr in );

 */

#endif	/* _GPXE_IN_H */
