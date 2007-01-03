#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <gpxe/in.h>
#include <gpxe/ip6.h>
#include <gpxe/if_ether.h>
#include <gpxe/pkbuff.h>
#include <gpxe/ndp.h>
#include <gpxe/icmp6.h>
#include <gpxe/tcpip.h>
#include <gpxe/netdevice.h>

struct tcpip_protocol icmp6_protocol;

/**
 * Send neighbour solicitation packet
 *
 * @v netdev	Network device
 * @v src	Source address
 * @v dest	Destination address
 *
 * This function prepares a neighbour solicitation packet and sends it to the
 * network layer.
 */
int icmp6_send_solicit ( struct net_device *netdev, struct in6_addr *src __unused,
			 struct in6_addr *dest ) {
	union {
		struct sockaddr_in6 sin6;
		struct sockaddr_tcpip st;
	} st_dest;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct neighbour_solicit *nsolicit;
	struct pk_buff *pkb = alloc_pkb ( sizeof ( *nsolicit ) + MIN_PKB_LEN );
	pkb_reserve ( pkb, MAX_HDR_LEN );
	nsolicit = pkb_put ( pkb, sizeof ( *nsolicit ) );

	/* Fill up the headers */
	memset ( nsolicit, 0, sizeof ( *nsolicit ) );
	nsolicit->type = ICMP6_NSOLICIT;
	nsolicit->code = 0;
	nsolicit->target = *dest;
	nsolicit->opt_type = 1;
	nsolicit->opt_len = ( 2 + ll_protocol->ll_addr_len ) / 8;
	memcpy ( nsolicit->opt_ll_addr, netdev->ll_addr,
				netdev->ll_protocol->ll_addr_len );
	/* Partial checksum */
	nsolicit->csum = 0;
	nsolicit->csum = tcpip_chksum ( nsolicit, sizeof ( *nsolicit ) );

	/* Solicited multicast address */
	st_dest.sin6.sin_family = AF_INET6;
	st_dest.sin6.sin6_addr.in6_u.u6_addr8[0] = 0xff;
	st_dest.sin6.sin6_addr.in6_u.u6_addr8[2] = 0x02;
	st_dest.sin6.sin6_addr.in6_u.u6_addr16[1] = 0x0000;
	st_dest.sin6.sin6_addr.in6_u.u6_addr32[1] = 0x00000000;
	st_dest.sin6.sin6_addr.in6_u.u6_addr16[4] = 0x0000;
	st_dest.sin6.sin6_addr.in6_u.u6_addr16[5] = 0x0001;
	st_dest.sin6.sin6_addr.in6_u.u6_addr32[3] = dest->in6_u.u6_addr32[3];
	st_dest.sin6.sin6_addr.in6_u.u6_addr8[13] = 0xff;
	
	/* Send packet over IP6 */
	return tcpip_tx ( pkb, &icmp6_protocol, &st_dest.st, &nsolicit->csum );
}

/**
 * Process ICMP6 headers
 *
 * @v pkb	Packet buffer
 * @v st_src	Source address
 * @v st_dest	Destination address
 */
static int icmp6_rx ( struct pk_buff *pkb, struct sockaddr_tcpip *st_src,
		      struct sockaddr_tcpip *st_dest ) {
	struct icmp6_header *icmp6hdr = pkb->data;

	/* Sanity check */
	if ( pkb_len ( pkb ) < sizeof ( *icmp6hdr ) ) {
		DBG ( "Packet too short (%d bytes)\n", pkb_len ( pkb ) );
		free_pkb ( pkb );
		return -EINVAL;
	}

	/* TODO: Verify checksum */

	/* Process the ICMP header */
	switch ( icmp6hdr->type ) {
	case ICMP6_NADVERT:
		return ndp_process_advert ( pkb, st_src, st_dest );
	}
	return -ENOSYS;
}

#if 0
void icmp6_test_nadvert (struct net_device *netdev, struct sockaddr_in6 *server_p, char *ll_addr) {

		struct sockaddr_in6 server;
		memcpy ( &server, server_p, sizeof ( server ) );
                struct pk_buff *rxpkb = alloc_pkb ( 500 );
                pkb_reserve ( rxpkb, MAX_HDR_LEN );
                struct neighbour_advert *nadvert = pkb_put ( rxpkb, sizeof ( *nadvert ) );
                nadvert->type = 136;
                nadvert->code = 0;
                nadvert->flags = ICMP6_FLAGS_SOLICITED;
		nadvert->csum = 0xffff;
		nadvert->target = server.sin6_addr;
                nadvert->opt_type = 2;
                nadvert->opt_len = 1;
                memcpy ( nadvert->opt_ll_addr, ll_addr, 6 );
                struct ip6_header *ip6hdr = pkb_push ( rxpkb, sizeof ( *ip6hdr ) );
                ip6hdr->ver_traffic_class_flow_label = htonl ( 0x60000000 );
		ip6hdr->hop_limit = 255;
		ip6hdr->nxt_hdr = 58;
		ip6hdr->payload_len = htons ( sizeof ( *nadvert ) );
                ip6hdr->src = server.sin6_addr;
                ip6hdr->dest = server.sin6_addr;
		hex_dump ( rxpkb->data, pkb_len ( rxpkb ) );
                net_rx ( rxpkb, netdev, htons ( ETH_P_IPV6 ), ll_addr );
}
#endif

/** ICMP6 protocol */
struct tcpip_protocol icmp6_protocol __tcpip_protocol = {
	.name = "ICMP6",
	.rx = icmp6_rx,
	.tcpip_proto = IP_ICMP6, // 58
};
