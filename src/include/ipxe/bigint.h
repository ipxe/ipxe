#ifndef _IPXE_BIGINT_H
#define _IPXE_BIGINT_H

/** @file
 *
 * Big integer support
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <assert.h>

/**
 * Define a big-integer type
 *
 * @v size		Number of elements
 * @ret bigint_t	Big integer type
 */
#define bigint_t( size )						\
	struct {							\
		bigint_element_t element[ (size) ];			\
	}

/**
 * Determine number of elements required for a big-integer type
 *
 * @v len		Maximum length of big integer, in bytes
 * @ret size		Number of elements
 */
#define bigint_required_size( len )					\
	( ( (len) + sizeof ( bigint_element_t ) - 1 ) /			\
	  sizeof ( bigint_element_t ) )

/**
 * Determine number of elements in big-integer type
 *
 * @v bigint		Big integer
 * @ret size		Number of elements
 */
#define bigint_size( bigint )						\
	( sizeof ( *(bigint) ) / sizeof ( (bigint)->element[0] ) )

/**
 * Transcribe big integer (for debugging)
 *
 * @v value		Big integer to be transcribed
 * @ret string		Big integer in string form (may be abbreviated)
 */
#define bigint_ntoa( value ) ( {					\
	unsigned int size = bigint_size (value);			\
	bigint_ntoa_raw ( (value)->element, size );			\
	} )

/**
 * Initialise big integer
 *
 * @v value		Big integer to initialise
 * @v data		Raw data
 * @v len		Length of raw data
 */
#define bigint_init( value, data, len ) do {				\
	unsigned int size = bigint_size (value);			\
	assert ( (len) <= ( size * sizeof ( (value)->element[0] ) ) );	\
	bigint_init_raw ( (value)->element, size, (data), (len) );	\
	} while ( 0 )

/**
 * Finalise big integer
 *
 * @v value		Big integer to finalise
 * @v out		Output buffer
 * @v len		Length of output buffer
 */
#define bigint_done( value, out, len ) do {				\
	unsigned int size = bigint_size (value);			\
	bigint_done_raw ( (value)->element, size, (out), (len) );	\
	} while ( 0 )

/**
 * Add big integers
 *
 * @v addend		Big integer to add
 * @v value		Big integer to be added to
 * @ret carry		Carry out
 */
#define bigint_add( addend, value ) ( {					\
	unsigned int size = bigint_size (addend);			\
	bigint_add_raw ( (addend)->element, (value)->element, size );	\
	} )

/**
 * Subtract big integers
 *
 * @v subtrahend	Big integer to subtract
 * @v value		Big integer to be subtracted from
 * @ret borrow		Borrow out
 */
#define bigint_subtract( subtrahend, value ) ( {			\
	unsigned int size = bigint_size (subtrahend);			\
	bigint_subtract_raw ( (subtrahend)->element, (value)->element,	\
			      size );					\
	} )

/**
 * Shift big integer left
 *
 * @v value		Big integer
 * @ret out		Bit shifted out
 */
#define bigint_shl( value ) ( {						\
	unsigned int size = bigint_size (value);			\
	bigint_shl_raw ( (value)->element, size );			\
	} )

/**
 * Shift big integer right
 *
 * @v value		Big integer
 * @ret out		Bit shifted out
 */
#define bigint_shr( value ) ( {						\
	unsigned int size = bigint_size (value);			\
	bigint_shr_raw ( (value)->element, size );			\
	} )

/**
 * Test if big integer is equal to zero
 *
 * @v value		Big integer
 * @v size		Number of elements
 * @ret is_zero		Big integer is equal to zero
 */
#define bigint_is_zero( value ) ( {					\
	unsigned int size = bigint_size (value);			\
	bigint_is_zero_raw ( (value)->element, size ); } )

/**
 * Compare big integers
 *
 * @v value		Big integer
 * @v reference		Reference big integer
 * @ret geq		Big integer is greater than or equal to the reference
 */
#define bigint_is_geq( value, reference ) ( {				\
	unsigned int size = bigint_size (value);			\
	bigint_is_geq_raw ( (value)->element, (reference)->element,	\
			    size ); } )

/**
 * Set bit in big integer
 *
 * @v value		Big integer
 * @v bit		Bit to set
 */
#define bigint_set_bit( value, bit ) do {				\
	unsigned int size = bigint_size (value);			\
	bigint_set_bit_raw ( (value)->element, size, bit );		\
	} while ( 0 )

