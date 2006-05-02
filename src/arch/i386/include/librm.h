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

/* Variables in librm.S, present in the normal data segment */
extern uint16_t rm_sp;
extern uint16_t rm_ss;
extern uint16_t rm_cs;
extern uint32_t pm_esp;

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
static inline void copy_to_real_librm ( uint16_t dest_seg, uint16_t dest_off,
					void *src, size_t n ) {
	memcpy ( VIRTUAL ( dest_seg, dest_off ), src, n );
}
static inline void copy_from_real_librm ( void *dest,
					  uint16_t src_seg, uint16_t src_off,
					  size_t n ) {
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

/* Copy to/from real-mode stack */
extern uint16_t copy_to_rm_stack ( void *data, size_t size );
extern void remove_from_rm_stack ( void *data, size_t size );

/* Place/remove parameter on real-mode stack in a way that's
 * compatible with libkir
 */
#define BASEMEM_PARAMETER_INIT_LIBRM( param ) \
	copy_to_rm_stack ( & ( param ), sizeof ( param ) )
#define BASEMEM_PARAMETER_DONE_LIBRM( param ) \
	remove_from_rm_stack ( & ( param ), sizeof ( param ) )
#define BASEMEM_PARAMETER_INIT BASEMEM_PARAMETER_INIT_LIBRM
#define BASEMEM_PARAMETER_DONE BASEMEM_PARAMETER_DONE_LIBRM

/* REAL_FRAGMENT: Declare and define a real-mode code fragment in .text16 */
#define	REAL_FRAGMENT( name, asm_code_str )				\
	extern void name ( void );					\
	__asm__ __volatile__ (						\
		".section \".text16\"\n\t"				\
		".code16\n\t"						\
		".arch i386\n\t"					\
		#name ":\n\t"						\
		asm_code_str "\n\t"					\
		"ret\n\t"						\
		".size " #name ", . - " #name "\n\t"			\
		".code32\n\t"						\
		".previous\n\t"						\
		: :							\
	)
#define FRAGMENT_SIZE( fragment ) ( (size_t) fragment ## _size )

/* REAL_CALL: call a real-mode routine via librm */
#define OUT_CONSTRAINTS(...) __VA_ARGS__
#define IN_CONSTRAINTS(...) __VA_ARGS__
#define CLOBBER(...) __VA_ARGS__
#define REAL_CALL( routine, num_out_constraints, out_constraints,	\
		   in_constraints, clobber )				\
	do {								\
		__asm__ __volatile__ (					\
				      "pushl $" #routine "\n\t"		\
				      "call real_call\n\t"		\
				      "addl $4, %%esp\n\t"		\
				      : out_constraints			\
				      : in_constraints			\
				      : clobber				\
				      );				\
	} while ( 0 )

/* REAL_EXEC: combine RM_FRAGMENT and REAL_CALL into one handy unit */
#define PASSTHRU(...) __VA_ARGS__
#define REAL_EXEC( name, asm_code_str, num_out_constraints, out_constraints, \
		   in_constraints, clobber )				     \
	do {								     \
		REAL_FRAGMENT ( name, asm_code_str );			     \
									     \
		REAL_CALL ( name, num_out_constraints,			     \
			    PASSTHRU ( out_constraints ),		     \
			    PASSTHRU ( in_constraints ),		     \
			    PASSTHRU ( clobber ) );			     \
	} while ( 0 )

#endif /* ASSEMBLY */

#endif /* LIBRM_H */
