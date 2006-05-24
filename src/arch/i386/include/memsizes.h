#ifndef MEMSIZES_H
#define MEMSIZES_H

#warning "This header is no longer functional; use memmap.h instead"

/*
 * These structures seem to be very i386 (and, in fact, PCBIOS)
 * specific, so I've moved them out of etherboot.h.
 *
 */

struct e820entry {
	uint64_t addr;
	uint64_t size;
	uint32_t type;
#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3 /* usable as RAM once ACPI tables have been read */
#define E820_NVS	4
} __attribute__ (( packed ));
#define E820ENTRY_SIZE 20
#define E820MAX 32

struct meminfo {
	uint16_t basememsize;
	uint16_t pad;
	uint32_t memsize;
	uint32_t map_count;
	struct e820entry map[E820MAX];
} __attribute__ (( packed ));

extern struct meminfo meminfo;

extern void get_memsizes ( void );

#endif /* MEMSIZES_H */
