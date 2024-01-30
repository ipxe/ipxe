#ifndef _IPXE_X25519_H
#define _IPXE_X25519_H

/** @file
 *
 * X25519 key exchange
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/bigint.h>
#include <ipxe/crypto.h>

/** X25519 unsigned big integer size
 *
 * X25519 uses the finite field of integers modulo the prime
 * p=2^255-19.  The canonical representations of integers in this
 * field therefore require only 255 bits.
 *
 * For internal calculations we use big integers containing up to 267
 * bits, since this ends up allowing us to avoid some unnecessary (and
 * expensive) intermediate reductions modulo p.
 */
#define X25519_SIZE bigint_required_size ( ( 267 /* bits */ + 7 ) / 8 )

/** An X25519 unsigned big integer used in internal calculations */
typedef bigint_t ( X25519_SIZE ) x25519_t;

/** An X25519 unsigned 258-bit integer
 *
 * This is an unsigned integer N in the finite field of integers
 * modulo the prime p=2^255-19.
 *
 * In this representation, N is encoded as any big integer that is in
 * the same congruence class as N (i.e that has the same value as N
 * modulo p) and that lies within the 258-bit range [0,8p-1].
 *
 * This type can be used as an input for multiplication (but not for
 * addition or subtraction).
 *
 * Addition or subtraction will produce an output of this type.
 */
union x25519_oct258 {
	/** Big integer value */
	x25519_t value;
};

/** An X25519 unsigned 257-bit integer
 *
 * This is an unsigned integer N in the finite field of integers
 * modulo the prime p=2^255-19.
 *
 * In this representation, N is encoded as any big integer that is in
 * the same congruence class as N (i.e that has the same value as N
 * modulo p) and that lies within the 257-bit range [0,4p-1].
 *
 * This type can be used as an input for addition, subtraction, or
 * multiplication.
 *
 * Multiplication will produce an output of this type.
 */
union x25519_quad257 {
	/** Big integer value */
	x25519_t value;
	/** X25519 unsigned 258-bit integer
	 *
	 * Any value in the range [0,4p-1] is automatically also
	 * within the range [0,8p-1] and so may be consumed as an
	 * unsigned 258-bit integer.
	 */
	const union x25519_oct258 oct258;
};

/** An X25519 32-byte value */
struct x25519_value {
	/** Raw value */
	uint8_t raw[32];
};

extern void x25519_multiply ( const union x25519_oct258 *multiplicand,
			      const union x25519_oct258 *multiplier,
			      union x25519_quad257 *result );
extern void x25519_invert ( const union x25519_oct258 *invertend,
			    union x25519_quad257 *result );
extern void x25519_reduce ( union x25519_quad257 *value );
extern int x25519_key ( const struct x25519_value *base,
			const struct x25519_value *scalar,
			struct x25519_value *result );

extern struct elliptic_curve x25519_curve;

#endif /* _IPXE_X25519_H */
