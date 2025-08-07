#ifndef _DWGPIO_H
#define _DWGPIO_H

/** @file
 *
 * Synopsys DesignWare GPIO driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** Maximum number of GPIOs per port */
#define DWGPIO_MAX_COUNT 32

/** Software port
 *
 * This is the register bank containing the DR, DDR, and CTL bits.
 */
#define DWGPIO_SWPORT( x ) ( 0x00 + ( (x) * 0x0c ) )

/** Data register
 *
 * Bits written to this register are output if the corresponding DDR
 * bit is set to 1 (output) and the corresponding CTL bit is set to 0
 * (software control).
 *
 * Bits read from this register reflect the most recently written
 * value, and do not reflect the actual status of the GPIO pin.
 */
#define DWGPIO_SWPORT_DR 0x00

/** Data direction register
 *
 * The GPIO is an output if the corresponding bit in this register is
 * set to 1.
 */
#define DWGPIO_SWPORT_DDR 0x04

/** Control register
 *
 * The GPIO is under software control (i.e. is functioning as a GPIO,
 * rather than being controlled by a separate functional block) if the
 * corresponding bit in this register is set to 0.
 */
#define DWGPIO_SWPORT_CTL 0x08

/** External port
 *
 * Bits read from this register reflect the current status of the GPIO
 * pin.
 */
#define DWGPIO_EXT_PORT( x ) ( 0x50 + ( (x) * 0x04 ) )

/** A DesignWare GPIO port group */
struct dwgpio_group {
	/** Registers */
	void *regs;
};

/** A DesignWare GPIO port */
struct dwgpio {
	/** Device name */
	const char *name;
	/** Port index */
	unsigned int port;
	/** Software port registers */
	void *swport;
	/** External port register */
	void *ext;

	/** Original data register value */
	uint32_t dr;
	/** Original data direction register value */
	uint32_t ddr;
	/** Original control register value */
	uint32_t ctl;
};

#endif /* _DWGPIO_H */
