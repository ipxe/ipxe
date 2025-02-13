/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Weierstrass elliptic curves
 *
 * The implementation is based upon Algorithm 1 from "Complete
 * addition formulas for prime order elliptic curves" (Joost Renes,
 * Craig Costello, and Lejla Batina), available from
 *
 *   https://www.microsoft.com/en-us/research/wp-content/uploads/2016/06/complete-2.pdf
 *
 * The steps within the algorithm have been reordered and temporary
 * variables shuffled to reduce stack usage, and calculations are
 * carried out modulo small multiples of the field prime in order to
 * elide reductions after intermediate addition and subtraction
 * operations.
 *
 * The algorithm is encoded using a bytecode representation, since
 * this substantially reduces the code size compared to direct
 * implementation of the big integer operations.
 */

#include <errno.h>
#include <ipxe/weierstrass.h>

/** Big integer register names */
enum weierstrass_register {

	/*
	 * Read-only registers
	 */

	/* Curve constant "a" (for multiply), zero (for add/subtract) */
	WEIERSTRASS_a = 0,
	/* Curve constant "3b" */
	WEIERSTRASS_3b,
	/* Augend (x,y,z) co-ordinates */
	WEIERSTRASS_x1,
	WEIERSTRASS_y1,
	WEIERSTRASS_z1,
	/* Addend (x,y,z) co-ordinates */
	WEIERSTRASS_x2,
	WEIERSTRASS_y2,
	WEIERSTRASS_z2,

	/*
	 * Read-write registers
	 */

	/* Temporary working registers */
	WEIERSTRASS_Wt,
	WEIERSTRASS_Wxy,
	WEIERSTRASS_Wyz,
	WEIERSTRASS_Wzx,
	/* Low half of multiplication product */
	WEIERSTRASS_Wp,
	/* Result (x,y,z) co-ordinates */
	WEIERSTRASS_x3,
	WEIERSTRASS_y3,
	WEIERSTRASS_z3,

	/* Number of registers */
	WEIERSTRASS_NUM_REGISTERS = 16
};

/** Zero register (for add/subtract operations */
#define WEIERSTRASS_zero WEIERSTRASS_a

/** Construct big integer register index */
#define WEIERSTRASS_REGISTER( name ) _C2 ( WEIERSTRASS_, name )

/** Bytecode operation codes */
enum weierstrass_opcode {
	/** Subtract big integers (and add nothing)*/
	WEIERSTRASS_OP_SUB_0N = 0,
	/** Subtract big integers (and add 2N) */
	WEIERSTRASS_OP_SUB_2N = WEIERSTRASS_2N,
	/** Subtract big integers (and add 4N) */
	WEIERSTRASS_OP_SUB_4N = WEIERSTRASS_4N,
	/** Add big integers */
	WEIERSTRASS_OP_ADD,
	/** Multiply big integers (and perform Montgomery reduction) */
	WEIERSTRASS_OP_MUL,
};

/**
 * Define a bytecode operation
 *
 * @v opcode		Operation code
 * @v dest		Destination big integer register name
 * @v left		Left source big integer register name
 * @v right		Right source big integer register name
 */
#define WEIERSTRASS_OP( opcode, dest, left, right )	\
	( ( (opcode) << 12 ) |				\
	  ( WEIERSTRASS_REGISTER ( dest ) << 8 ) |	\
	  ( WEIERSTRASS_REGISTER ( left ) << 4 ) |	\
	  ( WEIERSTRASS_REGISTER ( right ) << 0 ) )

/** Extract bytecode operation code */
#define WEIERSTRASS_OPCODE( op ) ( ( (op) >> 12 ) & 0xf )

/** Extract destination big integer register */
#define WEIERSTRASS_DEST( op ) ( ( (op) >> 8 ) & 0xf )

/** Extract left source big integer register */
#define WEIERSTRASS_LEFT( op ) ( ( (op) >> 4 ) & 0xf )

/** Extract right source big integer register */
#define WEIERSTRASS_RIGHT( op ) ( ( (op) >> 0 ) & 0xf )

/** Define a three-argument addition operation */
#define WEIERSTRASS_ADD3( dest, augend, addend ) \
	WEIERSTRASS_OP ( WEIERSTRASS_OP_ADD, dest, augend, addend )

/** Define a two-argument addition operation */
#define WEIERSTRASS_ADD2( augend, addend ) \
	WEIERSTRASS_ADD3 ( augend, augend, addend )

/** Define a move operation */
#define WEIERSTRASS_MOV( dest, source ) \
	WEIERSTRASS_ADD3( dest, source, zero )

/** Define a three-argument subtraction operation */
#define WEIERSTRASS_SUB3( dest, minuend, subtrahend, multiple ) \
	WEIERSTRASS_OP ( _C2 ( WEIERSTRASS_OP_SUB_, multiple ),	  \
			 dest, minuend, subtrahend )

/** Define a two-argument subtraction operation */
#define WEIERSTRASS_SUB2( minuend, subtrahend, multiple ) \
	WEIERSTRASS_SUB3 ( minuend, minuend, subtrahend, multiple )

