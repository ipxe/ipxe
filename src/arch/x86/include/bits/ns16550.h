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

	outb ( data, ( ns16550->base + address ) );
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

	return inb ( ns16550->base + address );
}

/* Fixed ISA serial port base addresses */
#define COM1_BASE 0x3f8
#define COM2_BASE 0x2f8
#define COM3_BASE 0x3e8
#define COM4_BASE 0x2e8

/* Fixed ISA serial ports */
extern struct uart com1;
extern struct uart com2;
extern struct uart com3;
extern struct uart com4;

/* Fixed ISA serial port names */
#define COM1 &com1
#define COM2 &com2
#define COM3 &com3
#define COM4 &com4

#endif /* _BITS_NS16550_H */
