#ifndef _IPXE_GDBSERIAL_H
#define _IPXE_GDBSERIAL_H

/** @file
 *
 * GDB remote debugging over serial
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

struct gdb_transport;

extern struct gdb_transport * gdbserial_configure ( const char *port,
						    unsigned int baud );

#endif /* _IPXE_GDBSERIAL_H */
