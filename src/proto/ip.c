#include <string.h>
#include <stdint.h>
#include <byteswap.h>
#include <gpxe/in.h>
#include <gpxe/ip.h>
#include "uip/uip.h"
#include "uip/uip_arp.h"

/** @file
 *
 * IP protocol
 *
 * The gPXE IP stack is currently implemented on top of the uIP
 * protocol stack.  This file provides wrappers around uIP so that
 * higher-level protocol implementations do not need to talk directly
 * to uIP (which has a somewhat baroque API).
 *
 */

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
 * Initialise TCP/IP stack
 *
 */
void init_tcpip ( void ) {
	uip_init();
	uip_arp_init();
}

#define UIP_HLEN ( 40 + UIP_LLH_LEN )

/**
 * Transmit TCP data
 *
 * This is a wrapper around netdev_transmit().  It gathers up the
 * packet produced by uIP, and then passes it to netdev_transmit() as
 * a single buffer.
 */
static void uip_transmit ( void ) {
	uip_arp_out();
	if ( uip_len > UIP_HLEN ) {
		memcpy ( uip_buf + UIP_HLEN, ( void * ) uip_appdata,
			 uip_len - UIP_HLEN );
	}
	netdev_transmit ( uip_buf, uip_len );
	uip_len = 0;
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
