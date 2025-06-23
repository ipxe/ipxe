#ifndef _IPXE_SERIAL_H
#define _IPXE_SERIAL_H

/** @file
 *
 * Serial console
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/api.h>
#include <ipxe/uart.h>
#include <config/serial.h>

#ifdef SERIAL_NULL
#define SERIAL_PREFIX_null
#else
#define SERIAL_PREFIX_null __null_
#endif

/**
 * Calculate static inline serial API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define SERIAL_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( SERIAL_PREFIX_ ## _subsys, _api_func )

/**
 * Provide a serial API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_SERIAL( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( SERIAL_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline serial API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_SERIAL_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( SERIAL_PREFIX_ ## _subsys, _api_func )

/**
 * Get null serial console UART
 *
 * @ret uart		Serial console UART, or NULL
 */
static inline __always_inline struct uart *
SERIAL_INLINE ( null, default_serial_console ) ( void ) {
	return NULL;
}

/**
 * Get serial console UART
 *
 * @ret uart		Serial console UART, or NULL
 */
struct uart * default_serial_console ( void );

extern struct uart *serial_console;

#endif /* _IPXE_SERIAL_H */
