#ifndef _IPXE_NDP_H
#define _IPXE_NDP_H

#include <stdint.h>
#include <byteswap.h>
#include <string.h>
#include <ipxe/icmp6.h>
#include <ipxe/ip6.h>
#include <ipxe/in.h>
#include <ipxe/netdevice.h>
#include <ipxe/iobuf.h>
#include <ipxe/tcpip.h>

#define NDP_STATE_INVALID 0
#define NDP_STATE_INCOMPLETE 1
#define NDP_STATE_REACHABLE 2
#define NDP_STATE_DELAY 3
#define NDP_STATE_PROBE 4
#define NDP_STATE_STALE 5

#define RSOLICIT_STATE_INVALID	0
#define RSOLICIT_STATE_PENDING	1
#define RSOLICIT_STATE_COMPLETE	2
#define RSOLICIT_STATE_ALMOST	3

#define RSOLICIT_CODE_NONE	0
#define RSOLICIT_CODE_MANAGED	1
#define RSOLICIT_CODE_OTHERCONF	2

#define NDP_OPTION_SOURCE_LL        1
#define NDP_OPTION_TARGET_LL        2
#define NDP_OPTION_PREFIX_INFO      3
#define NDP_OPTION_REDIRECT         4
#define NDP_OPTION_MTU              5

struct rsolicit_info {
	struct in6_addr router;
	struct in6_addr prefix;
	int prefix_length;
	int no_address; /* No address assignment takes place via this adv. */
	int flags; /* RSOLICIT_CODE_* flags. */
};

struct neighbour_solicit {
	uint8_t type;
	uint8_t code;
	uint16_t csum;
	uint32_t reserved;
	struct in6_addr target;
};

struct neighbour_advert {
	uint8_t type;
	uint8_t code;
	uint16_t csum;
	uint8_t flags;
	uint8_t reserved;
	struct in6_addr target;
};

struct router_solicit {
	uint8_t type;
	uint8_t code;
	uint16_t csum;
	uint32_t reserved;
};

struct router_advert {
	uint8_t type;
	uint8_t code;
	uint16_t csum;
	uint8_t hop_limit;
	uint8_t rsvd_flags;
	uint16_t lifetime;
	uint32_t reachable_time;
	uint32_t retrans_time;
};

struct ndp_option
{
	uint8_t type;
	uint8_t length;
};

struct ll_option
{
	uint8_t type;
	uint8_t length;
	uint8_t address[6];
};

struct prefix_option
{
	uint8_t type;
	uint8_t length;
	uint8_t prefix_len;
	uint8_t flags_rsvd;
	uint32_t lifetime;
	uint32_t pref_lifetime;
	uint32_t rsvd2;
	uint8_t prefix[16];
};

#define RADVERT_MANAGED		( 1 << 7 )
#define RADVERT_OTHERCONF	( 1 << 6 )

int ndp_resolve ( struct net_device *netdev, struct in6_addr *src,
		  struct in6_addr *dest, void *dest_ll_addr );

int ndp_process_radvert ( struct io_buffer *iobuf, struct net_device *netdev,
			  struct sockaddr_tcpip *st_src,
			  struct sockaddr_tcpip *st_dest );

int ndp_process_nadvert ( struct io_buffer *iobuf, struct sockaddr_tcpip *st_src,
			  struct sockaddr_tcpip *st_dest );

int ndp_process_nsolicit ( struct io_buffer *iobuf, struct net_device *netdev,
			   struct sockaddr_tcpip *st_src,
			   struct sockaddr_tcpip *st_dest );

#endif