/** Define a stop operation */
#define WEIERSTRASS_STOP WEIERSTRASS_SUB2 ( zero, zero, 0N )

/** Define a three-argument multiplication operation */
#define WEIERSTRASS_MUL3( dest, multiplicand, multiplier ) \
	WEIERSTRASS_OP ( WEIERSTRASS_OP_MUL, dest, multiplicand, multiplier )

/** Define a two-argument multiplication operation */
#define WEIERSTRASS_MUL2( multiplicand, multiplier ) \
	WEIERSTRASS_MUL3 ( multiplicand, multiplicand, multiplier )

/**
 * Initialise curve
 *
 * @v curve		Weierstrass curve
 */
static void weierstrass_init ( struct weierstrass_curve *curve ) {
	unsigned int size = curve->size;
	bigint_t ( size ) __attribute__ (( may_alias )) *prime =
		( ( void * ) curve->prime[0] );
	bigint_t ( size ) __attribute__ (( may_alias )) *fermat =
		( ( void * ) curve->fermat );
	bigint_t ( size ) __attribute__ (( may_alias )) *square =
		( ( void * ) curve->square );
	bigint_t ( size ) __attribute__ (( may_alias )) *one =
		( ( void * ) curve->one );
	bigint_t ( size ) __attribute__ (( may_alias )) *a =
		( ( void * ) curve->a );
	bigint_t ( size ) __attribute__ (( may_alias )) *b3 =
		( ( void * ) curve->b3 );
	bigint_t ( size ) __attribute__ (( may_alias )) *mont =
		( ( void * ) curve->mont[0] );
	bigint_t ( size ) __attribute__ (( may_alias )) *temp =
		( ( void * ) curve->prime[1] );
	bigint_t ( size * 2 ) __attribute__ (( may_alias )) *product =
		( ( void * ) temp );
	bigint_t ( size ) __attribute__ (( may_alias )) *two =
		( ( void * ) temp );
	static const uint8_t one_raw[] = { 1 };
	static const uint8_t two_raw[] = { 2 };
	size_t len = curve->len;
	unsigned int i;

	/* Initialise field prime */
	bigint_init ( prime, curve->prime_raw, len );
	DBGC ( curve, "WEIERSTRASS %s   N = %s\n",
	       curve->name, bigint_ntoa ( prime ) );

	/* Calculate Montgomery constant R^2 mod N */
	bigint_reduce ( prime, square );
	DBGC ( curve, "WEIERSTRASS %s R^2 = %s mod N\n",
	       curve->name, bigint_ntoa ( square ) );

	/* Calculate constant "3b" */
	bigint_init ( b3, curve->b_raw, len );
	DBGC ( curve, "WEIERSTRASS %s   b = %s\n",
	       curve->name, bigint_ntoa ( b3 ) );
	bigint_copy ( b3, a );
	bigint_add ( b3, b3 );
	bigint_add ( a, b3 );

	/* Initialise "a" */
	bigint_init ( a, curve->a_raw, len );
	DBGC ( curve, "WEIERSTRASS %s   a = %s\n",
	       curve->name, bigint_ntoa ( a ) );

	/* Initialise "1" */
	bigint_init ( one, one_raw, sizeof ( one_raw ) );

	/* Convert relevant constants to Montgomery form
	 *
	 * We rely on the fact that the prime multiples have not yet
	 * been calculated, and so can be used as a temporary buffer.
	 */
	for ( i = 0 ; i < WEIERSTRASS_NUM_MONT ; i++ ) {
		static const char *names[] = { "  ", " a", "3b" };
		bigint_multiply ( &mont[i], square, product );
		bigint_montgomery ( prime, product, &mont[i] );
		DBGC ( curve, "WEIERSTRASS %s %sR = %s mod N\n",
		       curve->name, names[i], bigint_ntoa ( &mont[i] ) );
	}

	/* Calculate constant "N-2"
	 *
	 * We rely on the fact that the prime multiples have not yet
	 * been calculated, and so can be used as a temporary buffer.
	 */
	bigint_copy ( prime, fermat );
	bigint_init ( two, two_raw, sizeof ( two_raw ) );
	bigint_subtract ( two, fermat );
	DBGC ( curve, "WEIERSTRASS %s N-2 = %s\n",
	       curve->name, bigint_ntoa ( fermat ) );

	/* Calculate multiples of field prime */
	for ( i = 1 ; i < WEIERSTRASS_NUM_MULTIPLES ; i++ ) {
		bigint_copy ( &prime[ i - 1 ], &prime[i] );
		bigint_add ( &prime[i], &prime[i] );
		DBGC ( curve, "WEIERSTRASS %s  %dN = %s\n",
		       curve->name, ( 1 << i ), bigint_ntoa ( &prime[i] ) );
	}
}

/**
 * Execute bytecode instruction
 *
 * @v curve		Weierstrass curve
 * @v regs		Registers
 * @v size		Big integer size
 * @v op		Operation
 */
