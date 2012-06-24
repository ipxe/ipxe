#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <ipxe/in.h>
#include <ipxe/ip6.h>
#include <ipxe/if_ether.h>
#include <ipxe/iobuf.h>
#include <ipxe/ndp.h>
#include <ipxe/icmp6.h>
#include <ipxe/tcpip.h>
#include <ipxe/netdevice.h>
#include <ipxe/ethernet.h>

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
	struct ll_option *llopt;
	struct io_buffer *iobuf = alloc_iob ( sizeof ( struct ll_option ) + 
					      sizeof ( *nsolicit ) + MIN_IOB_LEN );
	iob_reserve ( iobuf, MAX_HDR_LEN );
	nsolicit = iob_put ( iobuf, sizeof ( *nsolicit ) );
	llopt = iob_put ( iobuf, sizeof ( *llopt ) );

	/* Fill up the headers */
	memset ( nsolicit, 0, sizeof ( *nsolicit ) );
	nsolicit->type = ICMP6_NSOLICIT;
	nsolicit->code = 0;
	nsolicit->target = *dest;
	
	/* Fill in the link-layer address. FIXME: ll_option assumes 6 bytes. */
	llopt->type = 1;
	llopt->length = ( 2 + ll_protocol->ll_addr_len ) / 8;
	memcpy ( llopt->address, netdev->ll_addr, netdev->ll_protocol->ll_addr_len );
	
	/* Partial checksum */
	nsolicit->csum = 0;
	nsolicit->csum = tcpip_chksum ( nsolicit, sizeof ( *nsolicit ) + sizeof ( *llopt ) );

	/* Solicited multicast address - FF02::1 (all stations on local network) */
	memset(&st_dest.sin6, 0, sizeof(st_dest.sin6));
	st_dest.sin6.sin_family = AF_INET6;
	st_dest.sin6.sin6_addr.in6_u.u6_addr8[0] = 0xff;
	st_dest.sin6.sin6_addr.in6_u.u6_addr8[1] = 0x2;
	st_dest.sin6.sin6_addr.in6_u.u6_addr8[15] = 0x1;

	/* Send packet over IP6 */
	return ipv6_tx ( iobuf, &icmp6_protocol, NULL, &st_dest.st,
			 netdev, &nsolicit->csum );
}

/**
 * Send neighbour advertisement packet
 *
 * @v netdev	Network device
 * @v src	Source address
 * @v dest	Destination address
 *
 * This function prepares a neighbour advertisement packet and sends it to the
 * network layer.
 */
int icmp6_send_advert ( struct net_device *netdev, struct in6_addr *src,
			struct in6_addr *dest ) {
	union {
		struct sockaddr_in6 sin6;
		struct sockaddr_tcpip st;
	} st_dest;
	struct ll_protocol *ll_protocol = netdev->ll_protocol;
	struct neighbour_advert *nadvert;
	struct ll_option *llopt;
	struct io_buffer *iobuf = alloc_iob ( sizeof ( struct ll_option ) + 
					      sizeof ( *nadvert ) + MIN_IOB_LEN );
	iob_reserve ( iobuf, MAX_HDR_LEN );
	nadvert = iob_put ( iobuf, sizeof ( *nadvert ) );
	llopt = iob_put ( iobuf, sizeof ( *llopt ) );

	/* Fill up the headers */
	memset ( nadvert, 0, sizeof ( *nadvert ) );
	nadvert->type = ICMP6_NADVERT;
	nadvert->code = 0;
	nadvert->target = *src;
	nadvert->flags = ICMP6_FLAGS_SOLICITED | ICMP6_FLAGS_OVERRIDE;
	
	/* Fill in the link-layer address. FIXME: ll_option assumes 6 bytes. */
	llopt->type = 2;
	llopt->length = ( 2 + ll_protocol->ll_addr_len ) / 8;
	memcpy ( llopt->address, netdev->ll_addr, netdev->ll_protocol->ll_addr_len );

	/* Partial checksum */
	nadvert->csum = 0;
	nadvert->csum = tcpip_chksum ( nadvert, sizeof ( *nadvert ) + sizeof ( *llopt ) );

	/* Target network address. */
	st_dest.sin6.sin_family = AF_INET6;
	st_dest.sin6.sin6_addr = *dest;

	/* Send packet over IP6 */
	return ipv6_tx ( iobuf, &icmp6_protocol, NULL, &st_dest.st,
			 NULL, &nadvert->csum );
}

/**
 * Process ICMP6 Echo Request
 *
 * @v iobuf I/O buffer containing the original ICMPv6 packet.
 * @v st_src Address of the source station.
 * @v st_dest Address of the destination station.
 */
