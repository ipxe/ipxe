#ifndef _IPXE_NULL_TIME_H
#define _IPXE_NULL_TIME_H

/** @file
 *
 * Nonexistent time source
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef TIME_NULL
#define TIME_PREFIX_null
#else
#define TIME_PREFIX_null __null_
#endif

#endif /* _IPXE_NULL_TIME_H */
