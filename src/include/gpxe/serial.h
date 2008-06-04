#ifndef _GPXE_SERIAL_H
#define _GPXE_SERIAL_H

/** @file
 *
 * Serial driver functions
 *
 */

extern void serial_putc ( int ch );
extern int serial_getc ( void );
extern int serial_ischar ( void );

#endif /* _GPXE_SERIAL_H */
