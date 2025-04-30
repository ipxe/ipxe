#ifndef _IPXE_DUMMY_PIO_H
#define _IPXE_DUMMY_PIO_H

/** @file
 *
 * Dummy PIO reads and writes up to 32 bits
 *
 * There is no common standard for I/O-space access for non-x86 CPU
 * families, and non-MMIO peripherals are vanishingly rare.  Provide
 * dummy implementations that will allow code to link and should cause
 * drivers to simply fail to detect hardware at runtime.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>

#define DUMMY_INX( _prefix, _suffix, _type )				      \
static inline __always_inline _type					      \
IOAPI_INLINE ( _prefix, in ## _suffix ) ( volatile _type *io_addr __unused) { \
	return ~( (_type) 0 );						      \
}									      \
static inline __always_inline void					      \
IOAPI_INLINE ( _prefix, ins ## _suffix ) ( volatile _type *io_addr __unused,  \
					   _type *data, unsigned int count ) {\
	memset ( data, 0xff, count * sizeof ( *data ) );		      \
}

#define DUMMY_OUTX( _prefix, _suffix, _type )				      \
static inline __always_inline void					      \
IOAPI_INLINE ( _prefix, out ## _suffix ) ( _type data __unused,		      \
					   volatile _type *io_addr __unused ){\
	/* Do nothing */						      \
}									      \
static inline __always_inline void					      \
IOAPI_INLINE ( _prefix, outs ## _suffix ) ( volatile _type *io_addr __unused, \
					    const _type *data __unused,	      \
					    unsigned int count __unused ) {   \
	/* Do nothing */						      \
}

#define DUMMY_IODELAY( _prefix )					      \
static inline __always_inline void					      \
IOAPI_INLINE ( _prefix, iodelay ) ( void ) {				      \
	/* Nothing to do */						      \
}

#define DUMMY_PIO( _prefix )						      \
	DUMMY_INX ( _prefix, b, uint8_t );				      \
	DUMMY_INX ( _prefix, w, uint16_t );				      \
	DUMMY_INX ( _prefix, l, uint32_t );				      \
	DUMMY_OUTX ( _prefix, b, uint8_t );				      \
	DUMMY_OUTX ( _prefix, w, uint16_t );				      \
	DUMMY_OUTX ( _prefix, l, uint32_t );				      \
	DUMMY_IODELAY ( _prefix );

#define PROVIDE_DUMMY_PIO( _prefix )					      \
	PROVIDE_IOAPI_INLINE ( _prefix, inb );				      \
	PROVIDE_IOAPI_INLINE ( _prefix, inw );				      \
	PROVIDE_IOAPI_INLINE ( _prefix, inl );				      \
	PROVIDE_IOAPI_INLINE ( _prefix, outb );				      \
	PROVIDE_IOAPI_INLINE ( _prefix, outw );				      \
	PROVIDE_IOAPI_INLINE ( _prefix, outl );				      \
	PROVIDE_IOAPI_INLINE ( _prefix, iodelay );

#endif /* _IPXE_DUMMY_PIO_H */
