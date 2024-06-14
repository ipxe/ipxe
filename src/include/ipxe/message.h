#ifndef _IPXE_MESSAGE_H
#define _IPXE_MESSAGE_H

/** @file
 *
 * Message printing
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

extern void msg ( unsigned int row, const char *fmt, ... );
extern void clearmsg ( unsigned int row );
extern void alert ( unsigned int row, const char *fmt, ... );

#endif /* _IPXE_MESSAGE_H */
