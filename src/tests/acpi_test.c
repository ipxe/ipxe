/*
 * Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
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
 * ACPI tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <ipxe/acpi.h>
#include <ipxe/acpimac.h>
#include <ipxe/if_ether.h>
#include <ipxe/test.h>

/** An ACPI test table signature */
union acpi_test_signature {
	/** String */
	char str[4];
	/** Raw value */
	uint32_t raw;
};

/** An ACPI test table */
struct acpi_test_table {
	/** Signature */
	union acpi_test_signature signature;
	/** Table content */
	const void *data;
};

/** An ACPI test table set */
struct acpi_test_tables {
	/** Tables */
	struct acpi_test_table **table;
	/** Number of tables */
	unsigned int count;
};

/** An ACPI MAC extraction test */
struct acpi_mac_test {
	/** ACPI test table set */
	struct acpi_test_tables *tables;
	/** Expected MAC address */
	uint8_t expected[ETH_ALEN];
};

/** Define inline data */
#define DATA(...) { __VA_ARGS__ }

/** Define an ACPI test table */
#define ACPI_TABLE( name, SIGNATURE, DATA )				\
	static const uint8_t name ## _data[] = DATA;			\
	static struct acpi_test_table name = {				\
		.signature = {						\
			.str = SIGNATURE,				\
		},							\
		.data = name ## _data,					\
	}

