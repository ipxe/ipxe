#ifndef LIBRM_H
#define LIBRM_H

/* Drag in protected-mode segment selector values */
#include "virtaddr.h"
#include "realmode.h"

#ifndef ASSEMBLY

#include "stddef.h"
#include "string.h"

/*
 * Data structures and type definitions
 *
 */

/* Access to variables in .data16 and .text16 */
extern char *data16;
extern char *text16;

#define __data16( variable )						\
	__attribute__ (( section ( ".data16" ) ))			\
	_data16_ ## variable __asm__ ( #variable )

#define __data16_array( variable, array )				\
	__attribute__ (( section ( ".data16" ) ))			\
	_data16_ ## variable array __asm__ ( #variable )

#define __bss16( variable )						\
	__attribute__ (( section ( ".bss16" ) ))			\
	_data16_ ## variable __asm__ ( #variable )

#define __bss16_array( variable, array )				\
	__attribute__ (( section ( ".bss16" ) ))			\
	_data16_ ## variable array __asm__ ( #variable )

#define __text16( variable )						\
	__attribute__ (( section ( ".text16.data" ) ))			\
	_text16_ ## variable __asm__ ( #variable )

#define __text16_array( variable, array )				\
	__attribute__ (( section ( ".text16.data" ) ))			\
	_text16_ ## variable array __asm__ ( #variable )