int icmp6_handle_echo ( struct io_buffer *iobuf, struct sockaddr_tcpip *st_src,
			struct sockaddr_tcpip *st_dest ) {
	struct icmp6_header *icmp6hdr = iobuf->data;
	size_t len = iob_len ( iobuf );
	int rc;

	/* Change type to response and recalculate checksum */
	icmp6hdr->type = ICMP6_ECHO_RESPONSE;
	icmp6hdr->csum = 0;
	icmp6hdr->csum = tcpip_chksum ( icmp6hdr, len );

	/* Transmit the response */
	if ( ( rc = ipv6_tx ( iob_disown ( iobuf ), &icmp6_protocol, st_dest,
			      st_src, NULL, &icmp6hdr->csum ) ) != 0 ) {
		DBG ( "ICMP could not transmit ping response: %s\n",
		      strerror ( rc ) );
	}

	return rc;
}

/**
 * Process ICMP6 headers
 *
 * @v iobuf	I/O buffer
 * @v st_src	Source address
 * @v st_dest	Destination address
 */
int icmp6_rx ( struct io_buffer *iobuf, struct net_device *netdev,
		      struct sockaddr_tcpip *st_src,
		      struct sockaddr_tcpip *st_dest,
		      uint16_t pshdr_csum ) {
	struct icmp6_header *icmp6hdr = iobuf->data;
	size_t len = iob_len ( iobuf );
	unsigned int csum;
	int rc;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *icmp6hdr ) ) {
		DBG ( "Packet too short (%zd bytes)\n", iob_len ( iobuf ) );
		free_iob ( iobuf );
		return -EINVAL;
	}

	/* Verify checksum */
	csum = tcpip_continue_chksum ( pshdr_csum, icmp6hdr, len );
	if ( csum != 0 ) {
		DBG ( "ICMPv6 checksum incorrect (is %04x, should be 0000)\n",
		      csum );
		DBG_HD ( icmp6hdr, len );
		rc = -EINVAL;
		goto done;
	}

	DBG ( "ICMPv6: packet with type %d and code %x\n", icmp6hdr->type, icmp6hdr->code);

	/* Process the ICMP header */
	switch ( icmp6hdr->type ) {
	case ICMP6_ROUTER_ADVERT:
		rc = ndp_process_radvert ( iobuf, netdev, st_src, st_dest );
		break;
	case ICMP6_NSOLICIT:
		rc = ndp_process_nsolicit ( iobuf, netdev, st_src, st_dest );
		break;
	case ICMP6_NADVERT:
		rc = ndp_process_nadvert ( iobuf, st_src, st_dest );
		break;
	case ICMP6_ECHO_REQUEST:
		rc = icmp6_handle_echo ( iobuf, st_src, st_dest );
		break;
	default:
		rc = -ENOSYS;
	}

 done:
	free_iob ( iobuf );
	return rc;
}

#if 0
void icmp6_test_nadvert (struct net_device *netdev, struct sockaddr_in6 *server_p, char *ll_addr) {

		struct sockaddr_in6 server;
		memcpy ( &server, server_p, sizeof ( server ) );
                struct io_buffer *rxiobuf = alloc_iob ( 500 );
                iob_reserve ( rxiobuf, MAX_HDR_LEN );
                struct neighbour_advert *nadvert = iob_put ( rxiobuf, sizeof ( *nadvert ) );
                nadvert->type = 136;
                nadvert->code = 0;
                nadvert->flags = ICMP6_FLAGS_SOLICITED;
		nadvert->csum = 0xffff;
		nadvert->target = server.sin6_addr;
                nadvert->opt_type = 2;
                nadvert->opt_len = 1;
                memcpy ( nadvert->opt_ll_addr, ll_addr, 6 );
                struct ip6_header *ip6hdr = iob_push ( rxiobuf, sizeof ( *ip6hdr ) );
                ip6hdr->ver_traffic_class_flow_label = htonl ( 0x60000000 );
		ip6hdr->hop_limit = 255;
		ip6hdr->nxt_hdr = 58;
		ip6hdr->payload_len = htons ( sizeof ( *nadvert ) );
                ip6hdr->src = server.sin6_addr;
                ip6hdr->dest = server.sin6_addr;
		hex_dump ( rxiobuf->data, iob_len ( rxiobuf ) );
                net_rx ( rxiobuf, netdev, htons ( ETH_P_IPV6 ), ll_addr );
}
#endif

/** ICMP6 protocol */
struct tcpip_protocol icmp6_protocol __tcpip_protocol = {
	.name = "ICMP6",
	.rx = icmp6_rx,
	.tcpip_proto = IP_ICMP6, // 58
};

