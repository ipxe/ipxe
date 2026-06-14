#ifndef _BITS_TCPIP_H
#define _BITS_TCPIP_H

/** @file
 *
 * Transport-network layer interface
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

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
	struct s390x_pointer_pair pair = { data, len };
	unsigned int cksum;

	__asm__ ( /* Calculate 32-bit checksum */
		  "\n1:\n\t"
		  "cksm %0, %1\n\t"
		  "jo 1b\n\t"
		  /* Fold down to 16 bits */
		  "risbgz %1, %0, 32, 47, 16\n\t"
		  "alr %0, %1\n\t"
		  "srl %0, 16\n\t"
		  "alcr %0, %N1\n\t"
		  : "=&r" ( cksum ),
		    "+r" ( pair )
		  : "0" ( ~partial ) );

	return ~cksum;
}

#endif /* _BITS_TCPIP_H */
