#ifndef _IPXE_NS16550_H
#define _IPXE_NS16550_H

/** @file
 *
 * 16550-compatible UART
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/uart.h>

/** Transmitter holding register */
#define NS16550_THR 0x00

/** Receiver buffer register */
#define NS16550_RBR 0x00

/** Interrupt enable register */
#define NS16550_IER 0x01

/** FIFO control register */
#define NS16550_FCR 0x02
#define NS16550_FCR_FE	0x01	/**< FIFO enable */

/** Line control register */
#define NS16550_LCR 0x03
#define NS16550_LCR_WLS0	0x01	/**< Word length select bit 0 */
#define NS16550_LCR_WLS1	0x02	/**< Word length select bit 1 */
#define NS16550_LCR_STB		0x04	/**< Number of stop bits */
#define NS16550_LCR_PEN		0x08	/**< Parity enable */
#define NS16550_LCR_EPS		0x10	/**< Even parity select */
#define NS16550_LCR_DLAB	0x80	/**< Divisor latch access bit */

#define NS16550_LCR_WORD_LEN(x)	( ( (x) - 5 ) << 0 )	/**< Word length */
#define NS16550_LCR_STOP_BITS(x) ( ( (x) - 1 ) << 2 )	/**< Stop bits */
#define NS16550_LCR_PARITY(x)	( ( (x) - 0 ) << 3 )	/**< Parity */

/**
 * Calculate line control register value
 *
 * @v word_len		Word length (5-8)
 * @v parity		Parity (0=none, 1=odd, 3=even)
 * @v stop_bits		Stop bits (1-2)
 * @ret lcr		Line control register value
 */
#define NS16550_LCR_WPS( word_len, parity, stop_bits )	\
	( NS16550_LCR_WORD_LEN ( (word_len) ) |		\
	  NS16550_LCR_PARITY ( (parity) ) |		\
	  NS16550_LCR_STOP_BITS ( (stop_bits) ) )

/** Default LCR value: 8 data bits, no parity, one stop bit */
#define NS16550_LCR_8N1 NS16550_LCR_WPS ( 8, 0, 1 )

/** Modem control register */
#define NS16550_MCR 0x04
#define NS16550_MCR_DTR		0x01	/**< Data terminal ready */
#define NS16550_MCR_RTS		0x02	/**< Request to send */

/** Line status register */
#define NS16550_LSR 0x05
#define NS16550_LSR_DR 		0x01	/**< Data ready */
#define NS16550_LSR_THRE	0x20	/**< Transmitter holding reg. empty */
#define NS16550_LSR_TEMT	0x40	/**< Transmitter empty */

/** Scratch register */
#define NS16550_SCR 0x07

/** Divisor latch (least significant byte) */
#define NS16550_DLL 0x00

/** Divisor latch (most significant byte) */
#define NS16550_DLM 0x01

/** A 16550-compatible UART */
struct ns16550_uart {
	/** Register base address */
	void *base;
	/** Register shift */
	unsigned int shift;
	/** Input clock frequency */
	unsigned int clock;
	/** Baud rate divisor */
	uint16_t divisor;
};

/** Post-division clock cycles per data bit */
#define NS16550_CLK_BIT 16

/** Default input clock rate (1.8432 MHz) */
#define NS16550_CLK_DEFAULT 1843200

#include <bits/ns16550.h>

/** Dummy COM1 UART for non-x86 platforms
 *
 * The architecture-independent config/serial.h header has long
 * included the line
 *
 *   #define COMCONSOLE COM1
 *
 * which is meaningless on non-x86 platforms where there is no COM1
 * port.  Allow COM1 to be treated as equivalent to "no UART" on
 * non-x86 platforms, to avoid breaking existing build configurations.
 */
#ifndef COM1
#define COM1 NULL
#endif

void ns16550_write ( struct ns16550_uart *ns16550, unsigned int address,
		     uint8_t data );
uint8_t ns16550_read ( struct ns16550_uart *ns16550, unsigned int address );

extern struct uart_operations ns16550_operations;

#endif /* _IPXE_NS16550_H */
