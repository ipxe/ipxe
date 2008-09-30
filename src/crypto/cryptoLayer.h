#ifndef _MATRIXSSL_CRYPTOLAYER_H
#define _MATRIXSSL_CRYPTOLAYER_H

/** @file
 *
 * Compatibility layer for MatrixSSL
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <byteswap.h>
#include <gpxe/rotate.h>
#include <gpxe/crypto.h>

/* Drag in pscrypto.h */
typedef uint64_t ulong64;
typedef void psPool_t;
#define SMALL_CODE
#define USE_INT64
#define USE_RSA
#define USE_RSA_PUBLIC_ENCRYPT
#define CRYPT
#include "matrixssl/pscrypto.h"
#define SMALL_CODE
#undef CLEAN_STACK

#define sslAssert( ... ) assert ( __VA_ARGS__ )

static inline __attribute__ (( always_inline )) void * __malloc
psMalloc ( psPool_t *pool __unused, size_t len ) {
	return malloc ( len );
}

static inline __attribute__ (( always_inline )) void *
psRealloc ( void *ptr, size_t len ) {
	return realloc ( ptr, len );
}

static inline __attribute__ (( always_inline )) void psFree ( void *ptr ) {
	free ( ptr );
}

#define matrixStrDebugMsg( ... ) DBG ( __VA_ARGS__ )
#define matrixIntDebugMsg( ... ) DBG ( __VA_ARGS__ )

/* Use our standard cpu_to_leXX etc. macros */

#undef LOAD32L
#define LOAD32L( cpu32, ptr ) do {				\
		uint32_t *le32 = ( ( uint32_t * ) ptr );	\
		cpu32 = le32_to_cpu ( *le32 );			\
	} while ( 0 )

#undef LOAD32H
#define LOAD32H( cpu32, ptr ) do {				\
		uint32_t *be32 = ( ( uint32_t * ) ptr );	\
		cpu32 = be32_to_cpu ( *be32 );			\
	} while ( 0 )

#undef LOAD64L
#define LOAD64L( cpu64, ptr ) do {				\
		uint64_t *le64 = ( ( uint64_t * ) ptr );	\
		cpu64 = le64_to_cpu ( *le64 );			\
	} while ( 0 )

#undef LOAD64H
#define LOAD64H( cpu64, ptr ) do {				\
		uint64_t *be64 = ( ( uint64_t * ) ptr );	\
		cpu64 = be64_to_cpu ( *be64 );			\
	} while ( 0 )

#undef STORE32L
#define STORE32L( cpu32, ptr ) do {				\
		uint32_t *le32 = ( ( uint32_t * ) ptr );	\
		*le32 = cpu_to_le32 ( cpu32 );			\
	} while ( 0 )

#undef STORE32H
#define STORE32H( cpu32, ptr ) do {				\
		uint32_t *be32 = ( ( uint32_t * ) ptr );	\
		*be32 = cpu_to_be32 ( cpu32 );			\
	} while ( 0 )

#undef STORE64L
#define STORE64L( cpu64, ptr ) do {				\
		uint64_t *le64 = ( ( uint64_t * ) ptr );	\
		*le64 = cpu_to_le64 ( cpu64 );			\
	} while ( 0 )

#undef STORE64H
#define STORE64H( cpu64, ptr ) do {				\
		uint64_t *be64 = ( ( uint64_t * ) ptr );	\
		*be64 = cpu_to_be64 ( cpu64 );			\
	} while ( 0 )

/* Use rolXX etc. from bitops.h */

#undef ROL
#define ROL( data, rotation )	 rol32 ( (data), (rotation) )
#undef ROLc
#define ROLc( data, rotation )	 rol32 ( (data), (rotation) )
#undef ROR
#define ROR( data, rotation )	 ror32 ( (data), (rotation) )
#undef RORc
#define RORc( data, rotation )	 ror32 ( (data), (rotation) )
#undef ROL64
#define ROL64( data, rotation )	 rol64 ( (data), (rotation) )
#undef ROL64c
#define ROL64c( data, rotation ) rol64 ( (data), (rotation) )
#undef ROR64
#define ROR64( data, rotation )	 ror64 ( (data), (rotation) )
#undef ROR64c
#define ROR64c( data, rotation ) ror64 ( (data), (rotation) )

#endif /* _MATRIXSSL_CRYPTOLAYER_H */
