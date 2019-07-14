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

/*
 * Memory space mappings
 *
 */

/** Page shift */
#define PAGE_SHIFT 12

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
 * Dummy PIO reads and writes up to 32 bits
 *
 * There is no common standard for I/O-space access for ARM, and
 * non-MMIO peripherals are vanishingly rare.  Provide dummy
 * implementations that will allow code to link and should cause
 * drivers to simply fail to detect hardware at runtime.
 *
 */

#define ARM_INX( _suffix, _type )					      \
static inline __always_inline _type					      \
IOAPI_INLINE ( arm, in ## _suffix ) ( volatile _type *io_addr __unused) {     \
	return ~( (_type) 0 );						      \
}									      \
static inline __always_inline void					      \
IOAPI_INLINE ( arm, ins ## _suffix ) ( volatile _type *io_addr __unused,      \
				       _type *data, unsigned int count ) {    \
	memset ( data, 0xff, count * sizeof ( *data ) );		      \
}
ARM_INX ( b, uint8_t );
ARM_INX ( w, uint16_t );
ARM_INX ( l, uint32_t );

#define ARM_OUTX( _suffix, _type )					      \
static inline __always_inline void					      \
IOAPI_INLINE ( arm, out ## _suffix ) ( _type data __unused,		      \
				       volatile _type *io_addr __unused ) {   \
	/* Do nothing */						      \
}									      \
static inline __always_inline void					      \
IOAPI_INLINE ( arm, outs ## _suffix ) ( volatile _type *io_addr __unused,     \
					const _type *data __unused,	      \
					unsigned int count __unused ) {	      \
	/* Do nothing */						      \
}
ARM_OUTX ( b, uint8_t );
ARM_OUTX ( w, uint16_t );
ARM_OUTX ( l, uint32_t );

/*
 * Slow down I/O
 *
 */
static inline __always_inline void
IOAPI_INLINE ( arm, iodelay ) ( void ) {
	/* Nothing to do */
}

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

#endif /* _IPXE_ARM_IO_H */
