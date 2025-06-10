/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * IPv4 tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/in.h>
#include <ipxe/ip.h>
#include <ipxe/test.h>
#include "netdev_test.h"

/** Define inline IPv4 address */
#define IPV4(a,b,c,d) \
	htonl ( ( (a) << 24 ) | ( (b) << 16 ) | ( (c) << 8 ) | (d) )

/**
 * Report an inet_ntoa() test result
 *
 * @v addr		IPv4 address
 * @v text		Expected textual representation
 * @v file		Test code file
 * @v line		Test code line
 */
static void inet_ntoa_okx ( uint32_t addr, const char *text, const char *file,
			    unsigned int line ) {
	struct in_addr in = { .s_addr = addr };
	char *actual;

	/* Format address */
	actual = inet_ntoa ( in );
	DBG ( "inet_ntoa ( %d.%d.%d.%d ) = %s\n",
	      ( ( ntohl ( addr ) >> 24 ) & 0xff ),
	      ( ( ntohl ( addr ) >> 16 ) & 0xff ),
	      ( ( ntohl ( addr ) >> 8 ) & 0xff ),
	      ( ( ntohl ( addr ) >> 0 ) & 0xff ), actual );
	okx ( strcmp ( actual, text ) == 0, file, line );
}
#define inet_ntoa_ok( addr, text ) \
	inet_ntoa_okx ( addr, text, __FILE__, __LINE__ )

/**
 * Report an inet_aton() test result
 *
 * @v text		Textual representation
 * @v addr		Expected IPv4 address
 * @v file		Test code file
 * @v line		Test code line
 */
static void inet_aton_okx ( const char *text, uint32_t addr, const char *file,
			    unsigned int line ) {
	struct in_addr actual;

	/* Parse address */
	okx ( inet_aton ( text, &actual ) != 0, file, line );
	DBG ( "inet_aton ( \"%s\" ) = %s\n", text, inet_ntoa ( actual ) );
	okx ( actual.s_addr == addr, file, line );
};
#define inet_aton_ok( text, addr ) \
	inet_aton_okx ( text, addr, __FILE__, __LINE__ )

/**
 * Report an inet_aton() failure test result
 *
 * @v text		Textual representation
 * @v file		Test code file
 * @v line		Test code line
 */
static void inet_aton_fail_okx ( const char *text, const char *file,
				 unsigned int line ) {
	struct in_addr actual;

	/* Attempt to parse address */
	okx ( inet_aton ( text, &actual ) == 0, file, line );
}
#define inet_aton_fail_ok( text ) \
	inet_aton_fail_okx ( text, __FILE__, __LINE__ )

/**
 * Report an ipv4_route() test result
 *
 * @v dest		Destination address
 * @v scope		Destination scope test network device, or NULL
 * @v next		Expected next hop address (on success)
 * @v egress		Expected egress device, or NULL to expect failure
 * @v src		Expected source address (on success)
 * @v bcast		Expected broadcast packet (on success)
 * @v file		Test code file
 * @v line		Test code line
 */
