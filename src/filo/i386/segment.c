/* Segmentation of the i386 architecture.
 *
 * 2003-07 by SONE Takeshi
 */
#include <etherboot.h>

//#include <lib.h>
#include <sys_info.h>
#include "segment.h"

#define DEBUG_THIS DEBUG_SEGMENT
#include <debug.h>

/* i386 lgdt argument */
struct gdtarg {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

/* GDT, the global descriptor table */
struct segment_desc gdt[NUM_SEG] = {
    /* 0x00: null segment */
    {0, 0, 0, 0, 0, 0},
    /* 0x08: flat code segment */
    {0xffff, 0, 0, 0x9f, 0xcf, 0},
    /* 0x10: flat data segment */
    {0xffff, 0, 0, 0x93, 0xcf, 0},
    /* 0x18: code segment for relocated execution */
    {0xffff, 0, 0, 0x9f, 0xcf, 0},
    /* 0x20: data segment for relocated execution */
    {0xffff, 0, 0, 0x93, 0xcf, 0},
};

/* Copy GDT to new location and reload it */
void move_gdt(unsigned long newgdt)
{
    struct gdtarg gdtarg;

    debug("Moving GDT to %#lx...", newgdt);
    memcpy(phys_to_virt(newgdt), gdt, sizeof gdt);
    gdtarg.base = newgdt;
    gdtarg.limit = GDT_LIMIT;
    debug("reloading GDT...");
    __asm__ __volatile__ ("lgdt %0\n\t" : : "m" (gdtarg));
    debug("reloading CS for fun...");
    __asm__ __volatile__ ("ljmp %0, $1f\n1:" : : "n" (RELOC_CS));
    debug("ok\n");
}