/**
 * Clear bit in big integer
 *
 * @v value		Big integer
 * @v bit		Bit to set
 */
#define bigint_clear_bit( value, bit ) do {				\
	unsigned int size = bigint_size (value);			\
	bigint_clear_bit_raw ( (value)->element, size, bit );		\
	} while ( 0 )

/**
 * Test if bit is set in big integer
 *
 * @v value		Big integer
 * @v bit		Bit to test
 * @ret is_set		Bit is set
 */
#define bigint_bit_is_set( value, bit ) ( {				\
	unsigned int size = bigint_size (value);			\
	bigint_bit_is_set_raw ( (value)->element, size, bit ); } )

/**
 * Test if most significant bit is set in big integer
 *
 * @v value		Big integer
 * @ret is_set		Most significant bit is set
 */
#define bigint_msb_is_set( value ) ( {					\
	unsigned int size = bigint_size (value);			\
	bigint_msb_is_set_raw ( (value)->element, size ); } )

/**
 * Find highest bit set in big integer
 *
 * @v value		Big integer
 * @ret max_bit		Highest bit set + 1 (or 0 if no bits set)
 */
#define bigint_max_set_bit( value ) ( {					\
	unsigned int size = bigint_size (value);			\
	bigint_max_set_bit_raw ( (value)->element, size ); } )

/**
 * Grow big integer
 *
 * @v source		Source big integer
 * @v dest		Destination big integer
 */
#define bigint_grow( source, dest ) do {				\
	unsigned int source_size = bigint_size (source);		\
	unsigned int dest_size = bigint_size (dest);			\
	bigint_grow_raw ( (source)->element, source_size,		\
			  (dest)->element, dest_size );			\
	} while ( 0 )

/**
 * Shrink big integer
 *
 * @v source		Source big integer
 * @v dest		Destination big integer
 */
#define bigint_shrink( source, dest ) do {				\
	unsigned int source_size = bigint_size (source);		\
	unsigned int dest_size = bigint_size (dest);			\
	bigint_shrink_raw ( (source)->element, source_size,		\
			    (dest)->element, dest_size );		\
	} while ( 0 )

/**
 * Copy big integer
 *
 * @v source		Source big integer
 * @v dest		Destination big integer
 */
#define bigint_copy( source, dest ) do {				\
	build_assert ( sizeof ( *(source) ) == sizeof ( *(dest) ) );	\
	bigint_shrink ( (source), (dest) );				\
	} while ( 0 )

/**
 * Conditionally swap big integers (in constant time)
 *
 * @v first		Big integer to be conditionally swapped
 * @v second		Big integer to be conditionally swapped
 * @v swap		Swap first and second big integers
 */
#define bigint_swap( first, second, swap ) do {				\
	unsigned int size = bigint_size (first);			\
	bigint_swap_raw ( (first)->element, (second)->element, size,	\
			  (swap) );					\
	} while ( 0 )

/**
 * Multiply big integers
 *
 * @v multiplicand	Big integer to be multiplied
 * @v multiplier	Big integer to be multiplied
 * @v result		Big integer to hold result
 */
#define bigint_multiply( multiplicand, multiplier, result ) do {	\
	unsigned int multiplicand_size = bigint_size (multiplicand);	\
	unsigned int multiplier_size = bigint_size (multiplier);	\
	bigint_multiply_raw ( (multiplicand)->element,			\
			      multiplicand_size, (multiplier)->element,	\
			      multiplier_size, (result)->element );	\
	} while ( 0 )

/**
 * Reduce big integer R^2 modulo N
 *
 * @v modulus		Big integer modulus
 * @v result		Big integer to hold result
 */
#define bigint_reduce( modulus, result ) do {				\
	unsigned int size = bigint_size (modulus);			\
	bigint_reduce_raw ( (modulus)->element, (result)->element,	\
			    size );					\
	} while ( 0 )

/**
 * Compute inverse of odd big integer modulo any power of two
 *
 * @v invertend		Odd big integer to be inverted
 * @v inverse		Big integer to hold result
 */
#define bigint_mod_invert( invertend, inverse ) do {			\
	unsigned int size = bigint_size ( inverse );			\
	bigint_mod_invert_raw ( (invertend)->element,			\
				(inverse)->element, size );		\
	} while ( 0 )

