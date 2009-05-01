#ifndef _GPXE_GDBSERIAL_H
#define _GPXE_GDBSERIAL_H

/** @file
 *
 * GDB remote debugging over serial
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

struct gdb_transport;

/**
 * Set up the serial transport
 *
 * @ret transport suitable for starting the GDB stub or NULL on error
 */
struct gdb_transport *gdbserial_configure ( void );

#endif /* _GPXE_GDBSERIAL_H */
