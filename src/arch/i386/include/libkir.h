#ifndef LIBKIR_H
#define LIBKIR_H

#include "realmode.h"

#ifndef ASSEMBLY

/*
 * Full API documentation for these functions is in realmode.h.
 *
 */

/* Copy to/from base memory */

static inline void copy_to_real_libkir ( uint16_t dest_seg, uint16_t dest_off,
					 void *src, size_t n ) {
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

/* Place/remove parameter on real-mode stack in a way that's
 * compatible with libkir
 */
#define BASEMEM_PARAMETER_INIT_LIBKIR( param ) \
	( ( uint16_t ) ( ( uint32_t ) & ( param ) ) )
#define BASEMEM_PARAMETER_DONE_LIBKIR( param )
#define BASEMEM_PARAMETER_INIT BASEMEM_PARAMETER_INIT_LIBKIR
#define BASEMEM_PARAMETER_DONE BASEMEM_PARAMETER_DONE_LIBKIR

/* REAL_FRAGMENT: Declare and define a real-mode code fragment in
 * .text16.  We don't need this for REAL_EXEC, since we can just
 * execute our real-mode code inline, but it's handy in case someone
 * genuinely wants to create a block of code that can be copied to a
 * specific location and then executed.
 *
 * Note that we put the code in the data segment, since otherwise we
 * can't (easily) access it in order to copy it to its target
 * location.  We should never be calling any REAL_FRAGMENT routines
 * directly anyway.
 */
#define	REAL_FRAGMENT( name, asm_code_str )				\
	extern void name ( void );					\
	extern char name ## _size[];					\
	__asm__ __volatile__ (						\
		".section \".data.text16\"\n\t"				\
		".code16\n\t"						\
		".arch i386\n\t"					\
		#name ":\n\t"						\
		asm_code_str "\n\t"					\
		"lret\n\t"						\
		#name "_end:\n\t"					\
		".equ " #name "_size, " #name "_end - " #name "\n\t"	\
		".code16gcc\n\t"					\
		".previous\n\t"						\
		: :							\
	)
#define FRAGMENT_SIZE( fragment ) ( (size_t) fragment ## _size )

/* REAL_CALL: call an external real-mode routine */
#define OUT_CONSTRAINTS(...) __VA_ARGS__
#define IN_CONSTRAINTS(...) "m" ( __routine ), ## __VA_ARGS__
#define CLOBBER(...) __VA_ARGS__
#define REAL_CALL( routine, num_out_constraints, out_constraints,	     \
		   in_constraints, clobber )				     \
	do {								     \
		segoff_t __routine = routine;				     \
		__asm__ __volatile__ (					     \
			"pushl %" #num_out_constraints "\n\t"		     \
			".code16\n\t"					     \
			"pushw %%gs\n\t"	/* preserve segs */	     \
			"pushw %%fs\n\t"				     \
			"pushw %%es\n\t"				     \
			"pushw %%ds\n\t"				     \
			"pushw %%cs\n\t"	/* lcall to routine */	     \
			"call 1f\n\t"					     \
			"jmp 2f\n\t"					     \
			"\n1:\n\t"					     \
			"addr32 pushl 12(%%esp)\n\t"			     \
			"lret\n\t"					     \
			"\n2:\n\t"					     \
			"popw %%ds\n\t"		/* restore segs */	     \
			"popw %%es\n\t"					     \
			"popw %%fs\n\t"					     \
			"popw %%gs\n\t"					     \
			"addw $4, %%sp\n\t"				     \
			".code16gcc\n\t"				     \
			: out_constraints : in_constraints : clobber  	     \
		);							     \
	} while ( 0 )

/* REAL_EXEC: execute some inline assembly code in a way that matches
 * the interface of librm
 */

#define IN_CONSTRAINTS_NO_ROUTINE( routine, ... ) __VA_ARGS__
#define REAL_EXEC( name, asm_code_str, num_out_constraints, out_constraints, \
		   in_constraints, clobber )				     \
	__asm__ __volatile__ (						     \
		".code16\n\t"						     \
		"pushw %%gs\n\t"					     \
		"pushw %%fs\n\t"					     \
		"pushw %%es\n\t"					     \
		"pushw %%ds\n\t"					     \
		"\n" #name ":\n\t"					     \
		asm_code_str						     \
		"popw %%ds\n\t"						     \
		"popw %%es\n\t"						     \
		"popw %%fs\n\t"						     \
		"popw %%gs\n\t"						     \
		".code16gcc\n\t"					     \
		: out_constraints					     \
		: IN_CONSTRAINTS_NO_ROUTINE ( in_constraints )		     \
		: clobber				   		     \
		);

#endif /* ASSEMBLY */

#endif /* LIBKIR_H */
