#ifndef _IPXE_ICMP6_H
#define _IPXE_ICMP6_H

/** @file
 *
 * ICMP6 protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/ip6.h>
#include <ipxe/ndp.h>

#define ICMP6_ECHO_REQUEST	128
#define ICMP6_ECHO_RESPONSE	129
#define ICMP6_ROUTER_SOLICIT	133
#define ICMP6_ROUTER_ADVERT	134
#define ICMP6_NSOLICIT		135
#define ICMP6_NADVERT		136

extern struct tcpip_protocol icmp6_protocol __tcpip_protocol;

struct icmp6_header {
	uint8_t type;
	uint8_t code;
	uint16_t csum;
	/* Message body */
};

struct ra_msg {
	uint8_t type;
	uint8_t code;
	uint16_t csum;
	uint8_t reserved;
	uint8_t flags;
	uint16_t lifetime;
  /* FIXME:  hack alert */
	uint32_t reachable_time;
	uint32_t retrans_timer;
};

struct nd_opt_hdr {
	uint8_t		nd_opt_type;
	uint8_t		nd_opt_len;
} __packed;

#define ICMP6_FLAGS_ROUTER 0x80
#define ICMP6_FLAGS_SOLICITED 0x40
#define ICMP6_FLAGS_OVERRIDE 0x20

int icmp6_rx ( struct io_buffer *iobuf, struct net_device *netdev,
		      struct sockaddr_tcpip *st_src,
		      struct sockaddr_tcpip *st_dest,
		      uint16_t pshdr_csum );

int icmp6_send_solicit ( struct net_device *netdev, struct in6_addr *src, struct in6_addr *dest );

int icmp6_send_advert ( struct net_device *netdev, struct in6_addr *src, struct in6_addr *dest );

#endif /* _IPXE_ICMP6_H */