static void weierstrass_exec ( const struct weierstrass_curve *curve,
			       void **regs, unsigned int size,
			       unsigned int op ) {
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*prime = ( ( const void * ) curve->prime[0] );
	bigint_t ( size * 2 ) __attribute__ (( may_alias ))
		*product = regs[WEIERSTRASS_Wp];
	bigint_t ( size ) __attribute__ (( may_alias )) *dest;
	const bigint_t ( size ) __attribute__ (( may_alias )) *left;
	const bigint_t ( size ) __attribute__ (( may_alias )) *right;
	const bigint_t ( size ) __attribute__ (( may_alias )) *addend;
	const bigint_t ( size ) __attribute__ (( may_alias )) *subtrahend;
	unsigned int op_code;
	unsigned int op_dest;
	unsigned int op_left;
	unsigned int op_right;

	/* Decode instruction */
	op_code = WEIERSTRASS_OPCODE ( op );
	op_dest = WEIERSTRASS_DEST ( op );
	op_left = WEIERSTRASS_LEFT ( op );
	op_right = WEIERSTRASS_RIGHT ( op );
	dest = regs[op_dest];
	left = regs[op_left];
	right = regs[op_right];

	/* Check destination is a writable register */
	assert ( op_dest >= WEIERSTRASS_Wt );

	/* Handle multiplications */
	if ( op_code == WEIERSTRASS_OP_MUL ) {
		assert ( op_left != WEIERSTRASS_Wp );
		assert ( op_right != WEIERSTRASS_Wp );
		bigint_multiply ( left, right, product );
		bigint_montgomery_relaxed ( prime, product, dest );
		DBGCP ( curve, "WEIERSTRASS %s R%d := R%d x R%d = %s\n",
			curve->name, op_dest, op_left, op_right,
			bigint_ntoa ( dest ) );
		return;
	}

	/* Copy left source, if required */
	if ( op_dest != op_left )
		bigint_copy ( left, dest );

	/* Do nothing more if addend/subtrahend is zero */
	if ( ! op_right ) {
		DBGCP ( curve, "WEIERSTRASS %s R%d := R%d = %s\n",
			curve->name, op_dest, op_left, bigint_ntoa ( dest ) );
		return;
	}

	/* Determine addend and subtrahend */
	addend = NULL;
	subtrahend = NULL;
	if ( op_code == WEIERSTRASS_OP_ADD ) {
		DBGCP ( curve, "WEIERSTRASS %s R%d := R%d + R%d = ",
			curve->name, op_dest, op_left, op_right );
		addend = ( ( const void * ) right );
	} else {
		subtrahend = ( ( const void * ) right );
		if ( op_code > WEIERSTRASS_OP_SUB_0N ) {
			DBGCP ( curve, "WEIERSTRASS %s R%d := R%d - R%d + "
				"%dN = ", curve->name, op_dest, op_left,
				op_right, ( 1 << op_code ) );
			addend = ( ( const void * ) curve->prime[op_code] );
		} else {
			DBGCP ( curve, "WEIERSTRASS %s R%d := R%d - R%d = ",
				curve->name, op_dest, op_left, op_right );
		}
	}

	/* Perform addition and subtraction */
	if ( addend )
		bigint_add ( addend, dest );
	if ( subtrahend )
		bigint_subtract ( subtrahend, dest );
	DBGCP ( curve, "%s\n", bigint_ntoa ( dest ) );
}

/**
 * Add points on curve
 *
 * @v curve		Weierstrass curve
 * @v augend0		Element 0 of point (x1,y1,z1) to be added
 * @v addend0		Element 0 of point (x2,y2,z2) to be added
 * @v result0		Element 0 of point (x3,y3,z3) to hold result
 *
 * Points are represented in projective coordinates, with all values
 * in Montgomery form and in the range [0,4N) where N is the field
 * prime.
 *
 * The augend may have the same value as the addend (i.e. this routine
 * may be used to perform point doubling as well as point addition),
 * and either or both may be the point at infinity.
 *
 * The result may overlap either input, since the inputs are fully
 * consumed before the result is written.
 */
