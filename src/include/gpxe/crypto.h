#ifndef _GPXE_CRYPTO_H
#define _GPXE_CRYPTO_H

/** @file
 *
 * Cryptographic API
 *
 */

#include <stdint.h>

/**
 * A message-digest algorithm
 *
 */
struct digest_algorithm {
	/** Algorithm name */
	const char *name;
	/** Size of a context for this algorithm */
	size_t context_len;
	/** Size of a message digest for this algorithm */
	size_t digest_len;
	/**
	 * Initialise digest algorithm
	 *
	 * @v context		Context for digest operations
	 */
	void ( * init ) ( void *context );
	/**
	 * Calculate digest over data buffer
	 *
	 * @v context		Context for digest operations
	 * @v data		Data buffer
	 * @v len		Length of data buffer
	 */
	void ( * update ) ( void *context, const void *data, size_t len );
	/**
	 * Finish calculating digest
	 *
	 * @v context		Context for digest operations
	 * @v digest		Buffer for message digest
	 */
	void ( * finish ) ( void *context, void *digest );
};

#endif /* _GPXE_CRYPTO_H */
