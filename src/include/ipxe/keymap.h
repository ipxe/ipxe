#ifndef _IPXE_KEYMAP_H
#define _IPXE_KEYMAP_H

/**
 * @file
 *
 * Keyboard mappings
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/tables.h>

/** A remapped key
 *
 * Represents a mapping from an ASCII character (as interpreted from a
 * keyboard scancode by the US-only keyboard driver provided by the
 * BIOS) to the appropriate ASCII value for the keyboard layout.
 */
struct keymap_key {
	/** Character read from keyboard */
	uint8_t from;
	/** Character to be used instead */
	uint8_t to;
} __attribute__ (( packed ));

/** A keyboard mapping */
struct keymap {
	/** Name */
	const char *name;
	/** Basic remapping table (zero-terminated) */
	struct keymap_key *basic;
};

/** Keyboard mapping table */
#define KEYMAP __table ( struct keymap, "keymap" )

/** Define a keyboard mapping */
#define __keymap __table_entry ( KEYMAP, 01 )

/** Pseudo key flag */
#define KEYMAP_PSEUDO 0x80

extern unsigned int key_remap ( unsigned int character );

#endif /* _IPXE_KEYMAP_H */
