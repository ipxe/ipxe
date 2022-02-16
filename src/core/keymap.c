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

#include <string.h>
#include <ctype.h>
#include <ipxe/keys.h>
#include <ipxe/keymap.h>

/** @file
 *
 * Keyboard mappings
 *
 */

/** ASCII character mask */
#define ASCII_MASK 0x7f

/** Control character mask */
#define CTRL_MASK 0x1f

/** Upper case character mask */
#define UPPER_MASK 0x5f

/** Case toggle bit */
#define CASE_TOGGLE ( ASCII_MASK & ~UPPER_MASK )

/** Default keyboard mapping */
static TABLE_START ( keymap_start, KEYMAP );

/** Current keyboard mapping */
static struct keymap *keymap_current = keymap_start;

/**
 * Remap a key
 *
 * @v character		Character read from console
 * @ret mapped		Mapped character
 */
unsigned int key_remap ( unsigned int character ) {
	struct keymap *keymap = keymap_current;
	unsigned int mapped = ( character & KEYMAP_MASK );
	struct keymap_key *key;

	/* Invert case before remapping if applicable */
	if ( ( character & KEYMAP_CAPSLOCK_UNDO ) && isalpha ( mapped ) )
		mapped ^= CASE_TOGGLE;

	/* Select remapping table */
	key = ( ( character & KEYMAP_ALTGR ) ? keymap->altgr : keymap->basic );

	/* Remap via table */
	for ( ; key->from ; key++ ) {
		if ( mapped == key->from ) {
			mapped = key->to;
			break;
		}
	}

	/* Handle Ctrl-<key> and CapsLock */
	if ( isalpha ( mapped ) ) {
		if ( character & KEYMAP_CTRL ) {
			mapped &= CTRL_MASK;
		} else if ( character & KEYMAP_CAPSLOCK ) {
			mapped ^= CASE_TOGGLE;
		}
	}

	/* Clear flags */
	mapped &= ASCII_MASK;

	DBGC2 ( &keymap_current, "KEYMAP mapped %04x => %02x\n",
		character, mapped );
	return mapped;
}

/**
 * Find keyboard map by name
 *
 * @v name		Keyboard map name
 * @ret keymap		Keyboard map, or NULL if not found
 */
struct keymap * keymap_find ( const char *name ) {
	struct keymap *keymap;

	/* Find matching keyboard map */
	for_each_table_entry ( keymap, KEYMAP ) {
		if ( strcmp ( keymap->name, name ) == 0 )
			return keymap;
	}

	return NULL;
}

/**
 * Set keyboard map
 *
 * @v keymap		Keyboard map, or NULL to use default
 */
void keymap_set ( struct keymap *keymap ) {

	/* Use default keymap if none specified */
	if ( ! keymap )
		keymap = keymap_start;

	/* Set new keyboard map */
	if ( keymap != keymap_current )
		DBGC ( &keymap_current, "KEYMAP using \"%s\"\n", keymap->name );
	keymap_current = keymap;
}
