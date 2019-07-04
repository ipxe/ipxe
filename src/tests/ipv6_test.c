/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
 * IPv6 tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/ipv6.h>
#include <ipxe/test.h>

/** Define inline IPv6 address */
#define IPV6(...) { __VA_ARGS__ }

/** An IPv6 test routing table entry */
struct ipv6_test_route {
	/** Local address */
	const char *address;
	/** Prefix length */
	unsigned int prefix_len;
	/** Router address (if any) */
	const char *router;
};

/** An IPv6 test routing table */
struct ipv6_test_table {
	/** Test routing table entries */
	const struct ipv6_test_route *routes;
	/** Number of table entries */
	unsigned int count;
	/** Constructed routing table */
	struct list_head list;
};

/** Define a test routing table */
#define TABLE( name, ... )						\
	static const struct ipv6_test_route name ## _routes[] = {	\
		__VA_ARGS__						\
	};								\
	static struct ipv6_test_table name = {				\
		.routes = name ## _routes,				\
		.count = ( sizeof ( name ## _routes ) /			\
			   sizeof ( name ## _routes[0] ) ),		\
		.list = LIST_HEAD_INIT ( name.list ),			\
	};

/** The unspecified IPv6 address */
static const struct in6_addr sample_unspecified = {
	.s6_addr = IPV6 ( 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ),
};

/** A sample link-local IPv6 address */
static const struct in6_addr sample_link_local = {
	.s6_addr = IPV6 ( 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x69, 0xff, 0xfe, 0x50, 0x58, 0x45 ),
};

/** A sample site-local IPv6 address */
static const struct in6_addr sample_site_local = {
	.s6_addr = IPV6 ( 0xfe, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 ),
};

/** A sample ULA IPv6 address */
static const struct in6_addr sample_ula = {
	.s6_addr = IPV6 ( 0xfd, 0x44, 0x91, 0x12, 0x64, 0x42, 0x00, 0x00,
			  0x00, 0x00, 0x69, 0xff, 0xfe, 0x50, 0x58, 0x45 ),
};

/** A sample global IPv6 address */
static const struct in6_addr sample_global = {
	.s6_addr = IPV6 ( 0x20, 0x01, 0x0b, 0xa8, 0x00, 0x00, 0x01, 0xd4,
			  0x00, 0x00, 0x00, 0x00, 0x69, 0x50, 0x58, 0x45 ),
};

/** A sample multicast IPv6 address */
static const struct in6_addr sample_multicast = {
	.s6_addr = IPV6 ( 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 ),
};

/** Dummy network device used for routing tests */
static struct net_device ipv6_test_netdev = {
	.refcnt = REF_INIT ( ref_no_free ),
	.index = 42,
	.state = NETDEV_OPEN,
};

/** Routing table with only a link-local address */
TABLE ( table_link_local,
	{ "fe80::69ff:fe50:5845", 64, NULL } );

/** Routing table with a global address */
TABLE ( table_normal,
	{ "fe80::69ff:fe50:5845", 64, NULL },
	{ "2001:db8:3::1", 64, "fe80::1" } );

/** Routing table with multiple addresses and routers */
TABLE ( table_multi,
	{ "fe80::69ff:fe50:5845", 64, NULL },
	{ "2001:db8:3::1", 64, "fe80::1" },
	{ "2001:db8:5::1", 64, NULL },
	{ "2001:db8:42::1", 64, "fe80::2" },
	{ "fd44:9112:6442::69ff:fe50:5845", 64, "fe80::1" },
	{ "fd70:6ba9:50ae::69ff:fe50:5845", 64, "fe80::3" } );

/**
 * Report an inet6_ntoa() test result
 *
 * @v addr		IPv6 address
 * @v text		Expected textual representation
 * @v file		Test code file
 * @v line		Test code line
 */
static void inet6_ntoa_okx ( const struct in6_addr *addr, const char *text,
			     const char *file, unsigned int line ) {
	char *actual;

	actual = inet6_ntoa ( addr );
	DBG ( "inet6_ntoa ( %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ) "
	      "= %s\n", ntohs ( addr->s6_addr16[0] ),
	      ntohs ( addr->s6_addr16[1] ), ntohs ( addr->s6_addr16[2] ),
	      ntohs ( addr->s6_addr16[3] ), ntohs ( addr->s6_addr16[4] ),
	      ntohs ( addr->s6_addr16[5] ), ntohs ( addr->s6_addr16[6] ),
	      ntohs ( addr->s6_addr16[7] ), actual );
	okx ( strcmp ( actual, text ) == 0, file, line );
}
#define inet6_ntoa_ok( addr, text ) do {				\
	static const struct in6_addr in = {				\
		.s6_addr = addr,					\
	};								\
	inet6_ntoa_okx ( &in, text, __FILE__, __LINE__ );		\
	} while ( 0 )

/**
 * Report an inet6_aton() test result
 *
 * @v text		Textual representation
 * @v addr		Expected IPv6 address
 * @v file		Test code file
 * @v line		Test code line
 */
static void inet6_aton_okx ( const char *text, const struct in6_addr *addr,
			     const char *file, unsigned int line ) {
	struct in6_addr actual;

	okx ( inet6_aton ( text, &actual ) == 0, file, line );
	DBG ( "inet6_aton ( \"%s\" ) = %s\n", text, inet6_ntoa ( &actual ) );
	okx ( memcmp ( &actual, addr, sizeof ( actual ) ) == 0,
	      file, line );
}
#define inet6_aton_ok( text, addr ) do {				\
	static const struct in6_addr in = {				\
		.s6_addr = addr,					\
	};								\
	inet6_aton_okx ( text, &in, __FILE__, __LINE__ );		\
	} while ( 0 )

/**
 * Report an inet6_aton() failure test result
 *
 * @v text		Textual representation
 * @v file		Test code file
 * @v line		Test code line
 */
static void inet6_aton_fail_okx ( const char *text, const char *file,
				  unsigned int line ) {
	struct in6_addr dummy;

	okx ( inet6_aton ( text, &dummy ) != 0, file, line );
}
#define inet6_aton_fail_ok( text )					\
	inet6_aton_fail_okx ( text, __FILE__, __LINE__ )

/**
 * Create test routing table
 *
 * @v table		Test routing table
 * @v file		Test code file
 * @v line		Test code line
 */
static void ipv6_table_okx ( struct ipv6_test_table *table, const char *file,
			     unsigned int line ) {
	const struct ipv6_test_route *route;
	struct in6_addr address;
	struct in6_addr router;
	struct list_head saved;
	unsigned int i;

	/* Sanity check */
	okx ( list_empty ( &table->list ), file, line );

	/* Save existing routing table */
	INIT_LIST_HEAD ( &saved );
	list_splice_init ( &ipv6_miniroutes, &saved );

	/* Construct routing table */
	for ( i = 0 ; i < table->count ; i++ ) {

		/* Parse address and router (if applicable) */
		route = &table->routes[i];
		okx ( inet6_aton ( route->address, &address ) == 0,
		      file, line );
		if ( route->router ) {
			okx ( inet6_aton ( route->router, &router ) == 0,
			      file, line );
		}

		/* Add routing table entry */
		okx ( ipv6_add_miniroute ( &ipv6_test_netdev, &address,
					   route->prefix_len,
					   ( route->router ?
					     &router : NULL ) ) == 0,
		      file, line );
	}

	/* Save constructed routing table */
	list_splice_init ( &ipv6_miniroutes, &table->list );

	/* Restore original routing table */
	list_splice ( &saved, &ipv6_miniroutes );
}
#define ipv6_table_ok( table )						\
	ipv6_table_okx ( table, __FILE__, __LINE__ )

/**
 * Report an ipv6_route() test result
 *
 * @v table		Test routing table
 * @v dest		Destination address
 * @v src		Expected source address, or NULL to expect failure
 * @v next		Expected next hop address, or NULL to expect destination
 * @v file		Test code file
 * @v line		Test code line
 */
static void ipv6_route_okx ( struct ipv6_test_table *table, const char *dest,
			     const char *src, const char *next,
			     const char *file, unsigned int line ) {
	struct in6_addr in_dest;
	struct in6_addr in_src;
	struct in6_addr in_next;
	struct in6_addr *actual;
	struct ipv6_miniroute *miniroute;
	struct list_head saved;

	/* Switch to test routing table */
	INIT_LIST_HEAD ( &saved );
	list_splice_init ( &ipv6_miniroutes, &saved );
	list_splice_init ( &table->list, &ipv6_miniroutes );

	/* Parse addresses */
	okx ( inet6_aton ( dest, &in_dest ) == 0, file, line );
	if ( src )
		okx ( inet6_aton ( src, &in_src ) == 0, file, line );
	if ( next ) {
		okx ( inet6_aton ( next, &in_next ) == 0, file, line );
	} else {
		memcpy ( &in_next, &in_dest, sizeof ( in_next ) );
	}

	/* Perform routing */
	actual = &in_dest;
	miniroute = ipv6_route ( ipv6_test_netdev.index, &actual );

	/* Validate result */
	if ( src ) {

		/* Check that a route was found */
		okx ( miniroute != NULL, file, line );
		DBG ( "ipv6_route ( %s ) = %s", dest, inet6_ntoa ( actual ) );
		DBG ( " from %s\n", inet6_ntoa ( &miniroute->address ) );

		/* Check that expected source address was used */
		okx ( memcmp ( &miniroute->address, &in_src,
			       sizeof ( in_src ) ) == 0, file, line );

		/* Check that expected next hop address was used */
		okx ( memcmp ( actual, &in_next, sizeof ( *actual ) ) == 0,
		      file, line );

	} else {

		/* Routing is expected to fail */
		okx ( miniroute == NULL, file, line );
	}

	/* Restore original routing table */
	list_splice_init ( &ipv6_miniroutes, &table->list );
	list_splice ( &saved, &ipv6_miniroutes );
}
#define ipv6_route_ok( table, dest, src, next )				\
	ipv6_route_okx ( table, dest, src, next, __FILE__, __LINE__ )

/**
 * Destroy test routing table
 *
 * @v table		Test routing table
 */
static void ipv6_table_del ( struct ipv6_test_table *table ) {
	struct ipv6_miniroute *miniroute;
	struct ipv6_miniroute *tmp;
	struct list_head saved;

	/* Switch to test routing table */
	INIT_LIST_HEAD ( &saved );
	list_splice_init ( &ipv6_miniroutes, &saved );
	list_splice_init ( &table->list, &ipv6_miniroutes );

	/* Delete all existing routes */
	list_for_each_entry_safe ( miniroute, tmp, &ipv6_miniroutes, list )
		ipv6_del_miniroute ( miniroute );

	/* Restore original routing table */
	list_splice ( &saved, &ipv6_miniroutes );
}

/**
 * Perform IPv6 self-tests
 *
 */
static void ipv6_test_exec ( void ) {

	/* Address testing macros */
	ok (   IN6_IS_ADDR_UNSPECIFIED ( &sample_unspecified ) );
	ok ( ! IN6_IS_ADDR_UNSPECIFIED ( &sample_link_local ) );
	ok ( ! IN6_IS_ADDR_UNSPECIFIED ( &sample_site_local ) );
	ok ( ! IN6_IS_ADDR_UNSPECIFIED ( &sample_ula ) );
	ok ( ! IN6_IS_ADDR_UNSPECIFIED ( &sample_global ) );
	ok ( ! IN6_IS_ADDR_UNSPECIFIED ( &sample_multicast ) );
	ok ( ! IN6_IS_ADDR_MULTICAST ( &sample_unspecified ) );
	ok ( ! IN6_IS_ADDR_MULTICAST ( &sample_link_local ) );
	ok ( ! IN6_IS_ADDR_MULTICAST ( &sample_site_local ) );
	ok ( ! IN6_IS_ADDR_MULTICAST ( &sample_ula ) );
	ok ( ! IN6_IS_ADDR_MULTICAST ( &sample_global ) );
	ok (   IN6_IS_ADDR_MULTICAST ( &sample_multicast ) );
	ok ( ! IN6_IS_ADDR_LINKLOCAL ( &sample_unspecified ) );
	ok (   IN6_IS_ADDR_LINKLOCAL ( &sample_link_local ) );
	ok ( ! IN6_IS_ADDR_LINKLOCAL ( &sample_site_local ) );
	ok ( ! IN6_IS_ADDR_LINKLOCAL ( &sample_ula ) );
	ok ( ! IN6_IS_ADDR_LINKLOCAL ( &sample_global ) );
	ok ( ! IN6_IS_ADDR_LINKLOCAL ( &sample_multicast ) );
	ok ( ! IN6_IS_ADDR_SITELOCAL ( &sample_unspecified ) );
	ok ( ! IN6_IS_ADDR_SITELOCAL ( &sample_link_local ) );
	ok (   IN6_IS_ADDR_SITELOCAL ( &sample_site_local ) );
	ok ( ! IN6_IS_ADDR_SITELOCAL ( &sample_ula ) );
	ok ( ! IN6_IS_ADDR_SITELOCAL ( &sample_global ) );
	ok ( ! IN6_IS_ADDR_SITELOCAL ( &sample_multicast ) );
	ok ( ! IN6_IS_ADDR_ULA ( &sample_unspecified ) );
	ok ( ! IN6_IS_ADDR_ULA ( &sample_link_local ) );
	ok ( ! IN6_IS_ADDR_ULA ( &sample_site_local ) );
	ok (   IN6_IS_ADDR_ULA ( &sample_ula ) );
	ok ( ! IN6_IS_ADDR_ULA ( &sample_global ) );
	ok ( ! IN6_IS_ADDR_ULA ( &sample_multicast ) );

	/* inet6_ntoa() tests */
	inet6_ntoa_ok ( IPV6 ( 0x20, 0x01, 0x0b, 0xa8, 0x00, 0x00, 0x01, 0xd4,
			       0x00, 0x00, 0x00, 0x00, 0x69, 0x50, 0x58, 0x45 ),
			"2001:ba8:0:1d4::6950:5845" );
	/* No zeros */
	inet6_ntoa_ok ( IPV6 ( 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x01, 0x00, 0x01,
			       0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01 ),
			"2001:db8:1:1:1:1:1:1" );
	/* Run of zeros */
	inet6_ntoa_ok ( IPV6 ( 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 ),
			"2001:db8::1" );
	/* No "::" for single zero */
	inet6_ntoa_ok ( IPV6 ( 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x01,
			       0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01 ),
			"2001:db8:0:1:1:1:1:1" );
	/* Use "::" for longest run of zeros */
	inet6_ntoa_ok ( IPV6 ( 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 ),
			"2001:0:0:1::1" );
	/* Use "::" for leftmost equal-length run of zeros */
	inet6_ntoa_ok ( IPV6 ( 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 ),
			"2001:db8::1:0:0:1" );
	/* Trailing run of zeros */
	inet6_ntoa_ok ( IPV6 ( 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ),
			"fe80::" );
	/* Leading run of zeros */
	inet6_ntoa_ok ( IPV6 ( 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 ),
			"::1" );
	/* All zeros */
	inet6_ntoa_ok ( IPV6 ( 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ),
			"::" );
	/* Maximum length */
	inet6_ntoa_ok ( IPV6 ( 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff ),
			"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" );

	/* inet6_aton() tests */
	inet6_aton_ok ( "2001:ba8:0:1d4::6950:5845",
			IPV6 ( 0x20, 0x01, 0x0b, 0xa8, 0x00, 0x00, 0x01, 0xd4,
			       0x00, 0x00, 0x00, 0x00, 0x69, 0x50, 0x58, 0x45));
	/* No zeros */
	inet6_aton_ok ( "2001:db8:1:1:1:1:1:1",
			IPV6 ( 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x01, 0x00, 0x01,
			       0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01));
	/* All intervening zeros */
	inet6_aton_ok ( "fe80::1",
			IPV6 ( 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01));
	/* Trailing run of zeros */
	inet6_aton_ok ( "fe80::",
			IPV6 ( 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00));
	/* Leading run of zeros */
	inet6_aton_ok ( "::1",
			IPV6 ( 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01));
	/* All zeros */
	inet6_aton_ok ( "::",
			IPV6 ( 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00));

	/* inet6_aton() failure tests */
	inet6_aton_fail_ok ( "20012:ba8:0:1d4::6950:5845" );
	inet6_aton_fail_ok ( "200z:ba8:0:1d4::6950:5845" );
	inet6_aton_fail_ok ( "2001.ba8:0:1d4::6950:5845" );
	inet6_aton_fail_ok ( "2001:db8:1:1:1:1:1" );
	inet6_aton_fail_ok ( "2001:db8:1:1:1:1:1:1:2" );
	inet6_aton_fail_ok ( "2001:db8::1::2" );
	inet6_aton_fail_ok ( "2001:ba8:0:1d4:::6950:5845" );
	inet6_aton_fail_ok ( ":::" );

	/* Create test routing tables */
	ipv6_table_ok ( &table_link_local );
	ipv6_table_ok ( &table_normal );
	ipv6_table_ok ( &table_multi );

	/* Routing table with only a link-local address */
	ipv6_route_ok ( &table_link_local, "fe80::1",
			"fe80::69ff:fe50:5845", NULL );
	ipv6_route_ok ( &table_link_local, "2001:db8:1::1",
			NULL, NULL );
	ipv6_route_ok ( &table_link_local, "ff02::1",
			"fe80::69ff:fe50:5845", NULL );

	/** Routing table with a global address */
	ipv6_route_ok ( &table_normal, "fe80::1",
			"fe80::69ff:fe50:5845", NULL );
	ipv6_route_ok ( &table_normal, "2001:db8:3::42",
			"2001:db8:3::1", NULL );
	ipv6_route_ok ( &table_normal, "2001:ba8:0:1d4::6950:5845",
			"2001:db8:3::1", "fe80::1" );
	ipv6_route_ok ( &table_normal, "ff02::1",
			"fe80::69ff:fe50:5845", NULL );
	ipv6_route_ok ( &table_normal, "ff0e::1",
			"2001:db8:3::1", NULL );

	/** Routing table with multiple addresses and routers */
	ipv6_route_ok ( &table_multi, "fe80::1",
			"fe80::69ff:fe50:5845", NULL );
	ipv6_route_ok ( &table_multi, "2001:db8:3::17",
			"2001:db8:3::1", NULL );
	ipv6_route_ok ( &table_multi, "2001:db8:5::92",
			"2001:db8:5::1", NULL );
	ipv6_route_ok ( &table_multi, "2001:db8:42::17",
			"2001:db8:42::1", NULL );
	ipv6_route_ok ( &table_multi, "2001:db8:5:1::17",
			"2001:db8:3::1", "fe80::1" );
	ipv6_route_ok ( &table_multi, "fd44:9112:6442::1",
			"fd44:9112:6442::69ff:fe50:5845", NULL );
	ipv6_route_ok ( &table_multi, "fd70:6ba9:50ae::1",
			"fd70:6ba9:50ae::69ff:fe50:5845", NULL );
	ipv6_route_ok ( &table_multi, "fd40::3",
			"fd44:9112:6442::69ff:fe50:5845", "fe80::1" );
	ipv6_route_ok ( &table_multi, "fd70::2",
			"fd70:6ba9:50ae::69ff:fe50:5845", "fe80::3" );
	ipv6_route_ok ( &table_multi, "ff02::1",
			"fe80::69ff:fe50:5845", NULL );

	/* Destroy test routing tables */
	ipv6_table_del ( &table_link_local );
	ipv6_table_del ( &table_normal );
	ipv6_table_del ( &table_multi );
}

/** IPv6 self-test */
struct self_test ipv6_test __self_test = {
	.name = "ipv6",
	.exec = ipv6_test_exec,
};