/**
 * Perform relaxed Montgomery reduction (REDC) of a big integer
 *
 * @v modulus		Big integer odd modulus
 * @v value		Big integer to be reduced
 * @v result		Big integer to hold result
 * @ret carry		Carry out
 */
#define bigint_montgomery_relaxed( modulus, value, result ) ( {		\
	unsigned int size = bigint_size (modulus);			\
	bigint_montgomery_relaxed_raw ( (modulus)->element,		\
					(value)->element,		\
					(result)->element, size );	\
	} )

/**
 * Perform classic Montgomery reduction (REDC) of a big integer
 *
 * @v modulus		Big integer odd modulus
 * @v value		Big integer to be reduced
 * @v result		Big integer to hold result
 */
#define bigint_montgomery( modulus, value, result ) do {		\
	unsigned int size = bigint_size (modulus);			\
	bigint_montgomery_raw ( (modulus)->element, (value)->element,	\
				(result)->element, size );		\
	} while ( 0 )

/**
 * Perform generalised exponentiation via a Montgomery ladder
 *
 * @v result		Big integer result (initialised to identity element)
 * @v multiple		Big integer multiple (initialised to generator)
 * @v exponent		Big integer exponent
 * @v op		Montgomery ladder commutative operation
 * @v ctx		Operation context (if needed)
 * @v tmp		Temporary working space (if needed)
 */
#define bigint_ladder( result, multiple, exponent, op, ctx, tmp ) do {	\
	unsigned int size = bigint_size (result);			\
	unsigned int exponent_size = bigint_size (exponent);		\
	bigint_ladder_raw ( (result)->element, (multiple)->element,	\
			    size, (exponent)->element, exponent_size,	\
			    (op), (ctx), (tmp) );			\
	} while ( 0 )

/**
 * Perform modular exponentiation of big integers
 *
 * @v base		Big integer base
 * @v modulus		Big integer modulus
 * @v exponent		Big integer exponent
 * @v result		Big integer to hold result
 * @v tmp		Temporary working space
 */
#define bigint_mod_exp( base, modulus, exponent, result, tmp ) do {	\
	unsigned int size = bigint_size (base);				\
	unsigned int exponent_size = bigint_size (exponent);		\
	bigint_mod_exp_raw ( (base)->element, (modulus)->element,	\
			     (exponent)->element, (result)->element,	\
			     size, exponent_size, tmp );		\
	} while ( 0 )

/**
 * Calculate temporary working space required for moduluar exponentiation
 *
 * @v modulus		Big integer modulus
 * @ret len		Length of temporary working space
 */
#define bigint_mod_exp_tmp_len( modulus ) ( {				\
	unsigned int size = bigint_size (modulus);			\
	sizeof ( struct {						\
		bigint_t ( size ) temp[4];				\
	} ); } )

#include <bits/bigint.h>

/**
 * A big integer Montgomery ladder commutative operation
 *
 * @v operand		Element 0 of first input operand (may overlap result)
 * @v result		Element 0 of second input operand and result
 * @v size		Number of elements in operands and result
 * @v ctx		Operation context (if needed)
 * @v tmp		Temporary working space (if needed)
 */
typedef void ( bigint_ladder_op_t ) ( const bigint_element_t *operand0,
				      bigint_element_t *result0,
				      unsigned int size, const void *ctx,
				      void *tmp );

/**
 * Set bit in big integer
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @v bit		Bit to set
 */
static inline __attribute__ (( always_inline )) void
bigint_set_bit_raw ( bigint_element_t *value0, unsigned int size,
		     unsigned int bit ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	unsigned int index = ( bit / ( 8 * sizeof ( value->element[0] ) ) );
	unsigned int subindex = ( bit % ( 8 * sizeof ( value->element[0] ) ) );

	value->element[index] |= ( 1UL << subindex );
}

/**
 * Clear bit in big integer
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @v bit		Bit to clear
 */
static inline __attribute__ (( always_inline )) void
bigint_clear_bit_raw ( bigint_element_t *value0, unsigned int size,
		       unsigned int bit ) {
	bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( void * ) value0 );
	unsigned int index = ( bit / ( 8 * sizeof ( value->element[0] ) ) );
	unsigned int subindex = ( bit % ( 8 * sizeof ( value->element[0] ) ) );

	value->element[index] &= ~( 1UL << subindex );
}

/**
 * Test if bit is set in big integer
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @v bit		Bit to test
 * @ret is_set		Bit is set
 */
