#ifndef _GPXE_CHAP_H
#define _GPXE_CHAP_H

/** @file
 *
 * CHAP protocol
 *
 */

#include <stdint.h>
#include <gpxe/md5.h>

struct crypto_algorithm;

/** A CHAP challenge/response */
struct chap_challenge {
	/** Digest algorithm used for the response */
	struct crypto_algorithm *digest;
	/** Context used by the digest algorithm */
	uint8_t *digest_context;
	/** CHAP response */
	uint8_t *response;
	/** Length of CHAP response */
	size_t response_len;
};

extern int chap_init ( struct chap_challenge *chap,
		       struct crypto_algorithm *digest );
extern void chap_update ( struct chap_challenge *chap, const void *data,
			  size_t len );
extern void chap_respond ( struct chap_challenge *chap );
extern void chap_finish ( struct chap_challenge *chap );

/**
 * Add identifier data to the CHAP challenge
 *
 * @v chap		CHAP challenge/response
 * @v identifier	CHAP identifier
 *
 * The CHAP identifier is the first byte of the CHAP challenge.  This
 * function is a notational convenience for calling chap_update() for
 * the identifier byte.
 */
static inline void chap_set_identifier ( struct chap_challenge *chap,
					 unsigned int identifier ) {
	uint8_t ident_byte = identifier;

	chap_update ( chap, &ident_byte, sizeof ( ident_byte ) );
}

#endif /* _GPXE_CHAP_H */
