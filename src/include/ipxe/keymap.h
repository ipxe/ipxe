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
	/** AltGr remapping table (zero-terminated) */
	struct keymap_key *altgr;
};

/** Keyboard mapping table */
#define KEYMAP __table ( struct keymap, "keymap" )

/** Define a default keyboard mapping */
#define __keymap_default __table_entry ( KEYMAP, 01 )

/** Define a keyboard mapping */
#define __keymap __table_entry ( KEYMAP, 02 )

/** Mappable character mask */
#define KEYMAP_MASK 0xff

/** Pseudo key flag */
#define KEYMAP_PSEUDO 0x80

/** Ctrl key flag */
#define KEYMAP_CTRL 0x01000000

/** CapsLock key flag */
#define KEYMAP_CAPSLOCK 0x02000000

/** Undo CapsLock key flag
 *
 * Used when the keyboard driver has already interpreted the CapsLock
 * key, in which case the effect needs to be undone before remapping
 * in order to correctly handle keyboard mappings that swap alphabetic
 * and non-alphabetic keys.
 */
#define KEYMAP_CAPSLOCK_UNDO 0x04000000

/** Undo and redo CapsLock key flags */
#define KEYMAP_CAPSLOCK_REDO ( KEYMAP_CAPSLOCK | KEYMAP_CAPSLOCK_UNDO )

/** AltGr key flag */
#define KEYMAP_ALTGR 0x08000000

extern unsigned int key_remap ( unsigned int character );
extern struct keymap * keymap_find ( const char *name );
extern void keymap_set ( struct keymap *keymap );

#endif /* _IPXE_KEYMAP_H */
