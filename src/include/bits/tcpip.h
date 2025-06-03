#ifndef _BITS_TCPIP_H
#define _BITS_TCPIP_H

/** @file
 *
 * Generic architecture-specific transport-network layer interface
 *
 * This file is included only if the architecture does not provide its
 * own version of this file.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/**
 * Calculate continued TCP/IP checkum
 *
 * @v partial		Checksum of already-summed data, in network byte order
 * @v data		Data buffer
 * @v len		Length of data buffer
 * @ret cksum		Updated checksum, in network byte order
 */
static inline __attribute__ (( always_inline )) uint16_t
tcpip_continue_chksum ( uint16_t partial, const void *data, size_t len ) {

	/* Not yet optimised */
	return generic_tcpip_continue_chksum ( partial, data, len );
}

#endif /* _BITS_TCPIP_H */
