/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Settings self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <ipxe/settings.h>
#include <ipxe/test.h>

/** Define inline raw data */
#define RAW(...) { __VA_ARGS__ }

/**
 * Report a formatted-store test result
 *
 * @v settings		Settings block
 * @v setting		Setting
 * @v formatted		Formatted value
 * @v raw_array		Expected raw value
 */
#define storef_ok( settings, setting, formatted, raw_array ) do {	\
	const uint8_t expected[] = raw_array;				\
	uint8_t actual[ sizeof ( expected ) ];				\
	int len;							\
									\
	ok ( storef_setting ( settings, setting, formatted ) == 0 );	\
	len = fetch_setting ( settings, setting, actual,		\
			      sizeof ( actual ) );			\
	DBGC ( settings, "Stored %s \"%s\", got:\n",			\
	       (setting)->type->name, formatted );			\
	DBGC_HDA ( settings, 0, actual, len );				\
	ok ( len == ( int ) sizeof ( actual ) );			\
	ok ( memcmp ( actual, expected, sizeof ( actual ) ) == 0 );	\
	} while ( 0 )

/**
 * Report a formatted-fetch test result
 *
 * @v settings		Settings block
 * @v setting		Setting
 * @v raw_array		Raw value
 * @v formatted		Expected formatted value
 */
#define fetchf_ok( settings, setting, raw_array, formatted ) do {	\
	const uint8_t raw[] = raw_array;				\
	char actual[ strlen ( formatted ) + 1 ];			\
	int len;							\
									\
	ok ( store_setting ( settings, setting, raw,			\
			     sizeof ( raw ) ) == 0 );			\
	len = fetchf_setting ( settings, setting, actual,		\
			       sizeof ( actual ) );			\
	DBGC ( settings, "Fetched %s \"%s\" from:\n",			\
	       (setting)->type->name, formatted );			\
	DBGC_HDA ( settings, 0, raw, sizeof ( raw ) );			\
	ok ( len == ( int ) ( sizeof ( actual ) - 1 ) );		\
	ok ( strcmp ( actual, formatted ) == 0 );			\
	} while ( 0 )

/** Test generic settings block */
struct generic_settings test_generic_settings = {
	.settings = {
		.refcnt = NULL,
		.siblings =
		    LIST_HEAD_INIT ( test_generic_settings.settings.siblings ),
		.children =
		    LIST_HEAD_INIT ( test_generic_settings.settings.children ),
		.op = &generic_settings_operations,
	},
	.list = LIST_HEAD_INIT ( test_generic_settings.list ),
};

/** Test settings block */
#define test_settings test_generic_settings.settings

/** Test string setting */
static struct setting test_string_setting = {
	.name = "test_string",
	.type = &setting_type_string,
};

/** Test URI-encoded string setting */
static struct setting test_uristring_setting = {
	.name = "test_uristring",
	.type = &setting_type_uristring,
};

/** Test IPv4 address setting type */
static struct setting test_ipv4_setting = {
	.name = "test_ipv4",
	.type = &setting_type_ipv4,
};

/** Test signed 8-bit integer setting type */
static struct setting test_int8_setting = {
	.name = "test_int8",
	.type = &setting_type_int8,
};

/** Test signed 16-bit integer setting type */
static struct setting test_int16_setting = {
	.name = "test_int16",
	.type = &setting_type_int16,
};

/** Test signed 32-bit integer setting type */
static struct setting test_int32_setting = {
	.name = "test_int32",
	.type = &setting_type_int32,
};

/** Test unsigned 8-bit integer setting type */
static struct setting test_uint8_setting = {
	.name = "test_uint8",
	.type = &setting_type_uint8,
};

/** Test unsigned 16-bit integer setting type */
static struct setting test_uint16_setting = {
	.name = "test_uint16",
	.type = &setting_type_uint16,
};

/** Test unsigned 32-bit integer setting type */
static struct setting test_uint32_setting = {
	.name = "test_uint32",
	.type = &setting_type_uint32,
};

/** Test colon-separated hex string setting type */
static struct setting test_hex_setting = {
	.name = "test_hex",
	.type = &setting_type_hex,
};

/** Test hyphen-separated hex string setting type */
static struct setting test_hexhyp_setting = {
	.name = "test_hexhyp",
	.type = &setting_type_hexhyp,
};

/** Test UUID setting type */
static struct setting test_uuid_setting = {
	.name = "test_uuid",
	.type = &setting_type_uuid,
};

/**
 * Perform settings self-tests
 *
 */
