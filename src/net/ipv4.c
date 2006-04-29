#include <string.h>
#include <stdint.h>
#include <byteswap.h>
#include <vsprintf.h>
#include <gpxe/in.h>


#include <ip.h>


#include <gpxe/if_ether.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>
#include "../proto/uip/uip.h"

/** @file
 *
 * IPv4 protocol
 *
 * The gPXE IP stack is currently implemented on top of the uIP
 * protocol stack.  This file provides wrappers around uIP so that
 * higher-level protocol implementations do not need to talk directly
 * to uIP (which has a somewhat baroque API).
 *
 */

/** An IPv4 routing table entry */
struct ipv4_route {
	/** Network address */
	struct in_addr network;
	/** Subnet mask */
	struct in_addr netmask;
	/** Gateway address */
	struct in_addr gateway;
	/** Gateway device */
	struct in_addr gatewaydev;
};

enum {
	STATIC_SINGLE_NETDEV_ROUTE = 0,
	DEFAULT_ROUTE,
	NUM_ROUTES
};

/** IPv4 routing table */
static struct ipv4_route routing_table[NUM_ROUTES];

#define routing_table_end ( routing_table + NUM_ROUTES )

#if 0
/**
 * Set IP address
 *
 */
void set_ipaddr ( struct in_addr address ) {
	union {
		struct in_addr address;
		uint16_t uip_address[2];
	} u;

	u.address = address;
	uip_sethostaddr ( u.uip_address );
}

/**
 * Set netmask
 *
 */
void set_netmask ( struct in_addr address ) {
	union {
		struct in_addr address;
		uint16_t uip_address[2];
	} u;

	u.address = address;
	uip_setnetmask ( u.uip_address );
}

/**
 * Set default gateway
 *
 */
void set_gateway ( struct in_addr address ) {
	union {
		struct in_addr address;
		uint16_t uip_address[2];
	} u;

	u.address = address;
	uip_setdraddr ( u.uip_address );
}

/**
 * Run the TCP/IP stack
 *
 * Call this function in a loop in order to allow TCP/IP processing to
 * take place.  This call takes the stack through a single iteration;
 * it will typically be used in a loop such as
 *
 * @code
 *
 * struct tcp_connection *my_connection;
 * ...
 * tcp_connect ( my_connection );
 * while ( ! my_connection->finished ) {
 *   run_tcpip();
 * }
 *
 * @endcode
 *
 * where @c my_connection->finished is set by one of the connection's
 * #tcp_operations methods to indicate completion.
 */
void run_tcpip ( void ) {
	void *data;
	size_t len;
	uint16_t type;
	int i;
	
	if ( netdev_poll ( 1, &data, &len ) ) {
		/* We have data */
		memcpy ( uip_buf, data, len );
		uip_len = len;
		type = ntohs ( *( ( uint16_t * ) ( uip_buf + 12 ) ) );
		if ( type == UIP_ETHTYPE_ARP ) {
			uip_arp_arpin();
		} else {
			uip_arp_ipin();
			uip_input();
		}
		if ( uip_len > 0 )
			uip_transmit();
	} else {
		for ( i = 0 ; i < UIP_CONNS ; i++ ) {
			uip_periodic ( i );
			if ( uip_len > 0 )
				uip_transmit();
		}
	}
}
#endif

/**
 * Process incoming IP packets
 *
 * @v pkb		Packet buffer
 * @ret rc		Return status code
 *
 * This handles IP packets by handing them off to the uIP protocol
 * stack.
 */
static int ipv4_rx ( struct pk_buff *pkb ) {

	/* Transfer to uIP buffer.  Horrendously space-inefficient,
	 * but will do as a proof-of-concept for now.
	 */
	uip_len = pkb_len ( pkb );
	memcpy ( uip_buf, pkb->data, uip_len );

	/* Hand to uIP for processing */
	uip_input ();
	if ( uip_len > 0 ) {
		pkb_empty ( pkb );
		pkb_put ( pkb, uip_len );
		memcpy ( pkb->data, uip_buf, uip_len );
		net_transmit ( pkb );
	} else {
		free_pkb ( pkb );
	}
	return 0;
}

/**
 * Perform IP layer routing
 *
 * @v pkb	Packet buffer
 * @ret source	Network-layer source address
 * @ret dest	Network-layer destination address
 * @ret rc	Return status code
 */
static int ipv4_route ( const struct pk_buff *pkb,
			struct net_header *nethdr ) {
	struct iphdr *iphdr = pkb->data;
	struct in_addr *source = ( struct in_addr * ) nethdr->source_net_addr;
	struct in_addr *dest = ( struct in_addr * ) nethdr->dest_net_addr;
	struct ipv4_route *route;

	/* Route IP packet according to routing table */
	source->s_addr = INADDR_NONE;
	dest->s_addr = iphdr->dest.s_addr;
	for ( route = routing_table ; route < routing_table_end ; route++ ) {
		if ( ( dest->s_addr & route->netmask.s_addr )
		     == route->network.s_addr ) {
			source->s_addr = route->gatewaydev.s_addr;
			if ( route->gateway.s_addr )
				dest->s_addr = route->gateway.s_addr;
			break;
		}
	}

	/* Set broadcast and multicast flags as applicable */
	nethdr->flags = 0;
	if ( dest->s_addr == htonl ( INADDR_BROADCAST ) ) {
		nethdr->flags = PKT_FL_BROADCAST;
	} else if ( IN_MULTICAST ( dest->s_addr ) ) {
		nethdr->flags = PKT_FL_MULTICAST;
	}

	return 0;
}

/**
 * Transcribe IP address
 *
 * @v net_addr	IP address
 * @ret string	IP address in dotted-quad notation
 *
 */
static const char * ipv4_ntoa ( const void *net_addr ) {
	static char buf[16]; /* "xxx.xxx.xxx.xxx" */
	uint8_t *ip_addr = net_addr;

	sprintf ( buf, "%d.%d.%d.%d", ip_addr[0], ip_addr[1], ip_addr[2],
		  ip_addr[3] );
	return buf;
}

/** IPv4 protocol */
struct net_protocol ipv4_protocol = {
	.name = "IP",
	.net_proto = htons ( ETH_P_IP ),
	.net_addr_len = sizeof ( struct in_addr ),
	.rx_process = ipv4_rx,
	.route = ipv4_route,
	.ntoa = ipv4_ntoa,
};

NET_PROTOCOL ( ipv4_protocol );

/** IPv4 address for the static single net device */
struct net_address static_single_ipv4_address = {
	.net_protocol = &ipv4_protocol,

#warning "Remove this static-IP hack"
	.net_addr = { 0x0a, 0xfe, 0xfe, 0x01 },
};

STATIC_SINGLE_NETDEV_ADDRESS ( static_single_ipv4_address );

#warning "Remove this static-IP hack"
static struct ipv4_route routing_table[NUM_ROUTES] = {
	{ { htonl ( 0x0afefe00 ) }, { htonl ( 0xfffffffc ) },
	  { htonl ( 0x00000000 ) }, { htonl ( 0x0afefe01 ) } },
	{ { htonl ( 0x00000000 ) }, { htonl ( 0x00000000 ) },
	  { htonl ( 0x0afefe02 ) }, { htonl ( 0x0afefe01 ) } },
};

