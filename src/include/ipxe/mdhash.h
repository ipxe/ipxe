#ifndef _IPXE_MDHASH_H
#define _IPXE_MDHASH_H

/** @file
 *
 * Merkle-Damgård hash algorithms
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <stdint.h>
#include <stddef.h>
#include <endian.h>
#include <ipxe/crypto.h>

/** Merkle-Damgård hash algorithm digest and data block */
#define mdhash_dd_t( digestsize, blocksize )				\
	struct {							\
		/** Digest of data already processed */			\
		uint8_t digest[ (digestsize) ];				\
		/** Accumulated data */					\
		uint8_t data[ (blocksize) ];				\
	} __attribute__ (( packed ))

/** Merkle-Damgård hash algorithm context */
#define mdhash_context_t( digestsize, blocksize )			\
	struct {							\
		/** Amount of accumulated data */			\
		size_t len;						\
		/** Digest and accumulated data */			\
		mdhash_dd_t ( (digestsize), (blocksize) ) dd;		\
	}

/** Merkle-Damgård trailing bit-length count */
union mdhash_len {
	/** Raw bytes */
	uint8_t byte[16];
	/** Host-endian qwords */
	struct {
		/** Bit length */
		uint64_t bits;
		/** Padding */
		uint64_t pad;
	} __attribute__ (( packed )) qword;
};

/** Merkle-Damgård hash algorithm */
struct mdhash_algorithm {
	/**
	 * Compression function
	 *
	 * @v dd		Digest and accumulated data
	 * @v digest		Copy of current digest value
	 *
	 * We provide a read-only copy of the current digest value
	 * since most compression functions would otherwise have to
	 * make this copy themselves.
	 */
	void ( * compress ) ( void *dd, const void *digest );
	/** Initial digest value (as host-endian words) */
	const void *init;
	/** Digest size (before any final truncation) */
	uint8_t digestsize;
	/** Data toggle (for endianness swapping)
	 *
	 * This is the value to be XORed with a byte offset in the
	 * input data stream to obtain the byte offset within the
	 * internal word array.
	 *
	 * This same value is also XORed with a byte offset in the
	 * internal word array to obtain the byte offset within the
	 * digest output.
	 */
	uint8_t toggle;
	/** Length field length
	 *
	 * This is the length (in bytes) of the trailing bit-length
	 * field (used for Merkle-Damgård strengthening).
	 */
	uint8_t len_len;
	/** Length field toggle (for endianness swapping)
	 *
	 * This is the value to be XORed with the byte offset within a
	 * host-endian 64-bit integer to obtain the byte offset within
	 * the trailing bit-length field.
	 */
	uint8_t len_toggle;
};

/** Merkle-Damgård hash algorithm digest and data block sample pointer */
#define MDHASH_DD_PTR( _dd ) ( ( typeof (_dd) * ) NULL )

/** Merkle-Damgård hash algorithm compression function sample pointer */
#define MDHASH_COMPRESS_PTR( _dd )					\
	( ( void ( * )							\
	    ( _dd *, const typeof ( MDHASH_DD_PTR (_dd)->digest ) * ) )	\
	  NULL )

/** Merkle-Damgård hash algorithm digest size */
#define MDHASH_DIGEST_SIZE( _dd )					\
	( sizeof ( MDHASH_DD_PTR (_dd)->digest ) )

/** Merkle-Damgård hash algorithm data toggle */
#define MDHASH_TOGGLE( _dd, _byteorder )				\
	( ( (_byteorder) == __BYTE_ORDER ) ? 0 :			\
	  ( sizeof ( MDHASH_DD_PTR (_dd)->digest.h[0] ) - 1 ) )

/** Merkle-Damgård hash algorithm length field length */
#define MDHASH_LEN_LEN( _dd )						\
	( sizeof ( MDHASH_DD_PTR (_dd)->data.final.len ) )

/** Merkle-Damgård hash algorithm length field toggle */
#define MDHASH_LEN_TOGGLE( _dd, _byteorder )				\
	( ( ( (_byteorder) == __BYTE_ORDER ) ? 0 :			\
	    ( sizeof ( uint64_t ) - 1 ) ) |				\
	  ( sizeof ( MDHASH_DD_PTR (_dd)->data.final.len ) -		\
	    sizeof ( uint64_t ) ) )

/** Merkle-Damgård hash algorithm block size */
#define MDHASH_BLOCK_SIZE( _dd )					\
	( sizeof ( MDHASH_DD_PTR (_dd)->data ) )

/** Merkle-Damgård hash algorithm context size */
#define MDHASH_CTX_SIZE( _dd )						\
	( sizeof ( mdhash_context_t ( MDHASH_DIGEST_SIZE (_dd),		\
				      MDHASH_BLOCK_SIZE (_dd) ) ) )

extern void mdhash_init ( struct digest_algorithm *digest, void *ctx );
extern void mdhash_update ( struct digest_algorithm *digest, void *ctx,
			    const void *src, size_t len );
extern void mdhash_final ( struct digest_algorithm *digest, void *ctx,
			   void *out );

/** Define a Merkle-Damgård hash algorithm */
#define MDHASH_ALGORITHM( _name, _digest, _compress, _byteorder,	\
			  _dd, _init, _digestsize )			\
	static_assert ( sizeof (_init) == MDHASH_DIGEST_SIZE (_dd) );	\
	static_assert ( (_digestsize) <= MDHASH_DIGEST_SIZE (_dd) );	\
	static_assert ( offsetof ( _dd, digest ) == 0 );		\
	static_assert ( offsetof ( _dd, data ) ==			\
			MDHASH_DIGEST_SIZE (_dd) );			\
	static_assert ( sizeof ( * MDHASH_DD_PTR (_dd) ) ==		\
			( MDHASH_DIGEST_SIZE (_dd) +			\
			  MDHASH_BLOCK_SIZE (_dd) ) );			\
	struct mdhash_algorithm _name ## _mdhash = {			\
		.compress	= ( ( MDHASH_COMPRESS_PTR (_dd)		\
				      == (_compress) )			\
				    ? ( ( void * ) (_compress) )	\
				    : ( ( void * ) (_compress) ) ),	\
		.init		= ( ( &( MDHASH_DD_PTR (_dd)->digest )	\
				      == &(_init) )			\
				    ? &(_init) : &(_init) ),		\
		.digestsize	= MDHASH_DIGEST_SIZE (_dd),		\
		.toggle		= MDHASH_TOGGLE (_dd, _byteorder),	\
		.len_len	= MDHASH_LEN_LEN (_dd),			\
		.len_toggle	= MDHASH_LEN_TOGGLE (_dd, _byteorder),	\
	};								\
	struct digest_algorithm _digest = {				\
		.name		= #_name,				\
		.ctxsize	= MDHASH_CTX_SIZE (_dd),		\
		.blocksize	= MDHASH_BLOCK_SIZE (_dd),		\
		.digestsize	= (_digestsize),			\
		.init		= mdhash_init,				\
		.update		= mdhash_update,			\
		.final		= mdhash_final,				\
		.priv		= &_name ## _mdhash,			\
	}

#endif /* _IPXE_MDHASH_H */