/** Define an ACPI test table set */
#define ACPI_TABLES( name, ... )					\
	static struct acpi_test_table * name ## _table[] =		\
		{ __VA_ARGS__ };					\
	static struct acpi_test_tables name = {				\
		.table = name ## _table,				\
		.count = ( sizeof ( name ## _table ) /			\
			   sizeof ( name ## _table[0] ) ),		\
	}

/** Define an ACPI MAC extraction test */
#define ACPI_MAC( name, TABLES, EXPECTED )				\
	static struct acpi_mac_test name = {				\
		.tables = TABLES,					\
		.expected = EXPECTED,					\
	}

/** "AMAC" SSDT
 *
 * DefinitionBlock ("", "SSDT", 2, "", "", 0x0) {
 *   Scope (\_SB) {
 *     Method (HW00, 0, Serialized) { Return(0) }
 *     Method (AMAC, 0, Serialized) { ToString("_AUXMAC_#525400aabbcc#") }
 *     Method (HW42, 0, Serialized) { Return(42) }
 *   }
 * }
 */
ACPI_TABLE ( amac_ssdt, "SSDT",
	     DATA ( 0x53, 0x53, 0x44, 0x54, 0x5d, 0x00, 0x00, 0x00, 0x02,
		    0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x49, 0x4e, 0x54, 0x4c, 0x04, 0x06, 0x21, 0x20,
		    0x10, 0x38, 0x5c, 0x5f, 0x53, 0x42, 0x5f, 0x14, 0x08,
		    0x48, 0x57, 0x30, 0x30, 0x08, 0xa4, 0x00, 0x14, 0x1e,
		    0x41, 0x4d, 0x41, 0x43, 0x08, 0x0d, 0x5f, 0x41, 0x55,
		    0x58, 0x4d, 0x41, 0x43, 0x5f, 0x23, 0x35, 0x32, 0x35,
		    0x34, 0x30, 0x30, 0x61, 0x61, 0x62, 0x62, 0x63, 0x63,
		    0x23, 0x00, 0x14, 0x09, 0x48, 0x57, 0x34, 0x32, 0x08,
		    0xa4, 0x0a, 0x2a ) );

/** "AMAC" test tables */
ACPI_TABLES ( amac_tables, &amac_ssdt );

/** "AMAC" test */
ACPI_MAC ( amac, &amac_tables,
	   DATA ( 0x52, 0x54, 0x00, 0xaa, 0xbb, 0xcc ) );

/** "MACA" SSDT1 (does not contain AUXMAC) */
ACPI_TABLE ( maca_ssdt1, "SSDT",
	     DATA ( 0x53, 0x53, 0x44, 0x54, 0x3e, 0x00, 0x00, 0x00, 0x02,
		    0x5f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x49, 0x4e, 0x54, 0x4c, 0x04, 0x06, 0x21, 0x20,
		    0x10, 0x19, 0x5c, 0x5f, 0x53, 0x42, 0x5f, 0x14, 0x08,
		    0x48, 0x57, 0x30, 0x30, 0x08, 0xa4, 0x00, 0x14, 0x09,
		    0x48, 0x57, 0x34, 0x32, 0x08, 0xa4, 0x0a, 0x2a ) );

/** "MACA" SSDT2 (contains AUXMAC) */
ACPI_TABLE ( maca_ssdt2, "SSDT",
	     DATA (  0x53, 0x53, 0x44, 0x54, 0x54, 0x00, 0x00, 0x00, 0x02,
		     0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		     0x00, 0x49, 0x4e, 0x54, 0x4c, 0x04, 0x06, 0x21, 0x20,
		     0x10, 0x2f, 0x5c, 0x5f, 0x53, 0x42, 0x5f, 0x14, 0x1e,
		     0x4d, 0x41, 0x43, 0x41, 0x08, 0x0d, 0x5f, 0x41, 0x55,
		     0x58, 0x4d, 0x41, 0x43, 0x5f, 0x23, 0x35, 0x32, 0x35,
		     0x34, 0x30, 0x30, 0x31, 0x31, 0x32, 0x32, 0x33, 0x33,
		     0x23, 0x00, 0x14, 0x09, 0x48, 0x57, 0x39, 0x39, 0x08,
		     0xa4, 0x0a, 0x63 ) );

/** "MACA" test tables */
ACPI_TABLES ( maca_tables, &maca_ssdt1, &maca_ssdt2 );

/** "MACA" test */
ACPI_MAC ( maca, &maca_tables,
	   DATA ( 0x52, 0x54, 0x00, 0x11, 0x22, 0x33 ) );

/** "RTMA" SSDT */
ACPI_TABLE ( rtma_ssdt, "SSDT",
	     DATA ( 0x53, 0x53, 0x44, 0x54, 0x44, 0x00, 0x00, 0x00, 0x02,
		    0x6d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x49, 0x4e, 0x54, 0x4c, 0x04, 0x06, 0x21, 0x20,
		    0x10, 0x1f, 0x5c, 0x5f, 0x53, 0x42, 0x5f, 0x14, 0x18,
		    0x52, 0x54, 0x4d, 0x41, 0x08, 0x0d, 0x5f, 0x52, 0x54,
		    0x58, 0x4d, 0x41, 0x43, 0x5f, 0x23, 0x52, 0x54, 0x30,
		    0x30, 0x30, 0x31, 0x23, 0x00 ) );

/** "RTMA" test tables */
ACPI_TABLES ( rtma_tables, &rtma_ssdt );

/** "RTMA" test */
ACPI_MAC ( rtma, &rtma_tables,
	   DATA ( 0x52, 0x54, 0x30, 0x30, 0x30, 0x31 ) );

/** Current ACPI test table set */
static struct acpi_test_tables *acpi_test_tables;

/**
 * Locate ACPI test table
 *
 * @v signature		Requested table signature
 * @v index		Requested index of table with this signature
 * @ret table		Table, or NULL if not found
 */
static const struct acpi_header * acpi_test_find ( uint32_t signature,
						   unsigned int index ) {
	struct acpi_test_table *table;
	unsigned int i;

	/* Fail if no test tables are installed */
	if ( ! acpi_test_tables )
		return NULL;

	/* Scan through test tables */
	for ( i = 0 ; i < acpi_test_tables->count ; i++ ) {
		table = acpi_test_tables->table[i];
		if ( ( signature == le32_to_cpu ( table->signature.raw ) ) &&
		     ( index-- == 0 ) ) {
			return table->data;
		}
	}

	return NULL;
}

/** Override ACPI table finder */
typeof ( acpi_find ) *acpi_finder = acpi_test_find;

/**
 * Report ACPI MAC extraction test result
 *
 * @v test		ACPI MAC extraction test
 * @v file		Test code file
 * @v line		Test code line
 */
static void acpi_mac_okx ( struct acpi_mac_test *test,
			   const char *file, unsigned int line ) {
	uint8_t mac[ETH_ALEN];
	int rc;

	/* Set test table set */
	acpi_test_tables = test->tables;

	/* Extract MAC address */
	rc = acpi_mac ( mac );
	okx ( rc == 0, file, line );

	/* Check extracted MAC address */
	okx ( memcmp ( mac, test->expected, ETH_ALEN ) == 0, file, line );

	/* Clear test table set */
	acpi_test_tables = NULL;
}
#define acpi_mac_ok( test ) \
	acpi_mac_okx ( test, __FILE__, __LINE__ )

/**
 * Perform ACPI self-test
 *
 */
static void acpi_test_exec ( void ) {

	/* MAC extraction tests */
	acpi_mac_ok ( &amac );
	acpi_mac_ok ( &maca );
	acpi_mac_ok ( &rtma );
}

/** ACPI self-test */
struct self_test acpi_test __self_test = {
	.name = "acpi",
	.exec = acpi_test_exec,
};