static void ipv4_route_okx ( const char *dest, struct testnet *scope,
			     const char *next, struct testnet *egress,
			     const char *src, int bcast,
			     const char *file, unsigned int line ) {
	struct ipv4_miniroute *miniroute;
	struct in_addr in_dest;
	struct in_addr in_src;
	struct in_addr in_next;
	struct in_addr actual;
	unsigned int scope_id;

	/* Sanity checks */
	assert ( ( scope == NULL ) || ( scope->netdev != NULL ) );
	assert ( ( egress == NULL ) == ( src == NULL ) );

	/* Parse addresses */
	okx ( inet_aton ( dest, &in_dest ) != 0, file, line );
	if ( src )
		okx ( inet_aton ( src, &in_src ) != 0, file, line );
	if ( next ) {
		okx ( inet_aton ( next, &in_next ) != 0, file, line );
	} else {
		in_next.s_addr = in_dest.s_addr;
	}

	/* Perform routing */
	actual.s_addr = in_dest.s_addr;
	scope_id = ( scope ? scope->netdev->scope_id : 0 );
	miniroute = ipv4_route ( scope_id, &actual );

	/* Validate result */
	if ( src ) {

		/* Check that a route was found */
		okx ( miniroute != NULL, file, line );
		DBG ( "ipv4_route ( %s, %s ) = %s",
		      ( scope ? scope->dev.name : "<any>" ), dest,
		      inet_ntoa ( actual ) );
		DBG ( " from %s via %s\n",
		      inet_ntoa ( miniroute->address ), egress->dev.name );

		/* Check that expected network device was used */
		okx ( miniroute->netdev == egress->netdev, file, line );

		/* Check that expected source address was used */
		okx ( miniroute->address.s_addr == in_src.s_addr, file, line );

		/* Check that expected next hop address was used */
		okx ( actual.s_addr == in_next.s_addr, file, line );

		/* Check that expected broadcast choice was used */
		okx ( ( ! ( ( ~actual.s_addr ) & miniroute->hostmask.s_addr ) )
		      == ( !! bcast ), file, line );

	} else {

		/* Routing is expected to fail */
		okx ( miniroute == NULL, file, line );
		DBG ( "ipv4_route ( %s, %s ) = <unreachable>\n",
		      ( scope ? scope->dev.name : "<any>" ), dest );
	}
}
#define ipv4_route_ok( dest, scope, next, egress, src, bcast )		\
	ipv4_route_okx ( dest, scope, next, egress, src, bcast,		\
			 __FILE__, __LINE__ )

/** net0: Single address and gateway (DHCP assignment) */
TESTNET ( net0,
	  { "dhcp/ip", "192.168.0.1" },
	  { "dhcp/netmask", "255.255.255.0" },
	  { "dhcp/gateway", "192.168.0.254" } );

/** net1: Single address and gateway (DHCP assignment) */
TESTNET ( net1,
	  { "dhcp/ip", "192.168.0.2" },
	  { "dhcp/netmask", "255.255.255.0" },
	  { "dhcp/gateway", "192.168.0.254" } );

/** net2: Small /31 subnet mask */
TESTNET ( net2,
	  { "ip", "10.31.31.0" },
	  { "netmask", "255.255.255.254" },
	  { "gateway", "10.31.31.1" } );

/** net3: Small /32 subnet mask */
TESTNET ( net3,
	  { "ip", "10.32.32.32" },
	  { "netmask", "255.255.255.255" },
	  { "gateway", "192.168.32.254" } );

/** net4: Local subnet with no gateway */
TESTNET ( net4,
	  { "ip", "192.168.86.1" },
	  { "netmask", "255.255.240.0" } );

/** net5: Static routes */
TESTNET ( net5,
	  { "ip", "10.42.0.1" },
	  { "netmask", "255.255.0.0" },
	  { "gateway", "10.42.0.254" /* should be ignored */ },
	  { "static-routes",
	    "19:0a:2b:2b:80:0a:2a:2b:2b:" /* 10.43.43.128/25 via 10.42.43.43 */
	    "10:c0:a8:0a:2a:c0:a8:" /* 192.168.0.0/16 via 10.42.192.168 */
	    "18:c0:a8:00:00:00:00:00:" /* 192.168.0.0/24 on-link */
	    "00:0a:2a:01:01" /* default via 10.42.1.1 */ } );

/**
 * Perform IPv4 self-tests
 *
 */
