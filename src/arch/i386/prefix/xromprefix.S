/*
 * ROM prefix that loads the bulk of the ROM using direct PCI accesses,
 * so as not to take up much option ROM space on PCI <3.0 systems.
 */

FILE_LICENCE ( GPL2_OR_LATER )

#define LOAD_ROM_FROM_PCI
#include "romprefix.S"
