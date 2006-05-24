#ifndef LIBKIR_H
#define LIBKIR_H

#include "realmode.h"

#ifndef ASSEMBLY

/*
 * Full API documentation for these functions is in realmode.h.
 *
 */

/* Access to variables in .data16 and .text16 in a way compatible with librm */
#define __data16( variable ) variable
#define __data16_array( variable, array ) variable array
#define __text16( variable ) variable
#define __text16_array( variable,array ) variable array
#define __use_data16( variable ) variable
#define __use_text16( variable ) variable
#define __from_data16( variable ) variable
#define __from_text16( variable ) variable

/* Real-mode data and code segments */
static inline __attribute__ (( always_inline )) unsigned int _rm_cs ( void ) {
	uint16_t cs;
	__asm__ __volatile__ ( "movw %%cs, %w0" : "=r" ( cs ) );
	return cs;
}

static inline __attribute__ (( always_inline )) unsigned int _rm_ds ( void ) {
	uint16_t ds;
	__asm__ __volatile__ ( "movw %%ds, %w0" : "=r" ( ds ) );
	return ds;
}

#define rm_cs ( _rm_cs() )
#define rm_ds ( _rm_ds() )

/* Copy to/from base memory */

static inline void copy_to_real_libkir ( uint16_t dest_seg, uint16_t dest_off,
					 const void *src, size_t n ) {
	__asm__ __volatile__ ( "movw %4, %%es\n\t"
			       "cld\n\t"
			       "rep movsb\n\t"
			       "pushw %%ds\n\t" /* restore %es */
			       "popw %%es\n\t"
			       : "=S" ( src ), "=D" ( dest_off ),
			         "=c" ( n ) /* clobbered */
			       : "S" ( src ), "r" ( dest_seg ),
			         "D" ( dest_off ), "c" ( n )
			       : "memory" );
}

static inline void copy_from_real_libkir ( void *dest,
					   uint16_t src_seg, uint16_t src_off,
					   size_t n ) {
	__asm__ __volatile__ ( "movw %%ax, %%ds\n\t"
			       "cld\n\t"
			       "rep movsb\n\t"
			       "pushw %%es\n\t" /* restore %ds */
			       "popw %%ds\n\t"
			       : "=S" ( src_off ), "=D" ( dest ),
			         "=c" ( n ) /* clobbered */
			       : "a" ( src_seg ), "S" ( src_off ),
			         "D" ( dest ), "c" ( n )
			       : "memory" );
}
#define copy_to_real copy_to_real_libkir
#define copy_from_real copy_from_real_libkir

/*
 * Transfer individual values to/from base memory.  There may well be
 * a neater way to do this.  We have two versions: one for constant
 * offsets (where the mov instruction must be of the form "mov
 * %es:123, %xx") and one for non-constant offsets (where the mov
 * instruction must be of the form "mov %es:(%xx), %yx".  If it's
 * possible to incorporate both forms into one __asm__ instruction, I
 * don't know how to do it.
 *
 * Ideally, the mov instruction should be "mov%z0"; the "%z0" is meant
 * to expand to either "b", "w" or "l" depending on the size of
 * operand 0.  This would remove the (minor) ambiguity in the mov
 * instruction.  However, gcc on at least my system barfs with an
 * "internal compiler error" when confronted with %z0.
 *
 */

#define put_real_kir_const_off( var, seg, off )		  		     \
	__asm__ ( "movw %w1, %%es\n\t"					     \
		  "mov %0, %%es:%c2\n\t"				     \
		  "pushw %%ds\n\t" /* restore %es */			     \
		  "popw %%es\n\t"					     \
		  :							     \
		  : "r,r" ( var ), "rm,rm" ( seg ), "i,!r" ( off )	     \
		  )

#define put_real_kir_nonconst_off( var, seg, off )	  		     \
	__asm__ ( "movw %w1, %%es\n\t"					     \
		  "mov %0, %%es:(%2)\n\t"				     \
		  "pushw %%ds\n\t" /* restore %es */			     \
		  "popw %%es\n\t"					     \
		  :							     \
		  : "r" ( var ), "rm" ( seg ), "r" ( off )		     \
		  )