#define __use_data16( variable )					\
	( * ( ( typeof ( _data16_ ## variable ) * )			\
	      & ( data16 [ ( size_t ) & ( _data16_ ## variable ) ] ) ) )

#define __use_text16( variable )					\
	( * ( ( typeof ( _text16_ ## variable ) * )			\
	      & ( text16 [ ( size_t ) & ( _text16_ ## variable ) ] ) ) )

#define __from_data16( variable )					\
	( * ( ( typeof ( variable ) * )					\
	      ( ( ( void * ) &(variable) ) - ( ( void * ) data16 ) ) ) )

#define __from_text16( variable )					\
	( * ( ( typeof ( variable ) * )					\
	      ( ( ( void * ) &(variable) ) - ( ( void * ) text16 ) ) ) )

/* Variables in librm.S, present in the normal data segment */
extern uint16_t __data16 ( rm_cs );
#define rm_cs __use_data16 ( rm_cs )
extern uint16_t __text16 ( rm_ds );
#define rm_ds __use_text16 ( rm_ds )

/* Functions that librm expects to be able to link to.  Included here
 * so that the compiler will catch prototype mismatches.
 */
extern void gateA20_set ( void );

/*
 * librm_mgmt: functions for manipulating base memory and executing
 * real-mode code.
 *
 * Full API documentation for these functions is in realmode.h.
 *
 */

/* Macro for obtaining a physical address from a segment:offset pair. */
#define VIRTUAL(x,y) ( phys_to_virt ( ( ( x ) << 4 ) + ( y ) ) )

/* Copy to/from base memory */
static inline __attribute__ (( always_inline )) void
copy_to_real_librm ( unsigned int dest_seg, unsigned int dest_off,
		     void *src, size_t n ) {
	memcpy ( VIRTUAL ( dest_seg, dest_off ), src, n );
}
static inline __attribute__ (( always_inline )) void
copy_from_real_librm ( void *dest, unsigned int src_seg,
		       unsigned int src_off, size_t n ) {
	memcpy ( dest, VIRTUAL ( src_seg, src_off ), n );
}
#define put_real_librm( var, dest_seg, dest_off )			      \
	do {								      \
		* ( ( typeof(var) * ) VIRTUAL ( dest_seg, dest_off ) ) = var; \
	} while ( 0 )
#define get_real_librm( var, src_seg, src_off )				      \
	do {								      \
		var = * ( ( typeof(var) * ) VIRTUAL ( src_seg, src_off ) ); \
	} while ( 0 )
#define copy_to_real copy_to_real_librm
#define copy_from_real copy_from_real_librm
#define put_real put_real_librm
#define get_real get_real_librm

/**
 * A pointer to a user buffer
 *
 * Even though we could just use a void *, we use an intptr_t so that
 * attempts to use normal pointers show up as compiler warnings.  Such
 * code is actually valid for librm, but not for libkir (i.e. under
 * KEEP_IT_REAL), so it's good to have the warnings even under librm.
 */
typedef intptr_t userptr_t;

/**
 * Add offset to user pointer
 *
 * @v ptr		User pointer
 * @v offset		Offset
 * @ret new_ptr		New pointer value
 */
static inline __attribute__ (( always_inline )) userptr_t
userptr_add ( userptr_t ptr, off_t offset ) {
	return ( ptr + offset );
}

/**
 * Copy data to user buffer
 *
 * @v buffer		User buffer
 * @v offset		Offset within user buffer
 * @v src		Source
 * @v len		Length
 */
static inline __attribute__ (( always_inline )) void
copy_to_user ( userptr_t buffer, off_t offset, const void *src, size_t len ) {
	memcpy ( ( ( void * ) buffer + offset ), src, len );
}

/**
 * Copy data from user buffer
 *
 * @v dest		Destination
 * @v buffer		User buffer
 * @v offset		Offset within user buffer
 * @v len		Length
 */
static inline __attribute__ (( always_inline )) void
copy_from_user ( void *dest, userptr_t buffer, off_t offset, size_t len ) {
	memcpy ( dest, ( ( void * ) buffer + offset ), len );
}

/**
 * Copy data between user buffers
 *
 * @v dest		Destination user buffer
 * @v dest_off		Offset within destination buffer
 * @v src		Source user buffer
 * @v src_off		Offset within source buffer
 * @v len		Length
 */
static inline __attribute__ (( always_inline )) void
memcpy_user ( userptr_t dest, off_t dest_off, userptr_t src, off_t src_off,
	      size_t len ) {
	memcpy ( ( ( void * ) dest + dest_off ), ( ( void * ) src + src_off ),
		 len );
}

/**
 * Copy data between user buffers, allowing for overlap
 *
 * @v dest		Destination user buffer
 * @v dest_off		Offset within destination buffer
 * @v src		Source user buffer
 * @v src_off		Offset within source buffer
 * @v len		Length
 */
static inline __attribute__ (( always_inline )) void
memmove_user ( userptr_t dest, off_t dest_off, userptr_t src, off_t src_off,
	       size_t len ) {
	memmove ( ( ( void * ) dest + dest_off ), ( ( void * ) src + src_off ),
		  len );
}

/**
 * Fill user buffer with a constant byte
 *
 * @v buffer		User buffer
 * @v offset		Offset within buffer
 * @v c			Constant byte with which to fill
 * @v len		Length
 */
static inline __attribute__ (( always_inline )) void
memset_user ( userptr_t buffer, off_t offset, int c, size_t len ) {
	memset ( ( ( void * ) buffer + offset ), c, len );
}

/**
 * Find length of NUL-terminated string in user buffer
 *
 * @v buffer		User buffer
 * @v offset		Offset within buffer
 * @ret len		Length of string (excluding NUL)
 */
static inline __attribute__ (( always_inline )) size_t
strlen_user ( userptr_t buffer, off_t offset ) {
	return strlen ( ( void * ) buffer + offset );
}

/**
 * Find character in user buffer
 *
 * @v buffer		User buffer
 * @v offset		Starting offset within buffer
 * @v c			Character to search for
 * @v len		Length of user buffer
 * @ret offset		Offset of character, or <0 if not found
 */
static inline __attribute__ (( always_inline )) off_t
memchr_user ( userptr_t buffer, off_t offset, int c, size_t len ) {
	void *found;

	found = memchr ( ( ( void * ) buffer + offset ), c, len );
	return ( found ? ( found - ( void * ) buffer ) : -1 );
}

/**
 * Convert virtual address to user buffer
 *
 * @v virtual		Virtual address
 * @ret buffer		User buffer
 *
 * This constructs a user buffer from an ordinary pointer.  Use it
 * when you need to pass a pointer to an internal buffer to a function
 * that expects a @c userptr_t.
 */
static inline __attribute__ (( always_inline )) userptr_t
virt_to_user ( void * virtual ) {
	return ( ( intptr_t ) virtual );
}

/**
 * Convert segment:offset address to user buffer
 *
 * @v segment		Real-mode segment
 * @v offset		Real-mode offset
 * @ret buffer		User buffer
 */
static inline __attribute__ (( always_inline )) userptr_t
real_to_user ( unsigned int segment, unsigned int offset ) {
	return virt_to_user ( VIRTUAL ( segment, offset ) );
}

/**
 * Convert physical address to user buffer
 *
 * @v physical		Physical address
 * @ret buffer		User buffer
 */
static inline __attribute__ (( always_inline )) userptr_t
phys_to_user ( physaddr_t physical ) {
	return virt_to_user ( phys_to_virt ( physical ) );
}

/**
 * Convert user buffer to physical address
 *
 * @v buffer		User buffer
 * @v offset		Offset within user buffer
 * @ret physical	Physical address
 */
static inline __attribute__ (( always_inline )) physaddr_t
user_to_phys ( userptr_t buffer, off_t offset ) {
	return virt_to_phys ( ( void * ) buffer + offset );
}

/* TEXT16_CODE: declare a fragment of code that resides in .text16 */
#define TEXT16_CODE( asm_code_str )			\
	".section \".text16\", \"ax\", @progbits\n\t"	\
	".code16\n\t"					\
	asm_code_str "\n\t"				\
	".code32\n\t"					\
	".previous\n\t"

/* REAL_CODE: declare a fragment of code that executes in real mode */
#define REAL_CODE( asm_code_str )			\
	"pushl $1f\n\t"					\
	"call real_call\n\t"				\
	"addl $4, %%esp\n\t"				\
	TEXT16_CODE ( "\n1:\n\t"			\
		      asm_code_str			\
		      "\n\t"				\
		      "ret\n\t" )

/* PHYS_CODE: declare a fragment of code that executes in flat physical mode */
#define PHYS_CODE( asm_code_str )			\
	"call _virt_to_phys\n\t"			\
	asm_code_str					\
	"call _phys_to_virt\n\t"

#endif /* ASSEMBLY */

#endif /* LIBRM_H */
