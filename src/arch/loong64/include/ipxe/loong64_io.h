#ifndef _IPXE_LOONG64_IO_H
#define _IPXE_LOONG64_IO_H

/** @file
 *
 * iPXE I/O API for LoongArch64
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef IOAPI_LOONG64
#define IOAPI_PREFIX_loong64
#else
#define IOAPI_PREFIX_loong64 __loong64_
#endif

#include <ipxe/dummy_pio.h>

/*
 * Memory space mappings
 *
 */

/*
 * Physical<->Bus address mappings
 *
 */

static inline __always_inline unsigned long
IOAPI_INLINE ( loong64, phys_to_bus ) ( unsigned long phys_addr ) {
	return phys_addr;
}

static inline __always_inline unsigned long
IOAPI_INLINE ( loong64, bus_to_phys ) ( unsigned long bus_addr ) {
	return bus_addr;
}

/*
 * MMIO reads and writes up to native word size
 *
 */

#define LOONG64_READX( _suffix, _type, _insn_suffix )			      \
static inline __always_inline _type					      \
IOAPI_INLINE ( loong64, read ## _suffix ) ( volatile _type *io_addr ) {	      \
	_type data;							      \
	__asm__ __volatile__ ( "ld." _insn_suffix " %0, %1"		      \
			       : "=r" ( data ) : "m" ( *io_addr ) );	      \
	return data;							      \
}
LOONG64_READX ( b, uint8_t, "bu");
LOONG64_READX ( w, uint16_t, "hu");
LOONG64_READX ( l, uint32_t, "wu");
LOONG64_READX ( q, uint64_t, "d");

#define LOONG64_WRITEX( _suffix, _type, _insn_suffix )			      \
static inline __always_inline void					      \
IOAPI_INLINE ( loong64, write ## _suffix ) ( _type data,		      \
					     volatile _type *io_addr ) {      \
	__asm__ __volatile__ ( "st." _insn_suffix " %0, %1"		      \
			       : : "r" ( data ), "m" ( *io_addr ) );	      \
}
LOONG64_WRITEX ( b, uint8_t, "b");
LOONG64_WRITEX ( w, uint16_t, "h");
LOONG64_WRITEX ( l, uint32_t, "w" );
LOONG64_WRITEX ( q, uint64_t, "d");

/*
 * Memory barrier
 *
 */
static inline __always_inline void
IOAPI_INLINE ( loong64, mb ) ( void ) {
	__asm__ __volatile__ ( "dbar 0" );
}

/* Dummy PIO */
DUMMY_PIO ( loong64 );

#endif /* _IPXE_LOONG64_IO_H */