static void settings_test_exec ( void ) {

	/* Register test settings block */
	ok ( register_settings ( &test_settings, NULL, "test" ) == 0 );

	/* "string" setting type */
	storef_ok ( &test_settings, &test_string_setting, "hello",
		    RAW ( 'h', 'e', 'l', 'l', 'o' ) );
	fetchf_ok ( &test_settings, &test_string_setting,
		    RAW ( 'w', 'o', 'r', 'l', 'd' ), "world" );

	/* "uristring" setting type */
	storef_ok ( &test_settings, &test_uristring_setting, "hello%20world",
		    RAW ( 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l',
			  'd' ) );
	fetchf_ok ( &test_settings, &test_uristring_setting,
		    RAW ( 1, 2, 3, 4, 5 ), "%01%02%03%04%05" );

	/* "ipv4" setting type */
	storef_ok ( &test_settings, &test_ipv4_setting, "192.168.0.1",
		    RAW ( 192, 168, 0, 1 ) );
	fetchf_ok ( &test_settings, &test_ipv4_setting,
		    RAW ( 212, 13, 204, 60 ), "212.13.204.60" );

	/* Integer setting types */
	storef_ok ( &test_settings, &test_int8_setting,
		    "54", RAW ( 54 ) );
	storef_ok ( &test_settings, &test_int8_setting,
		    "0x7f", RAW ( 0x7f ) );
	storef_ok ( &test_settings, &test_int8_setting,
		    "0x1234", RAW ( 0x34 ) );
	storef_ok ( &test_settings, &test_int8_setting,
		    "-32", RAW ( -32 ) );
	fetchf_ok ( &test_settings, &test_int8_setting,
		    RAW ( -9 ), "-9" );
	fetchf_ok ( &test_settings, &test_int8_setting,
		    RAW ( 106 ), "106" );
	storef_ok ( &test_settings, &test_uint8_setting,
		    "129", RAW ( 129 ) );
	storef_ok ( &test_settings, &test_uint8_setting,
		    "0x3421", RAW ( 0x21 ) );
	fetchf_ok ( &test_settings, &test_uint8_setting,
		    RAW ( 0x54 ), "0x54" );
	storef_ok ( &test_settings, &test_int16_setting,
		    "29483", RAW ( 0x73, 0x2b ) );
	fetchf_ok ( &test_settings, &test_int16_setting,
		    RAW ( 0x82, 0x14 ), "-32236" );
	fetchf_ok ( &test_settings, &test_int16_setting,
		    RAW ( 0x12, 0x78 ), "4728" );
	storef_ok ( &test_settings, &test_uint16_setting,
		    "48727", RAW ( 0xbe, 0x57 ) );
	fetchf_ok ( &test_settings, &test_uint16_setting,
		    RAW ( 0x9a, 0x24 ), "0x9a24" );
	storef_ok ( &test_settings, &test_int32_setting,
		    "2901274", RAW ( 0x00, 0x2c, 0x45, 0x1a ) );
	fetchf_ok ( &test_settings, &test_int32_setting,
		    RAW ( 0xff, 0x34, 0x2d, 0xaf ), "-13357649" );
	fetchf_ok ( &test_settings, &test_int32_setting,
		    RAW ( 0x01, 0x00, 0x34, 0xab ), "16790699" );
	storef_ok ( &test_settings, &test_uint32_setting,
		    "0xb598d21", RAW ( 0x0b, 0x59, 0x8d, 0x21 ) );
	fetchf_ok ( &test_settings, &test_uint32_setting,
		    RAW ( 0xf2, 0x37, 0xb2, 0x18 ), "0xf237b218" );

	/* "hex" setting type */
	storef_ok ( &test_settings, &test_hex_setting,
		    "", RAW ( 0x00 ) );
	storef_ok ( &test_settings, &test_hex_setting,
		    ":", RAW ( 0x00, 0x00 ) );
	storef_ok ( &test_settings, &test_hex_setting,
		    "1:2:", RAW ( 0x01, 0x02, 0x00 ) );
	storef_ok ( &test_settings, &test_hex_setting,
		    "08:12:f5:22:90:1b:4b:47:a8:30:cb:4d:67:4c:d6:76",
		    RAW ( 0x08, 0x12, 0xf5, 0x22, 0x90, 0x1b, 0x4b, 0x47, 0xa8,
			  0x30, 0xcb, 0x4d, 0x67, 0x4c, 0xd6, 0x76 ) );
	fetchf_ok ( &test_settings, &test_hex_setting,
		    RAW ( 0x62, 0xd9, 0xd4, 0xc4, 0x7e, 0x3b, 0x41, 0x46, 0x91,
			  0xc6, 0xfd, 0x0c, 0xbf ),
		    "62:d9:d4:c4:7e:3b:41:46:91:c6:fd:0c:bf" );

	/* "hexhyp" setting type */
	storef_ok ( &test_settings, &test_hexhyp_setting,
		    "11-33-22", RAW ( 0x11, 0x33, 0x22 ) );
	fetchf_ok ( &test_settings, &test_hexhyp_setting,
		    RAW ( 0x9f, 0xe5, 0x6d, 0xfb, 0x24, 0x3a, 0x4c, 0xbb, 0xa9,
			  0x09, 0x6c, 0x66, 0x13, 0xc1, 0xa8, 0xec, 0x27 ),
		    "9f-e5-6d-fb-24-3a-4c-bb-a9-09-6c-66-13-c1-a8-ec-27" );

	/* "uuid" setting type (no store capability) */
	fetchf_ok ( &test_settings, &test_uuid_setting,
		    RAW ( 0x1a, 0x6a, 0x74, 0x9d, 0x0e, 0xda, 0x46, 0x1a,0xa8,
			  0x7a, 0x7c, 0xfe, 0x4f, 0xca, 0x4a, 0x57 ),
		    "1a6a749d-0eda-461a-a87a-7cfe4fca4a57" );

	/* Clear and unregister test settings block */
	clear_settings ( &test_settings );
	unregister_settings ( &test_settings );
}

/** Settings self-test */
struct self_test settings_test __self_test = {
	.name = "settings",
	.exec = settings_test_exec,
};