#define put_real_kir( var, seg, off )					     \
	do {								     \
	  if ( __builtin_constant_p ( off ) )				     \
		  put_real_kir_const_off ( var, seg, off );		     \
	  else								     \
		  put_real_kir_nonconst_off ( var, seg, off );		     \
	} while ( 0 )

#define get_real_kir_const_off( var, seg, off )		  		     \
	__asm__ ( "movw %w1, %%es\n\t"					     \
		  "mov %%es:%c2, %0\n\t"				     \
		  "pushw %%ds\n\t" /* restore %es */			     \
		  "popw %%es\n\t"					     \
		  : "=r,r" ( var )					     \
		  : "rm,rm" ( seg ), "i,!r" ( off )			     \
		  )

#define get_real_kir_nonconst_off( var, seg, off )			     \
	__asm__ ( "movw %w1, %%es\n\t"					     \
		  "mov %%es:(%2), %0\n\t"				     \
		  "pushw %%ds\n\t" /* restore %es */			     \
		  "popw %%es\n\t"					     \
		  : "=r" ( var )					     \
		  : "rm" ( seg ), "r" ( off )				     \
		  )

#define get_real_kir( var, seg, off )					     \
	do {								     \
	  if ( __builtin_constant_p ( off ) )				     \
		  get_real_kir_const_off ( var, seg, off );		     \
	  else								     \
		  get_real_kir_nonconst_off ( var, seg, off );		     \
	} while ( 0 )

#define put_real put_real_kir
#define get_real get_real_kir

/**
 * A pointer to a user buffer
 *
 * This is actually a struct segoff, but encoded as a uint32_t to
 * ensure that gcc passes it around efficiently.
 */
typedef uint32_t userptr_t;

/**
 * Copy data to user buffer
 *
 * @v buffer	User buffer
 * @v offset	Offset within user buffer
 * @v src	Source
 * @v len	Length
 */
static inline __attribute__ (( always_inline )) void
copy_to_user ( userptr_t buffer, off_t offset, const void *src, size_t len ) {
	copy_to_real ( ( buffer >> 16 ), ( ( buffer & 0xffff ) + offset ),
		       src, len );
}

/**
 * Copy data from user buffer
 *
 * @v dest	Destination
 * @v buffer	User buffer
 * @v offset	Offset within user buffer
 * @v len	Length
 */
static inline __attribute__ (( always_inline )) void
copy_from_user ( void *dest, userptr_t buffer, off_t offset, size_t len ) {
	copy_from_real ( dest, ( buffer >> 16 ),
			 ( ( buffer & 0xffff ) + offset ), len );
}

/**
 * Convert segment:offset address to user buffer
 *
 * @v segment	Real-mode segment
 * @v offset	Real-mode offset
 * @ret buffer	User buffer
 */
static inline __attribute__ (( always_inline )) userptr_t
real_to_user ( unsigned int segment, unsigned int offset ) {
	return ( ( segment << 16 ) | offset );
}

/**
 * Convert virtual address to user buffer
 *
 * @v virtual	Virtual address
 * @ret buffer	User buffer
 *
 * This constructs a user buffer from an ordinary pointer.  Use it
 * when you need to pass a pointer to an internal buffer to a function
 * that expects a @c userptr_t.
 */
static inline __attribute__ (( always_inline )) userptr_t
virt_to_user ( void * virtual ) {
	return real_to_user ( rm_ds, ( intptr_t ) virtual );
}

/* Place/remove parameter on real-mode stack in a way that's
 * compatible with libkir
 */
#define BASEMEM_PARAMETER_INIT_LIBKIR( param ) \
	( ( uint16_t ) ( ( uint32_t ) & ( param ) ) )
#define BASEMEM_PARAMETER_DONE_LIBKIR( param )
#define BASEMEM_PARAMETER_INIT BASEMEM_PARAMETER_INIT_LIBKIR
#define BASEMEM_PARAMETER_DONE BASEMEM_PARAMETER_DONE_LIBKIR

/* REAL_CODE: declare a fragment of code that executes in real mode */
#define REAL_CODE( asm_code_str )	\
	".code16\n\t"			\
	asm_code_str "\n\t"		\
	".code16gcc\n\t"

#endif /* ASSEMBLY */

#endif /* LIBKIR_H */