static void weierstrass_add_raw ( const struct weierstrass_curve *curve,
				  const bigint_element_t *augend0,
				  const bigint_element_t *addend0,
				  bigint_element_t *result0 ) {
	unsigned int size = curve->size;
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*prime = ( ( const void * ) curve->prime[0] );
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*a = ( ( const void * ) curve->a );
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*b3 = ( ( const void * ) curve->b3 );
	const weierstrass_t ( size ) __attribute__ (( may_alias ))
		*augend = ( ( const void * ) augend0 );
	const weierstrass_t ( size ) __attribute__ (( may_alias ))
		*addend = ( ( const void * ) addend0 );
	weierstrass_t ( size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );
	struct {
		bigint_t ( size ) Wt;
		bigint_t ( size ) Wxy;
		bigint_t ( size ) Wyz;
		bigint_t ( size ) Wzx;
		bigint_t ( size * 2 ) Wp;
	} temp;
	void *regs[WEIERSTRASS_NUM_REGISTERS];
	unsigned int schedule;
	const uint16_t *op;
	unsigned int i;

	/* On entry, we assume that x1, x2, y1, y2, z1, z2 are all in
	 * the range [0,4N).  Additions will extend the range.
	 * Subtractions will extend the range (and require an addition
	 * of a suitable multiple of the modulus to ensure that the
	 * result is a positive value).  Relaxed Montgomery
	 * multiplications will reduce the range to [0,2N).  The
	 * outputs x3, y3, z3 will be in the range [0,4N) and
	 * therefore usable as subsequent inputs.
	 */
	static const uint16_t ops[] = {
		/* [Wxy] Qxy = (x1+y1)*(x2+y2)			    (mod 2N) */
		WEIERSTRASS_ADD3 ( Wt, x1, y1 ),
		WEIERSTRASS_ADD3 ( Wxy, x2, y2 ),
		WEIERSTRASS_MUL2 ( Wxy, Wt ),
		/* [Wyz] Qyz = (y1+z1)*(y2+z2)			    (mod 2N) */
		WEIERSTRASS_ADD3 ( Wt, y1, z1 ),
		WEIERSTRASS_ADD3 ( Wyz, y2, z2 ),
		WEIERSTRASS_MUL2 ( Wyz, Wt ),
		/* [Wzx] Qzx = (z1+x1)*(z2+x2)			    (mod 2N) */
		WEIERSTRASS_ADD3 ( Wt, z1, x1 ),
		WEIERSTRASS_ADD3 ( Wzx, z2, x2 ),
		WEIERSTRASS_MUL2 ( Wzx, Wt ),
		/* [x3] Px = x1*x2				    (mod 2N) */
		WEIERSTRASS_MUL3 ( x3, x1, x2 ),
		/* [y3] Py = y1*y2				    (mod 2N) */
		WEIERSTRASS_MUL3 ( y3, y1, y2 ),
		/* [z3] Pz = z1*z2				    (mod 2N) */
		WEIERSTRASS_MUL3 ( z3, z1, z2 ),
		/* [Wxy] Rxy = Qxy - Px - Py			    (mod 6N)
		 *	     = (x1+y1)*(x2+y2) - x1*x2 - y1*y2	    (mod 6N)
		 *	     = x1*y2 + x2*y1			    (mod 6N)
		 */
		WEIERSTRASS_SUB2 ( Wxy, x3, 0N ),
		WEIERSTRASS_SUB2 ( Wxy, y3, 4N ),
		/* [Wyz] Ryz = Qyz - Py - Pz			    (mod 6N)
		 *	     = (y1+z1)*(y2+z2) - y1*y2 - z1*z2	    (mod 6N)
		 *	     = y1*z2 + y2*z1			    (mod 6N)
		 */
		WEIERSTRASS_SUB2 ( Wyz, y3, 0N ),
		WEIERSTRASS_SUB2 ( Wyz, z3, 4N ),
		/* [Wzx] Rzx = Qzx - Pz - Px			    (mod 6N)
		 *	     = (z1+x1)*(z2+x2) - z1*z2 - x1*x2	    (mod 6N)
		 *	     = x1*z2 + x2*z1			    (mod 6N)
		 */
		WEIERSTRASS_SUB2 ( Wzx, z3, 0N ),
		WEIERSTRASS_SUB2 ( Wzx, x3, 4N ),
		/* [Wt] aRzx = a * Rzx				    (mod 2N)
		 *	     = a * (x1*z2 + x2*z1)		    (mod 2N)
		 */
		WEIERSTRASS_MUL3 ( Wt, a, Wzx ),
		/* [Wp] 3bPz = 3b * Pz				    (mod 2N)
		 *	     = 3b*z1*z2				    (mod 2N)
		 */
		WEIERSTRASS_MUL3 ( Wp, 3b, z3 ),
		/* [Wp] Sy = aRzx + 3bPz			    (mod 4N)
		 *	   = a*(x1*z2 + x2*z1) + 3b*z1*z2	    (mod 4N)
		 */
		WEIERSTRASS_ADD2 ( Wp, Wt ),
		/* [Wt] Syz = Py + Sy				    (mod 6N)
		 *	    = y1*y2 + a*(x1*z2 + x2*z1) + 3b*z1*z2  (mod 6N)
		 */
		WEIERSTRASS_ADD3 ( Wt, y3, Wp ),
		/* [y3] Sxy = Py - Sy				    (mod 6N)
		 *	    = y1*y2 - a*(x1*z2 + x2*z1) - 3b*z1*z2  (mod 6N)
		 */
		WEIERSTRASS_SUB2 ( y3, Wp, 4N ),
		/* [z3] aPz = a * Pz				    (mod 2N)
		 *	    = a * z1*z2				    (mod 2N)
		 */
		WEIERSTRASS_MUL2 ( z3, a ),
		/* [Wzx] 3bRzx = 3b * Rzx			    (mod 2N)
		 *	       = 3b * (x1*z2 + x2*z1)		    (mod 2N)
		 */
		WEIERSTRASS_MUL2 ( Wzx, 3b ),
		/* [x3] aPzx' = Px - aPz			    (mod 4N)
		 *	      = x1*x2 - a*z1*z2			    (mod 4N)
		 */
		WEIERSTRASS_SUB2 ( x3, z3, 2N ),
		/* [Wp] Szx = a * aPzx'				    (mod 2N)
		 *	    = a * (x1*x2 - a*z1*z2)		    (mod 2N)
		 * 	    = a*x1*x2 - (a^2)*z1*z2		    (mod 2N)
		 */
		WEIERSTRASS_MUL3 ( Wp, a, x3 ),
		/* [x3] Px = aPzx' + aPz			    (mod 6N)
		 *	   = x1*x2 - a*z1*z2 + a*z1*z2		    (mod 6N)
		 *	   = x1*x2				    (mod 6N)
		 */
		WEIERSTRASS_ADD2 ( x3, z3 ),
		/* [Wzx] Tzx = 3bRzx + Szx			    (mod 4N)
		 *	     = a*x1*x2 + 3b*(x1*z2 + x2*z1) -
		 *	       (a^2)*z1*z2			    (mod 4N)
		 */
		WEIERSTRASS_ADD2 ( Wzx, Wp ),
		/* [z3] aPzx = Px + aPz				    (mod 8N)
		 *	     = x1*x2 + a*z1*z2			    (mod 8N)
		 */
		WEIERSTRASS_ADD2 ( z3, x3 ),
		/* [x3] 2Px = Px + Px				    (mod 12N)
		 *          = 2*x1*x2				    (mod 12N)
		 */
		WEIERSTRASS_ADD2 ( x3, x3 ),
		/* [x3] Tyz = 2Px + aPzx			    (mod 20N)
		 *          = 2*x1*x2 + x1*x2 + a*z1*z2		    (mod 20N)
		 *	    = 3*x1*x2 + a*z1*z2			    (mod 20N)
		 */
		WEIERSTRASS_ADD2 ( x3, z3 ),
		/* [z3] Syz = Syz				    (mod 6N)
		 *	    = y1*y2 + a*(x1*z2 + x2*z1) + 3b*z1*z2  (mod 6N)
		 */
		WEIERSTRASS_MOV ( z3, Wt ),
		/* [Wt] Tyz = Tyz				    (mod 20N)
		 *	    = 3*x1*x2 + a*z1*z2			    (mod 20N)
		 */
		WEIERSTRASS_MOV ( Wt, x3 ),
		/* [x3] Ux = Rxy * Sxy				    (mod 2N)
		 *	   = (x1*y2 + x2*y1) *
		 *	     (y1*y2 - a*(x1*z2 + x2*z1) - 3b*z1*z2) (mod 2N)
		 */
		WEIERSTRASS_MUL3 ( x3, Wxy, y3 ),
		/* [y3] Uy = Syz * Sxy				    (mod 2N)
		 *	   = (y1*y2 + a*(x1*z2 + x2*z1) + 3b*z1*z2) *
		 *	     (y1*y2 - a*(x1*z2 + x2*z1) - 3b*z1*z2) (mod 2N)
		 */
		WEIERSTRASS_MUL2 ( y3, z3 ),
		/* [z3] Uz = Ryz * Syz				    (mod 2N)
		 *	   = (y1*z2 + y2*z1) *
		 *	     (y1*y2 + a*(x1*z2 + x2*z1) + 3b*z1*z2) (mod 2N)
		 */
		WEIERSTRASS_MUL2 ( z3, Wyz ),
		/* [Wp] Vx = Ryz * Tzx				    (mod 2N)
		 *	   = (y1*z2 + y2*z1) *
		 *	     (a*x1*x2 + 3b*(x1*z2 + x2*z1) - (a^2)*z1*z2)
		 *						    (mod 2N)
		 */
		WEIERSTRASS_MUL3 ( Wp, Wyz, Wzx ),
		/* [x3] x3 = Ux - Vx				    (mod 4N)
		 *	   = ((x1*y2 + x2*y1) *
		 *	      (y1*y2 - a*(x1*z2 + x2*z1) - 3b*z1*z2)) -
		 *	     ((y1*z2 + y2*z1) *
		 *	      (a*x1*x2 + 3b*(x1*z2 + x2*z1) - (a^2)*z1*z2))
		 *						    (mod 4N)
		 */
		WEIERSTRASS_SUB2 ( x3, Wp, 2N ),
		/* [Wp] Vy = Tyz * Tzx				    (mod 2N)
		 *	   = (3*x1*x2 + a*z1*z2) *
		 *	     (a*x1*x2 + 3b*(x1*z2 + x2*z1) - (a^2)*z1*z2)
		 *						    (mod 2N)
		 */
		WEIERSTRASS_MUL3 ( Wp, Wt, Wzx ),
		/* [y3] y3 = Vy + Uy				    (mod 4N)
		 *         = ((3*x1*x2 + a*z1*z2) *
		 *	      (a*x1*x2 + 3b*(x1*z2 + x2*z1) - (a^2)*z1*z2)) +
		 *	     ((y1*y2 + a*(x1*z2 + x2*z1) + 3b*z1*z2) *
		 *	      (y1*y2 - a*(x1*z2 + x2*z1) - 3b*z1*z2))
		 *						    (mod 4N)
		 */
		WEIERSTRASS_ADD2 ( y3, Wp ),
		/* [Wp] Vz = Rxy * Tyz				    (mod 2N)
		 *	   = (x1*y2 + x2*y1) * (3*x1*x2 + a*z1*z2)  (mod 2N)
		 */
		WEIERSTRASS_MUL3 ( Wp, Wxy, Wt ),
		/* [z3] z3 = Uz + Vz				    (mod 4N)
		 *	   = ((y1*z2 + y2*z1) *
		 *	      (y1*y2 + a*(x1*z2 + x2*z1) + 3b*z1*z2)) +
		 *	     ((x1*y2 + x2*y1) * (3*x1*x2 + a*z1*z2))
		 *						    (mod 4N)
		 */
		WEIERSTRASS_ADD2 ( z3, Wp ),
		/* Stop */
		WEIERSTRASS_STOP
	};

	/* Initialise register list */
	regs[WEIERSTRASS_a] = ( ( void * ) a );
	regs[WEIERSTRASS_3b] = ( ( void * ) b3 );
	regs[WEIERSTRASS_x1] = ( ( void * ) &augend->x );
	regs[WEIERSTRASS_x2] = ( ( void * ) &addend->x );
	regs[WEIERSTRASS_x3] = ( ( void * ) &result->x );
	regs[WEIERSTRASS_Wt] = &temp;
	schedule = ( ( ( 1 << WEIERSTRASS_NUM_REGISTERS ) - 1 )
		     - ( 1 << WEIERSTRASS_a )
		     - ( 1 << WEIERSTRASS_3b )
		     - ( 1 << WEIERSTRASS_x1 )
		     - ( 1 << WEIERSTRASS_x2 )
		     - ( 1 << WEIERSTRASS_x3 )
		     - ( 1 << WEIERSTRASS_Wt ) );
	for ( i = 0 ; schedule ; i++, schedule >>= 1 ) {
		if ( schedule & 1 )
			regs[i] = ( regs[ i - 1 ] + sizeof ( *prime ) );
	}
	DBGC2 ( curve, "WEIERSTRASS %s augend (%s,",
		curve->name, bigint_ntoa ( &augend->x ) );
	DBGC2 ( curve, "%s,", bigint_ntoa ( &augend->y ) );
	DBGC2 ( curve, "%s)\n", bigint_ntoa ( &augend->z ) );
	DBGC2 ( curve, "WEIERSTRASS %s addend (%s,",
		curve->name, bigint_ntoa ( &addend->x ) );
	DBGC2 ( curve, "%s,", bigint_ntoa ( &addend->y ) );
	DBGC2 ( curve, "%s)\n", bigint_ntoa ( &addend->z ) );

	/* Sanity checks */
	assert ( regs[WEIERSTRASS_a] == a );
	assert ( regs[WEIERSTRASS_3b] == b3 );
	assert ( regs[WEIERSTRASS_x1] == &augend->x );
	assert ( regs[WEIERSTRASS_y1] == &augend->y );
	assert ( regs[WEIERSTRASS_z1] == &augend->z );
	assert ( regs[WEIERSTRASS_x2] == &addend->x );
	assert ( regs[WEIERSTRASS_y2] == &addend->y );
	assert ( regs[WEIERSTRASS_z2] == &addend->z );
	assert ( regs[WEIERSTRASS_x3] == &result->x );
	assert ( regs[WEIERSTRASS_y3] == &result->y );
	assert ( regs[WEIERSTRASS_z3] == &result->z );
	assert ( regs[WEIERSTRASS_Wt] == &temp.Wt );
	assert ( regs[WEIERSTRASS_Wxy] == &temp.Wxy );
	assert ( regs[WEIERSTRASS_Wyz] == &temp.Wyz );
	assert ( regs[WEIERSTRASS_Wzx] == &temp.Wzx );
	assert ( regs[WEIERSTRASS_Wp] == &temp.Wp );

	/* Execute bytecode instruction sequence */
	for ( op = ops ; *op != WEIERSTRASS_STOP ; op++ )
		weierstrass_exec ( curve, regs, size, *op );
	DBGC2 ( curve, "WEIERSTRASS %s result (%s,",
		curve->name, bigint_ntoa ( &result->x ) );
	DBGC2 ( curve, "%s,", bigint_ntoa ( &result->y ) );
	DBGC2 ( curve, "%s)\n", bigint_ntoa ( &result->z ) );
}

