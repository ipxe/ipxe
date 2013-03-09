#ifndef _IPXE_IP6_H
#define _IPXE_IP6_H

/** @file
 *
 * IP6 protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <ipxe/in.h>
#include <ipxe/netdevice.h>
#include <ipxe/tcpip.h>
#include <ipxe/retry.h>

/* IP6 constants */

#define IP6_VERSION	0x6
#define IP6_HOP_LIMIT	255

#define IP6_FRAG_IOB_SIZE	2000
#define IP6_FRAG_TIMEOUT	50

#define IP6_MORE_FRAGMENTS	0x01

/**
 * I/O buffer contents
 * This is duplicated in tcp.h and here. Ideally it should go into iobuf.h
 */
#define MAX_HDR_LEN	100
#define MAX_IOB_LEN	1500
#define MIN_IOB_LEN	MAX_HDR_LEN + 100 /* To account for padding by LL */

#define IP6_EQUAL( in6_addr1, in6_addr2 ) \
        ( memcmp ( ( char* ) &( in6_addr1 ), ( char* ) &( in6_addr2 ),\
	sizeof ( struct in6_addr ) ) == 0 )

#define IS_UNSPECIFIED( addr ) \
	( ( (addr).in6_u.u6_addr32[0] == 0x00000000 ) && \
	( (addr).in6_u.u6_addr32[1] == 0x00000000 ) && \
	( (addr).in6_u.u6_addr32[2] == 0x00000000 ) && \
	( (addr).in6_u.u6_addr32[3] == 0x00000000 ) )
/* IP6 header */
struct ip6_header {
	uint32_t 	ver_traffic_class_flow_label;
	uint16_t 	payload_len;
	uint8_t 	nxt_hdr;
	uint8_t 	hop_limit;
	struct in6_addr src;
	struct in6_addr dest;
};

/* IP6 pseudo header */
struct ipv6_pseudo_header {
	struct in6_addr src;
	struct in6_addr dest;
	uint8_t zero_padding;
	uint8_t nxt_hdr;
	uint16_t len;
};

/* IP6 option header */
struct ip6_opt_hdr {
	uint8_t type;
	uint8_t len;
};

/* IP6 fragment header */
struct ip6_frag_hdr {
	uint8_t next_hdr;
	uint8_t rsvd;
	uint16_t offset_flags;
	uint32_t ident;
};

/* Fragment reassembly buffer */
struct frag_buffer {
	/* "Next Header" for the packet. */
	uint8_t next_hdr;
	/* Identification number */
	uint32_t ident;
	/* Source network address */
	struct in6_addr src;
	/* Destination network address */
	struct in6_addr dest;
	/* Reassembled I/O buffer */
	struct io_buffer *frag_iob;
	/* Reassembly timer */
	struct retry_timer frag_timer;
	/* List of fragment reassembly buffers */
	struct list_head list;
};

/* Next header numbers */
#define IP6_HOPBYHOP_FIRST	0x00
#define IP6_HOPBYHOP		0xFE
#define IP6_PAD			0x00
#define IP6_PADN		0x01
#define IP6_ICMP6		0x3A
#define IP6_ROUTING 		0x2B
#define IP6_FRAGMENT		0x2C
#define IP6_AUTHENTICATION	0x33
#define IP6_DEST_OPTS		0x3C
#define IP6_ESP			0x32
#define IP6_NO_HEADER		0x3B

/** An IPv6 routing table entry */
struct ipv6_miniroute {
	/* List of miniroutes */
	struct list_head list;

	/* Network device */
	struct net_device *netdev;

	/* Destination prefix */
	struct in6_addr prefix;
	/* Prefix length */
	int prefix_len;
	/* IPv6 address of interface */
	struct in6_addr address;
	/* Gateway address */
	struct in6_addr gateway;
};

struct io_buffer;

extern struct list_head ipv6_miniroutes;

extern struct net_protocol ipv6_protocol __net_protocol;
extern struct tcpip_net_protocol ipv6_tcpip_protocol __tcpip_net_protocol;
extern char * inet6_ntoa ( struct in6_addr in6 );
extern int inet6_aton ( const char *cp, struct in6_addr *inp );

void ipv6_generate_eui64 ( uint8_t *out, uint8_t *ll );

int ipv6_match_prefix ( struct in6_addr *p1, struct in6_addr *p2, size_t len );

extern int add_ipv6_address ( struct net_device *netdev,
			      struct in6_addr prefix, int prefix_len,
			      struct in6_addr address,
			      struct in6_addr gateway );
extern void del_ipv6_address ( struct net_device *netdev );

extern int ipv6_tx ( struct io_buffer *iobuf,
		      struct tcpip_protocol *tcpip,
		      struct sockaddr_tcpip *st_src __unused,
		      struct sockaddr_tcpip *st_dest,
		      struct net_device *netdev,
		      uint16_t *trans_csum );

#endif /* _IPXE_IP6_H */
