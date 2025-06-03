#ifndef _IPXE_ARM_IO_H
#define _IPXE_ARM_IO_H

/** @file
 *
 * iPXE I/O API for ARM
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef IOAPI_ARM
#define IOAPI_PREFIX_arm
#else
#define IOAPI_PREFIX_arm __arm_
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
IOAPI_INLINE ( arm, phys_to_bus ) ( unsigned long phys_addr ) {
	return phys_addr;
}

static inline __always_inline unsigned long
IOAPI_INLINE ( arm, bus_to_phys ) ( unsigned long bus_addr ) {
	return bus_addr;
}

/*
 * MMIO reads and writes up to native word size
 *
 */

#define ARM_READX( _suffix, _type, _insn_suffix, _reg_prefix )		      \
static inline __always_inline _type					      \
IOAPI_INLINE ( arm, read ## _suffix ) ( volatile _type *io_addr ) {	      \
	_type data;							      \
	__asm__ __volatile__ ( "ldr" _insn_suffix " %" _reg_prefix "0, %1"    \
			       : "=r" ( data ) : "Qo" ( *io_addr ) );	      \
	return data;							      \
}
#ifdef __aarch64__
ARM_READX ( b, uint8_t, "b", "w" );
ARM_READX ( w, uint16_t, "h", "w" );
ARM_READX ( l, uint32_t, "", "w" );
ARM_READX ( q, uint64_t, "", "" );
#else
ARM_READX ( b, uint8_t, "b", "" );
ARM_READX ( w, uint16_t, "h", "" );
ARM_READX ( l, uint32_t, "", "" );
#endif

#define ARM_WRITEX( _suffix, _type, _insn_suffix, _reg_prefix )		      \
static inline __always_inline void					      \
IOAPI_INLINE ( arm, write ## _suffix ) ( _type data,			      \
					 volatile _type *io_addr ) {	      \
	__asm__ __volatile__ ( "str" _insn_suffix " %" _reg_prefix "0, %1"    \
			       : : "r" ( data ), "Qo" ( *io_addr ) );	      \
}
#ifdef __aarch64__
ARM_WRITEX ( b, uint8_t, "b", "w" );
ARM_WRITEX ( w, uint16_t, "h", "w" );
ARM_WRITEX ( l, uint32_t, "", "w" );
ARM_WRITEX ( q, uint64_t, "", "" );
#else
ARM_WRITEX ( b, uint8_t, "b", "" );
ARM_WRITEX ( w, uint16_t, "h", "" );
ARM_WRITEX ( l, uint32_t, "", "" );
#endif

/*
 * Memory barrier
 *
 */
static inline __always_inline void
IOAPI_INLINE ( arm, mb ) ( void ) {

#ifdef __aarch64__
	__asm__ __volatile__ ( "dmb sy" );
#else
	__asm__ __volatile__ ( "dmb" );
#endif
}

/* Dummy PIO */
DUMMY_PIO ( arm );

#endif /* _IPXE_ARM_IO_H */
