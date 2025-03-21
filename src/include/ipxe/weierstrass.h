#ifndef _IPXE_WEIERSTRASS_H
#define _IPXE_WEIERSTRASS_H

/** @file
 *
 * Weierstrass elliptic curves
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/bigint.h>
#include <ipxe/crypto.h>

/** Number of axes in Weierstrass curve point representation */
#define WEIERSTRASS_AXES 2

/**
 * Maximum multiple of field prime encountered during calculations
 *
 * Calculations are performed using values modulo a small multiple of
 * the field prime, rather than modulo the field prime itself.  This
 * allows explicit reductions after additions, subtractions, and
 * relaxed Montgomery multiplications to be omitted entirely, provided
 * that we keep careful track of the field prime multiple for each
 * intermediate value.
 *
 * Relaxed Montgomery multiplication will produce a result in the
 * range t < (1+m/k)N, where m is this maximum multiple of the field
 * prime, and k is the constant in R > kN representing the leading
 * zero padding in the big integer representation of the field prime.
 * We choose to set k=m so that multiplications will always produce a
 * result in the range t < 2N.
 *
 * This is expressed as the base-two logarithm of the multiple
 * (rounded up), to simplify compile-time calculations.
 */
#define WEIERSTRASS_MAX_MULTIPLE_LOG2 5 /* maximum reached is mod 20N */

/**
 * Determine number of elements in scalar values for a Weierstrass curve
 *
 * @v len		Length of field prime, in bytes
 * @ret size		Number of elements
 */
#define weierstrass_size( len )						\
	bigint_required_size ( (len) +					\
			       ( ( WEIERSTRASS_MAX_MULTIPLE_LOG2 + 7 )	\
				 / 8 ) )

/**
 * Define a Weierstrass projective co-ordinate type
 *
 * @v size		Number of elements in scalar values
 * @ret weierstrass_t	Projective co-ordinate type
 */
#define weierstrass_t( size )						\
	union {								\
		bigint_t ( size ) axis[3];				\
		struct {						\
			bigint_t ( size ) x;				\
			bigint_t ( size ) y;				\
			bigint_t ( size ) z;				\
		};							\
		bigint_t ( size * 3 ) all;				\
	}

/** Indexes for stored multiples of the field prime */
enum weierstrass_multiple {
	WEIERSTRASS_N = 0,
	WEIERSTRASS_2N,
	WEIERSTRASS_4N,
	WEIERSTRASS_NUM_MULTIPLES
};

/** Number of cached in Montgomery form for each Weierstrass curve */
#define WEIERSTRASS_NUM_MONT 3

/** Number of cached big integers for each Weierstrass curve */
#define WEIERSTRASS_NUM_CACHED						\
	( WEIERSTRASS_NUM_MULTIPLES +					\
	  1 /* fermat */ + 1 /* mont */ +				\
	  WEIERSTRASS_NUM_MONT )

/**
 * A Weierstrass elliptic curve
 *
 * This is an elliptic curve y^2 = x^3 + ax + b
 */
struct weierstrass_curve {
	/** Number of elements in scalar values */
	const unsigned int size;
	/** Curve name */
	const char *name;
	/** Length of raw scalar values */
	size_t len;
	/** Field prime */
	const uint8_t *prime_raw;
	/** Constant "a" */
	const uint8_t *a_raw;
	/** Constant "b" */
	const uint8_t *b_raw;
	/** Base point */
	const uint8_t *base;

	/** Cached field prime "N" (and multiples thereof) */
	bigint_element_t *prime[WEIERSTRASS_NUM_CACHED];
	/** Cached constant "N-2" (for Fermat's little theorem) */
	bigint_element_t *fermat;
	/** Cached Montgomery constant (R^2 mod N) */
	bigint_element_t *square;
	/** Cached constants in Montgomery form */
	union {
		struct {
			/** Cached constant "1", in Montgomery form */
			bigint_element_t *one;
			/** Cached constant "a", in Montgomery form */
			bigint_element_t *a;
			/** Cached constant "3b", in Montgomery form */
			bigint_element_t *b3;
		};
		bigint_element_t *mont[WEIERSTRASS_NUM_MONT];
	};
};

extern int weierstrass_multiply ( struct weierstrass_curve *curve,
				  const void *base, const void *scalar,
				  void *result );

/** Define a Weierstrass curve */
#define WEIERSTRASS_CURVE( _name, _curve, _len, _prime, _a, _b, _base )	\
	static bigint_t ( weierstrass_size(_len) )			\
		_name ## _cache[WEIERSTRASS_NUM_CACHED];		\
	static struct weierstrass_curve _name ## _weierstrass = {	\
		.size = weierstrass_size(_len),				\
		.name = #_name,						\
		.len = (_len),						\
		.prime_raw = (_prime),					\
		.a_raw = (_a),						\
		.b_raw = (_b),						\
		.base = (_base),					\
		.prime = {						\
			(_name ## _cache)[0].element,			\
			(_name ## _cache)[1].element,			\
			(_name ## _cache)[2].element,			\
		},							\
		.fermat = (_name ## _cache)[3].element,			\
		.square = (_name ## _cache)[4].element,			\
		.one = (_name ## _cache)[5].element,			\
		.a = (_name ## _cache)[6].element,			\
		.b3 = (_name ## _cache)[7].element,			\
	};								\
	static int _name ## _multiply ( const void *base,		\
					const void *scalar,		\
					void *result ) {		\
		return weierstrass_multiply ( &_name ## _weierstrass,	\
					      base, scalar, result );	\
	}								\
	struct elliptic_curve _curve = {				\
		.name = #_name,						\
		.pointsize = ( WEIERSTRASS_AXES * (_len) ),		\
		.keysize = (_len),					\
		.multiply = _name ## _multiply,				\
	}

#endif /* _IPXE_WEIERSTRASS_H */