static void ipv4_test_exec ( void ) {

	/* Address testing macros */
	ok (   IN_IS_CLASSA ( IPV4 ( 10, 0, 0, 1 ) ) );
	ok ( ! IN_IS_CLASSB ( IPV4 ( 10, 0, 0, 1 ) ) );
	ok ( ! IN_IS_CLASSC ( IPV4 ( 10, 0, 0, 1 ) ) );
	ok ( ! IN_IS_CLASSA ( IPV4 ( 172, 16, 0, 1 ) ) );
	ok (   IN_IS_CLASSB ( IPV4 ( 172, 16, 0, 1 ) ) );
	ok ( ! IN_IS_CLASSC ( IPV4 ( 172, 16, 0, 1 ) ) );
	ok ( ! IN_IS_CLASSA ( IPV4 ( 192, 168, 0, 1 ) ) );
	ok ( ! IN_IS_CLASSB ( IPV4 ( 192, 168, 0, 1 ) ) );
	ok (   IN_IS_CLASSC ( IPV4 ( 192, 168, 0, 1 ) ) );
	ok ( ! IN_IS_MULTICAST ( IPV4 ( 127, 0, 0, 1 ) ) );
	ok ( ! IN_IS_MULTICAST ( IPV4 ( 8, 8, 8, 8 ) ) );
	ok ( ! IN_IS_MULTICAST ( IPV4 ( 0, 0, 0, 0 ) ) );
	ok ( ! IN_IS_MULTICAST ( IPV4 ( 223, 0, 0, 1 ) ) );
	ok ( ! IN_IS_MULTICAST ( IPV4 ( 240, 0, 0, 1 ) ) );
	ok (   IN_IS_MULTICAST ( IPV4 ( 224, 0, 0, 1 ) ) );
	ok (   IN_IS_MULTICAST ( IPV4 ( 231, 89, 0, 2 ) ) );
	ok (   IN_IS_MULTICAST ( IPV4 ( 239, 6, 1, 17 ) ) );

	/* inet_ntoa() tests */
	inet_ntoa_ok ( IPV4 ( 127, 0, 0, 1 ), "127.0.0.1" );
	inet_ntoa_ok ( IPV4 ( 0, 0, 0, 0 ), "0.0.0.0" );
	inet_ntoa_ok ( IPV4 ( 255, 255, 255, 255 ), "255.255.255.255" );
	inet_ntoa_ok ( IPV4 ( 212, 13, 204, 60 ), "212.13.204.60" );

	/* inet_aton() tests */
	inet_aton_ok ( "212.13.204.60", IPV4 ( 212, 13, 204, 60 ) );
	inet_aton_ok ( "127.0.0.1", IPV4 ( 127, 0, 0, 1 ) );

	/* inet_aton() failure tests */
	inet_aton_fail_ok ( "256.0.0.1" ); /* Byte out of range */
	inet_aton_fail_ok ( "212.13.204.60.1" ); /* Too long */
	inet_aton_fail_ok ( "127.0.0" ); /* Too short */
	inet_aton_fail_ok ( "1.2.3.a" ); /* Invalid characters */
	inet_aton_fail_ok ( "127.0..1" ); /* Missing bytes */

	/* Single address and gateway */
	testnet_ok ( &net0 );
	ipv4_route_ok ( "192.168.0.10", NULL,
			"192.168.0.10", &net0, "192.168.0.1", 0 );
	ipv4_route_ok ( "10.0.0.6", NULL,
			"192.168.0.254", &net0, "192.168.0.1", 0 );
	ipv4_route_ok ( "192.168.0.255", NULL,
			"192.168.0.255", &net0, "192.168.0.1", 1 );
	testnet_remove_ok ( &net0 );

	/* Overridden DHCP-assigned address */
	testnet_ok ( &net1 );
	ipv4_route_ok ( "192.168.1.3", NULL,
			"192.168.0.254", &net1, "192.168.0.2", 0 );
	testnet_set_ok ( &net1, "ip", "192.168.1.2" );
	ipv4_route_ok ( "192.168.1.3", NULL,
			"192.168.1.3", &net1, "192.168.1.2", 0 );
	testnet_remove_ok ( &net1 );

	/* Small /31 subnet */
	testnet_ok ( &net2 );
	ipv4_route_ok ( "10.31.31.1", NULL,
			"10.31.31.1", &net2, "10.31.31.0", 0 );
	ipv4_route_ok ( "212.13.204.60", NULL,
			"10.31.31.1", &net2, "10.31.31.0", 0 );
	testnet_remove_ok ( &net2 );

	/* Small /32 subnet */
	testnet_ok ( &net3 );
	ipv4_route_ok ( "10.32.32.31", NULL,
			"192.168.32.254", &net3, "10.32.32.32", 0 );
	ipv4_route_ok ( "8.8.8.8", NULL,
			"192.168.32.254", &net3, "10.32.32.32", 0 );
	testnet_remove_ok ( &net3 );

	/* No gateway */
	testnet_ok ( &net4 );
	ipv4_route_ok ( "192.168.87.1", NULL,
			"192.168.87.1", &net4, "192.168.86.1", 0 );
	ipv4_route_ok ( "192.168.96.1", NULL, NULL, NULL, NULL, 0 );
	testnet_remove_ok ( &net4 );

	/* Multiple interfaces */
	testnet_ok ( &net0 );
	testnet_ok ( &net1 );
	testnet_ok ( &net2 );
	testnet_close_ok ( &net1 );
	ipv4_route_ok ( "192.168.0.9", NULL,
			"192.168.0.9", &net0, "192.168.0.1", 0 );
	ipv4_route_ok ( "10.31.31.1", NULL,
			"10.31.31.1", &net2, "10.31.31.0", 0 );
	testnet_close_ok ( &net0 );
	testnet_open_ok ( &net1 );
	ipv4_route_ok ( "192.168.0.9", NULL,
			"192.168.0.9", &net1, "192.168.0.2", 0 );
	ipv4_route_ok ( "10.31.31.1", NULL,
			"10.31.31.1", &net2, "10.31.31.0", 0 );
	testnet_close_ok ( &net2 );
	ipv4_route_ok ( "8.8.8.8", NULL,
			"192.168.0.254", &net1, "192.168.0.2", 0 );
	testnet_close_ok ( &net1 );
	testnet_open_ok ( &net0 );
	ipv4_route_ok ( "8.8.8.8", NULL,
			"192.168.0.254", &net0, "192.168.0.1", 0 );
	testnet_close_ok ( &net0 );
	testnet_open_ok ( &net2 );
	ipv4_route_ok ( "8.8.8.8", NULL,
			"10.31.31.1", &net2, "10.31.31.0", 0 );
	testnet_remove_ok ( &net2 );
	testnet_remove_ok ( &net1 );
	testnet_remove_ok ( &net0 );

	/* Static routes */
	testnet_ok ( &net5 );
	ipv4_route_ok ( "10.42.99.0", NULL,
			"10.42.99.0", &net5, "10.42.0.1", 0 );
	ipv4_route_ok ( "8.8.8.8", NULL,
			"10.42.1.1", &net5, "10.42.0.1", 0 );
	ipv4_route_ok ( "10.43.43.1", NULL,
			"10.42.1.1", &net5, "10.42.0.1", 0 );
	ipv4_route_ok ( "10.43.43.129", NULL,
			"10.42.43.43", &net5, "10.42.0.1", 0 );
	ipv4_route_ok ( "192.168.54.8", NULL,
			"10.42.192.168", &net5, "10.42.0.1", 0 );
	ipv4_route_ok ( "192.168.0.8", NULL,
			"192.168.0.8", &net5, "10.42.0.1", 0 );
	ipv4_route_ok ( "192.168.0.255", NULL,
			"192.168.0.255", &net5, "10.42.0.1", 1 );
	testnet_remove_ok ( &net5 );
}

/** IPv4 self-test */
struct self_test ipv4_test __self_test = {
	.name = "ipv4",
	.exec = ipv4_test_exec,
};
