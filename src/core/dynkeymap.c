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
 * Dynamic keyboard mappings
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <ipxe/settings.h>
#include <ipxe/keymap.h>

/**
 * Require a keyboard map
 *
 * @v name		Keyboard map name
 */
#define REQUIRE_KEYMAP( name ) REQUIRE_OBJECT ( keymap_ ## name )

/** Keyboard map setting */
const struct setting keymap_setting __setting ( SETTING_MISC, keymap ) = {
	.name = "keymap",
	.description = "Keyboard map",
	.type = &setting_type_string,
};

/**
 * Apply keyboard map settings
 *
 * @ret rc		Return status code
 */
static int keymap_apply ( void ) {
	struct keymap *keymap;
	char *name;
	int rc;

	/* Fetch keyboard map name */
	fetch_string_setting_copy ( NULL, &keymap_setting, &name );

	/* Identify keyboard map */
	if ( name ) {
		/* Identify named keyboard map */
		keymap = keymap_find ( name );
		if ( ! keymap ) {
			DBGC ( &keymap_setting, "KEYMAP could not identify "
			       "\"%s\"\n", name );
			rc = -ENOENT;
			goto err_unknown;
		}
	} else {
		/* Use default keyboard map */
		keymap = NULL;
	}

	/* Set keyboard map */
	keymap_set ( keymap );

	/* Success */
	rc = 0;

 err_unknown:
	free ( name );
	return rc;
}

/** Keyboard map setting applicator */
struct settings_applicator keymap_applicator __settings_applicator = {
	.apply = keymap_apply,
};

/* Provide virtual "dynamic" keyboard map for linker */
PROVIDE_SYMBOL ( obj_keymap_dynamic );

/* Drag in keyboard maps via keymap_setting */
REQUIRING_SYMBOL ( keymap_setting );

/* Require all known keyboard maps */
REQUIRE_KEYMAP ( al );
REQUIRE_KEYMAP ( by );
REQUIRE_KEYMAP ( cf );
REQUIRE_KEYMAP ( cz );
REQUIRE_KEYMAP ( de );
REQUIRE_KEYMAP ( dk );
REQUIRE_KEYMAP ( es );
REQUIRE_KEYMAP ( et );
REQUIRE_KEYMAP ( fi );
REQUIRE_KEYMAP ( fr );
REQUIRE_KEYMAP ( gr );
REQUIRE_KEYMAP ( hu );
REQUIRE_KEYMAP ( il );
REQUIRE_KEYMAP ( it );
REQUIRE_KEYMAP ( lt );
REQUIRE_KEYMAP ( mk );
REQUIRE_KEYMAP ( mt );
REQUIRE_KEYMAP ( nl );
REQUIRE_KEYMAP ( no );
REQUIRE_KEYMAP ( no_latin1 );
REQUIRE_KEYMAP ( pl );
REQUIRE_KEYMAP ( pt );
REQUIRE_KEYMAP ( ro );
REQUIRE_KEYMAP ( ru );
REQUIRE_KEYMAP ( se );
REQUIRE_KEYMAP ( sg );
REQUIRE_KEYMAP ( sr_latin );
REQUIRE_KEYMAP ( ua );
REQUIRE_KEYMAP ( uk );
REQUIRE_KEYMAP ( us );
