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
 */

FILE_LICENCE ( GPL2_OR_LATER );

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

/**
 * Report an inet6_ntoa() test result
 *
 * @v addr		IPv6 address
 * @v text		Expected textual representation
 */
#define inet6_ntoa_ok( addr, text ) do {				\
	static const struct in6_addr in = {				\
		.s6_addr = addr,					\
	};								\
	static const char expected[] = text;				\
	char *actual;							\
									\
	actual = inet6_ntoa ( &in );					\
	DBG ( "inet6_ntoa ( %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ) "	\
	      "= %s\n", ntohs ( in.s6_addr16[0] ),			\
	      ntohs ( in.s6_addr16[1] ), ntohs ( in.s6_addr16[2] ),	\
	      ntohs ( in.s6_addr16[3] ), ntohs ( in.s6_addr16[4] ),	\
	      ntohs ( in.s6_addr16[5] ), ntohs ( in.s6_addr16[6] ),	\
	      ntohs ( in.s6_addr16[7] ), actual );			\
	ok ( strcmp ( actual, expected ) == 0 );			\
	} while ( 0 )

/**
 * Perform IPv6 self-tests
 *
 */
static void ipv6_test_exec ( void ) {

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
}

/** IPv6 self-test */
struct self_test ipv6_test __self_test = {
	.name = "ipv6",
	.exec = ipv6_test_exec,
};