/**
 * Add points on curve
 *
 * @v curve		Weierstrass curve
 * @v augend		Point (x1,y1,z1) to be added
 * @v addend		Point (x2,y2,z2) to be added
 * @v result0		Point (x3,y3,z3) to hold result
 */
#define weierstrass_add( curve, augend, addend, result ) do {		\
	weierstrass_add_raw ( (curve), (augend)->all.element,		\
			      (addend)->all.element,			\
			      (result)->all.element );			\
	} while ( 0 )

/**
 * Add points on curve as part of a Montgomery ladder
 *
 * @v operand		Element 0 of first input operand (may overlap result)
 * @v result		Element 0 of second input operand and result
 * @v size		Number of elements in operands and result
 * @v ctx		Operation context
 * @v tmp		Temporary working space (not used)
 */
static void weierstrass_add_ladder ( const bigint_element_t *operand0,
				     bigint_element_t *result0,
				     unsigned int size, const void *ctx,
				     void *tmp __unused ) {
	const struct weierstrass_curve *curve = ctx;
	const weierstrass_t ( curve->size ) __attribute__ (( may_alias ))
		*operand = ( ( const void * ) operand0 );
	weierstrass_t ( curve->size ) __attribute__ (( may_alias ))
		*result = ( ( void * ) result0 );

	/* Add curve points */
	assert ( size == bigint_size ( &operand->all ) );
	assert ( size == bigint_size ( &result->all ) );
	weierstrass_add ( curve, operand, result, result );
}

