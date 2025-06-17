#ifndef _IPXE_UART_H
#define _IPXE_UART_H

/** @file
 *
 * Generic UART
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/refcnt.h>
#include <ipxe/list.h>

/** A generic UART */
struct uart {
	/** Reference count */
	struct refcnt refcnt;
	/** Name */
	const char *name;
	/** List of registered UARTs */
	struct list_head list;

	/** UART operations */
	struct uart_operations *op;
	/** Driver-private data */
	void *priv;
};

/** UART operations */
struct uart_operations {
	/**
	 * Transmit byte
	 *
	 * @v uart		UART
	 * @v byte		Byte to transmit
	 * @ret rc		Return status code
	 */
	void ( * transmit ) ( struct uart *uart, uint8_t byte );
	/**
	 * Check if data is ready
	 *
	 * @v uart		UART
	 * @ret ready		Data is ready
	 */
	int ( * data_ready ) ( struct uart *uart );
	/**
	 * Receive byte
	 *
	 * @v uart		UART
	 * @ret byte		Received byte
	 */
	uint8_t ( * receive ) ( struct uart *uart );
	/**
	 * Initialise UART
	 *
	 * @v uart		UART
	 * @v baud		Baud rate, or zero to leave unchanged
	 * @ret rc		Return status code
	 */
	int ( * init ) ( struct uart *uart, unsigned int baud );
	/**
	 * Flush transmitted data
	 *
	 * @v uart		UART
	 */
	void ( * flush ) ( struct uart *uart );
};

/**
 * Transmit byte
 *
 * @v uart		UART
 * @v byte		Byte to transmit
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) void
uart_transmit ( struct uart *uart, uint8_t byte ) {

	uart->op->transmit ( uart, byte );
}

/**
 * Check if data is ready
 *
 * @v uart		UART
 * @ret ready		Data is ready
 */
static inline __attribute__ (( always_inline )) int
uart_data_ready ( struct uart *uart ) {

	return uart->op->data_ready ( uart );
}

/**
 * Receive byte
 *
 * @v uart		UART
 * @ret byte		Received byte
 */
static inline __attribute__ (( always_inline )) uint8_t
uart_receive ( struct uart *uart ) {

	return uart->op->receive ( uart );
}

/**
 * Initialise UART
 *
 * @v uart		UART
 * @v baud		Baud rate, or zero to leave unchanged
 * @ret rc		Return status code
 */
static inline __attribute__ (( always_inline )) int
uart_init ( struct uart *uart, unsigned int baud ) {

	return uart->op->init ( uart, baud );
}

/**
 * Flush transmitted data
 *
 * @v uart		UART
 */
static inline __attribute__ (( always_inline )) void
uart_flush ( struct uart *uart ) {

	uart->op->flush ( uart );
}

extern struct list_head uarts;
extern struct uart_operations null_uart_operations;

/**
 * Get reference to UART
 *
 * @v uart		UART
 * @ret uart		UART
 */
static inline __attribute__ (( always_inline )) struct uart *
uart_get ( struct uart *uart ) {

	ref_get ( &uart->refcnt );
	return uart;
}

/**
 * Drop reference to UART
 *
 * @v uart		UART
 */
static inline __attribute__ (( always_inline )) void
uart_put ( struct uart *uart ) {

	ref_put ( &uart->refcnt );
}

/**
 * Nullify UART
 *
 * @v uart		UART
 */
static inline __attribute__ (( always_inline )) void
uart_nullify ( struct uart *uart ) {

	uart->op = &null_uart_operations;
}

extern struct uart * alloc_uart ( size_t priv_len );
extern int uart_register ( struct uart *uart );
extern int uart_register_fixed ( void );
extern void uart_unregister ( struct uart *uart );
extern struct uart * uart_find ( const char *name );

#endif /* _IPXE_UART_H */
