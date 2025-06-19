#ifndef _BITS_NS16550_H
#define _BITS_NS16550_H

/** @file
 *
 * 16550-compatible UART
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/io.h>

/**
 * Write to UART register
 *
 * @v ns16550		16550 UART
 * @v address		Register address
 * @v data		Data
 */
static inline __attribute__ (( always_inline )) void
ns16550_write ( struct ns16550_uart *ns16550, unsigned int address,
		uint8_t data ) {

	writeb ( data, ( ns16550->base + ( address << ns16550->shift ) ) );
}

/**
 * Read from UART register
 *
 * @v ns16550		16550 UART
 * @v address		Register address
 * @ret data		Data
 */
static inline __attribute__ (( always_inline )) uint8_t
ns16550_read ( struct ns16550_uart *ns16550, unsigned int address ) {

	return readb ( ns16550->base + ( address << ns16550->shift ) );
}

#endif /* _BITS_NS16550_H */