/**
 * Verify point is on curve
 *
 * @v curve		Weierstrass curve
 * @v point0		Element 0 of point (x,y,z) to be verified
 * @ret rc		Return status code
 *
 * As with point addition, points are represented in projective
 * coordinates, with all values in Montgomery form and in the range
 * [0,4N) where N is the field prime.
 */
static int weierstrass_verify_raw ( const struct weierstrass_curve *curve,
				    const bigint_element_t *point0 ) {
	unsigned int size = curve->size;
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*prime = ( ( const void * ) curve->prime[0] );
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*a = ( ( const void * ) curve->a );
	const bigint_t ( size ) __attribute__ (( may_alias ))
		*b3 = ( ( const void * ) curve->b3 );
	const weierstrass_t ( size ) __attribute__ (( may_alias ))
		*point = ( ( const void * ) point0 );
	struct {
		bigint_t ( size ) Wt;
		bigint_t ( size * 2 ) Wp;
	} temp;
	void *regs[WEIERSTRASS_NUM_REGISTERS];
	const uint16_t *op;

	/* Calculate 3*(x^3 + a*x + b - y^2) */
	static const uint16_t ops[] = {
		/* [Wt] Tx = x^2				    (mod 2N) */
		WEIERSTRASS_MUL3 ( Wt, x1, x1 ),
		/* [Wt] Txa = Tx + a				    (mod 3N)
		 *	    = x^2 + a				    (mod 3N)
		 */
		WEIERSTRASS_MOV ( Wp, a ),
		WEIERSTRASS_ADD2 ( Wt, Wp ),
		/* [Wt] Txax = Txa * x				    (mod 2N)
		 *	     = (x^2 + a)*x			    (mod 2N)
		 *	     = x^3 + a*x			    (mod 2N)
		 */
		WEIERSTRASS_MUL2 ( Wt, x1 ),
		/* [Wp] Ty = y^2				    (mod 2N) */
		WEIERSTRASS_MUL3 ( Wp, y1, y1 ),
		/* [Wt] Txaxy = Txax - Ty			    (mod 4N)
		 *	      = x^3 + a*x - y^2			    (mod 4N)
		 */
		WEIERSTRASS_SUB2 ( Wt, Wp, 2N ),
		/* [Wp] 2Txaxy = Txaxy + Txaxy			    (mod 8N)
		 *	       = 2*(x^3 + a*x - y^2)		    (mod 8N)
		 */
		WEIERSTRASS_ADD3 ( Wp, Wt, Wt ),
		/* [Wt] 3Txaxy = 2Txaxy + Txaxy			    (mod 12N)
		 *	       = 3*(x^3 + a*x - y^2)		    (mod 12N)
		 */
		WEIERSTRASS_ADD2 ( Wt, Wp ),
		/* [Wt] 3Txaxyb = 3Txaxy + 3b			    (mod 13N)
		 *		= 3*(x^3 + a*x - y^2) + 3b	    (mod 13N)
		 *		= 3*(x^3 + a*x + b - y^2)	    (mod 13N)
		 */
		WEIERSTRASS_ADD2 ( Wt, 3b ),
		/* Stop */
		WEIERSTRASS_STOP
	};

	/* Initialise register list */
	regs[WEIERSTRASS_a] = ( ( void * ) a );
	regs[WEIERSTRASS_3b] = ( ( void * ) b3 );
	regs[WEIERSTRASS_x1] = ( ( void * ) &point->x );
	regs[WEIERSTRASS_y1] = ( ( void * ) &point->y );
	regs[WEIERSTRASS_Wt] = &temp.Wt;
	regs[WEIERSTRASS_Wp] = &temp.Wp;

	/* Execute bytecode instruction sequence */
	for ( op = ops ; *op != WEIERSTRASS_STOP ; op++ )
		weierstrass_exec ( curve, regs, size, *op );

	/* Check that result is zero (modulo the field prime) */
	bigint_grow ( &temp.Wt, &temp.Wp );
	bigint_montgomery ( prime, &temp.Wp, &temp.Wt );
	if ( ! bigint_is_zero ( &temp.Wt ) ) {
		DBGC ( curve, "WEIERSTRASS %s base point is not on curve\n",
		       curve->name );
		return -EINVAL;
	}

	return 0;
}

