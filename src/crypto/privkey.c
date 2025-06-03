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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ipxe/dhcp.h>
#include <ipxe/settings.h>
#include <ipxe/x509.h>
#include <ipxe/privkey.h>

/** @file
 *
 * Private key
 *
 * Life would in theory be easier if we could use a single file to
 * hold both the certificate and corresponding private key.
 * Unfortunately, the only common format which supports this is
 * PKCS#12 (aka PFX), which is too ugly to be allowed anywhere near my
 * codebase.  See, for reference and amusement:
 *
 *    http://www.cs.auckland.ac.nz/~pgut001/pubs/pfx.html
 */

/* Allow private key to be overridden if not explicitly specified */
#ifdef PRIVATE_KEY
#define ALLOW_KEY_OVERRIDE 0
#else
#define ALLOW_KEY_OVERRIDE 1
#endif

/* Raw private key data */
extern char private_key_data[];
extern size_t ABS_SYMBOL ( private_key_len );
__asm__ ( ".section \".rodata\", \"a\", " PROGBITS "\n\t"
	  "\nprivate_key_data:\n\t"
#ifdef PRIVATE_KEY
	  ".incbin \"" PRIVATE_KEY "\"\n\t"
#endif /* PRIVATE_KEY */
	  ".size private_key_data, ( . - private_key_data )\n\t"
	  ".equ private_key_len, ( . - private_key_data )\n\t"
	  ".previous\n\t" );

/** Private key */
struct private_key private_key = {
	.refcnt = REF_INIT ( ref_no_free ),
	.builder = {
		.data = private_key_data,
		.len = ABS_VALUE_INIT ( private_key_len ),
	},
};

/** Default private key */
static struct asn1_cursor default_private_key = {
	.data = private_key_data,
	.len = ( ( size_t ) private_key_len ),
};

/** Private key setting */
static struct setting privkey_setting __setting ( SETTING_CRYPTO, privkey ) = {
	.name = "privkey",
	.description = "Private key",
	.tag = DHCP_EB_KEY,
	.type = &setting_type_hex,
};

/**
 * Free private key
 *
 * @v refcnt		Reference counter
 */
void privkey_free ( struct refcnt *refcnt ) {
	struct private_key *key =
		container_of ( refcnt, struct private_key, refcnt );

	free ( key->builder.data );
	free ( key );
}

/**
 * Apply private key configuration settings
 *
 * @ret rc		Return status code
 */
static int privkey_apply_settings ( void ) {
	static void *key_data = NULL;
	int len;

	/* Allow private key to be overridden only if not explicitly
	 * specified at build time.
	 */
	if ( ALLOW_KEY_OVERRIDE ) {

		/* Restore default private key */
		memcpy ( &private_key.builder, &default_private_key,
			 sizeof ( private_key.builder ) );

		/* Fetch new private key, if any */
		free ( key_data );
		if ( ( len = fetch_raw_setting_copy ( NULL, &privkey_setting,
						      &key_data ) ) >= 0 ) {
			private_key.builder.data = key_data;
			private_key.builder.len = len;
		}
	}

	/* Debug */
	if ( private_key.builder.len ) {
		DBGC ( &private_key, "PRIVKEY using %s private key:\n",
		       ( key_data ? "external" : "built-in" ) );
		DBGC_HDA ( &private_key, 0, private_key.builder.data,
			   private_key.builder.len );
	} else {
		DBGC ( &private_key, "PRIVKEY has no private key\n" );
	}

	return 0;
}

/** Private key settings applicator */
struct settings_applicator privkey_applicator __settings_applicator = {
	.apply = privkey_apply_settings,
};