static inline __attribute__ (( always_inline )) int
bigint_bit_is_set_raw ( const bigint_element_t *value0, unsigned int size,
			unsigned int bit ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( const void * ) value0 );
	unsigned int index = ( bit / ( 8 * sizeof ( value->element[0] ) ) );
	unsigned int subindex = ( bit % ( 8 * sizeof ( value->element[0] ) ) );

	return ( !! ( value->element[index] & ( 1UL << subindex ) ) );
}

/**
 * Test if most significant bit is set in big integer
 *
 * @v value0		Element 0 of big integer
 * @v size		Number of elements
 * @ret is_set		Most significant bit is set
 */
static inline __attribute__ (( always_inline )) int
bigint_msb_is_set_raw ( const bigint_element_t *value0, unsigned int size ) {
	const bigint_t ( size ) __attribute__ (( may_alias )) *value =
		( ( const void * ) value0 );
	unsigned int index = ( size - 1 );
	unsigned int subindex = ( ( 8 * sizeof ( value->element[0] ) ) - 1 );

	return ( !! ( value->element[index] & ( 1UL << subindex ) ) );
}

const char * bigint_ntoa_raw ( const bigint_element_t *value0,
			       unsigned int size );
void bigint_init_raw ( bigint_element_t *value0, unsigned int size,
		       const void *data, size_t len );
void bigint_done_raw ( const bigint_element_t *value0, unsigned int size,
		       void *out, size_t len );
int bigint_add_raw ( const bigint_element_t *addend0,
		     bigint_element_t *value0, unsigned int size );
int bigint_subtract_raw ( const bigint_element_t *subtrahend0,
			  bigint_element_t *value0, unsigned int size );
int bigint_shl_raw ( bigint_element_t *value0, unsigned int size );
int bigint_shr_raw ( bigint_element_t *value0, unsigned int size );
int bigint_is_zero_raw ( const bigint_element_t *value0, unsigned int size );
int bigint_is_geq_raw ( const bigint_element_t *value0,
			const bigint_element_t *reference0,
			unsigned int size );
int bigint_bit_is_set_raw ( const bigint_element_t *value0, unsigned int size,
			    unsigned int bit );
int bigint_max_set_bit_raw ( const bigint_element_t *value0,
			     unsigned int size );
void bigint_grow_raw ( const bigint_element_t *source0,
		       unsigned int source_size, bigint_element_t *dest0,
		       unsigned int dest_size );
void bigint_shrink_raw ( const bigint_element_t *source0,
			 unsigned int source_size, bigint_element_t *dest0,
			 unsigned int dest_size );
void bigint_swap_raw ( bigint_element_t *first0, bigint_element_t *second0,
		       unsigned int size, int swap );
void bigint_multiply_one ( const bigint_element_t multiplicand,
			   const bigint_element_t multiplier,
			   bigint_element_t *result,
			   bigint_element_t *carry );
void bigint_multiply_raw ( const bigint_element_t *multiplicand0,
			   unsigned int multiplicand_size,
			   const bigint_element_t *multiplier0,
			   unsigned int multiplier_size,
			   bigint_element_t *result0 );
void bigint_reduce_raw ( const bigint_element_t *modulus0,
			 bigint_element_t *result0, unsigned int size );
void bigint_mod_invert_raw ( const bigint_element_t *invertend0,
			     bigint_element_t *inverse0, unsigned int size );
int bigint_montgomery_relaxed_raw ( const bigint_element_t *modulus0,
				    bigint_element_t *value0,
				    bigint_element_t *result0,
				    unsigned int size );
void bigint_montgomery_raw ( const bigint_element_t *modulus0,
			     bigint_element_t *value0,
			     bigint_element_t *result0, unsigned int size );
void bigint_ladder_raw ( bigint_element_t *result0,
			 bigint_element_t *multiple0, unsigned int size,
			 const bigint_element_t *exponent0,
			 unsigned int exponent_size, bigint_ladder_op_t *op,
			 const void *ctx, void *tmp );
void bigint_mod_exp_ladder ( const bigint_element_t *multiplier0,
			     bigint_element_t *result0, unsigned int size,
			     const void *ctx, void *tmp );
void bigint_mod_exp_raw ( const bigint_element_t *base0,
			  const bigint_element_t *modulus0,
			  const bigint_element_t *exponent0,
			  bigint_element_t *result0,
			  unsigned int size, unsigned int exponent_size,
			  void *tmp );

#endif /* _IPXE_BIGINT_H */