/**
 * Verify point is on curve
 *
 * @v curve		Weierstrass curve
 * @v point		Point (x,y,z) to be verified
 * @ret rc		Return status code
 */
#define weierstrass_verify( curve, point ) ( {				\
	weierstrass_verify_raw ( (curve), (point)->all.element );	\
	} )

/**
 * Multiply curve point by scalar
 *
 * @v curve		Weierstrass curve
 * @v base		Base point (or NULL to use generator)
 * @v scalar		Scalar multiple
 * @v result		Result point to fill in
 * @ret rc		Return status code
 */
int weierstrass_multiply ( struct weierstrass_curve *curve, const void *base,
			   const void *scalar, void *result ) {
	unsigned int size = curve->size;
	size_t len = curve->len;
	const bigint_t ( size ) __attribute__ (( may_alias )) *prime =
		( ( const void * ) curve->prime[0] );
	const bigint_t ( size ) __attribute__ (( may_alias )) *prime2 =
		( ( const void * ) curve->prime[WEIERSTRASS_2N] );
	const bigint_t ( size ) __attribute__ (( may_alias )) *fermat =
		( ( const void * ) curve->fermat );
	const bigint_t ( size ) __attribute__ (( may_alias )) *square =
		( ( const void * ) curve->square );
	const bigint_t ( size ) __attribute__ (( may_alias )) *one =
		( ( const void * ) curve->one );
	struct {
		union {
			weierstrass_t ( size ) result;
			bigint_t ( size * 2 ) product_in;
		};
		union {
			weierstrass_t ( size ) multiple;
			bigint_t ( size * 2 ) product_out;
		};
		bigint_t ( bigint_required_size ( len ) ) scalar;
	} temp;
	size_t offset;
	unsigned int i;
	int rc;

	/* Initialise curve, if not already done
	 *
	 * The least significant element of the field prime must be
	 * odd, and so the least significant element of the
	 * (initialised) first multiple of the field prime must be
	 * non-zero.
	 */
	if ( ! prime2->element[0] )
		weierstrass_init ( curve );

	/* Use generator if applicable */
	if ( ! base )
		base = curve->base;

	/* Convert input to projective coordinates in Montgomery form */
	DBGC ( curve, "WEIERSTRASS %s base (", curve->name );
	for ( i = 0, offset = 0 ; i < WEIERSTRASS_AXES ; i++, offset += len ) {
		bigint_init ( &temp.multiple.axis[i], ( base + offset ), len );
		DBGC ( curve, "%s%s", ( i ? "," : "" ),
		       bigint_ntoa ( &temp.multiple.axis[i] ) );
		bigint_multiply ( &temp.multiple.axis[i], square,
				  &temp.product_in );
		bigint_montgomery_relaxed ( prime, &temp.product_in,
					    &temp.multiple.axis[i] );
	}
	bigint_copy ( one, &temp.multiple.z );
	DBGC ( curve, ")\n" );

	/* Verify point is on curve */
	if ( ( rc = weierstrass_verify ( curve, &temp.multiple ) ) != 0 )
		return rc;

	/* Construct identity element (the point at infinity) */
	memset ( &temp.result, 0, sizeof ( temp.result ) );
	bigint_copy ( one, &temp.result.y );

	/* Initialise scalar */
	bigint_init ( &temp.scalar, scalar, len );
	DBGC ( curve, "WEIERSTRASS %s scalar %s\n",
	       curve->name, bigint_ntoa ( &temp.scalar ) );

	/* Perform multiplication via Montgomery ladder */
	bigint_ladder ( &temp.result.all, &temp.multiple.all, &temp.scalar,
			weierstrass_add_ladder, curve, NULL );

	/* Invert result Z co-ordinate (via Fermat's little theorem) */
	bigint_copy ( one, &temp.multiple.z );
	bigint_ladder ( &temp.multiple.z, &temp.result.z, fermat,
			bigint_mod_exp_ladder, prime, &temp.product_out );

	/* Convert result back to affine co-ordinates */
	DBGC ( curve, "WEIERSTRASS %s result (", curve->name );
	for ( i = 0, offset = 0 ; i < WEIERSTRASS_AXES ; i++, offset += len ) {
		bigint_multiply ( &temp.result.axis[i], &temp.multiple.z,
				  &temp.product_out );
		bigint_montgomery_relaxed ( prime, &temp.product_out,
					    &temp.result.axis[i] );
		bigint_grow ( &temp.result.axis[i], &temp.product_out );
		bigint_montgomery ( prime, &temp.product_out,
				    &temp.result.axis[i] );
		DBGC ( curve, "%s%s", ( i ? "," : "" ),
		       bigint_ntoa ( &temp.result.axis[i] ) );
		bigint_done ( &temp.result.axis[i], ( result + offset ), len );
	}
	DBGC ( curve, ")\n" );

	return 0;
}
