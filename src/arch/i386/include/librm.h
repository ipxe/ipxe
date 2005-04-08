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

/* Real-mode call parameter block, as passed to real_call */
struct real_call_params {
	struct i386_seg_regs;
	struct i386_regs;
	segoff_t rm_code;
	segoff_t reserved;
} PACKED;

/* Current location of librm in base memory */
extern char *installed_librm;

/* Start and size of our source copy of librm (i.e. the one that we
 * can install by copying it to base memory and setting
 * installed_librm)
 */
extern char librm[];
extern size_t _librm_size[];

/* Linker symbols for offsets within librm.  Other symbols should
 * almost certainly not be referred to from C code.
 */
extern void (*_real_to_prot[]) ( void );
extern void (*_prot_to_real[]) ( void );
extern void (*_prot_call[]) ( void );
extern void (*_real_call[]) ( void );
extern segoff_t _rm_stack[];
extern uint32_t _pm_stack[];
extern char _librm_ref_count[];

/* Symbols within current installation of librm */
#define LIBRM_VAR( sym ) \
	( * ( ( typeof ( * _ ## sym ) * ) \
	      & ( installed_librm [ (int) _ ## sym ] ) ) )
#define LIBRM_FN( sym ) \
	 ( ( typeof ( * _ ## sym ) ) \
	      & ( installed_librm [ (int) _ ## sym ] ) )
#define LIBRM_CONSTANT( sym ) \
	( ( typeof ( * _ ## sym ) ) ( _ ## sym ) )
#define inst_real_to_prot	LIBRM_FN ( real_to_prot )
#define inst_prot_to_real	LIBRM_FN ( prot_to_real )
#define inst_prot_call		LIBRM_FN ( prot_call )
#define inst_real_call		LIBRM_FN ( real_call )
#define inst_rm_stack		LIBRM_VAR ( rm_stack )
#define inst_pm_stack		LIBRM_VAR ( pm_stack )
#define inst_librm_ref_count	LIBRM_VAR ( librm_ref_count )
#define librm_size		LIBRM_CONSTANT ( librm_size )

/* Functions that librm expects to be able to link to.  Included here
 * so that the compiler will catch prototype mismatches.
 */
extern void _phys_to_virt ( void );
extern void _virt_to_phys ( void );
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
	extern char name ## _size[];					\
	__asm__ __volatile__ (						\
		".section \".text16\"\n\t"				\
		".code16\n\t"						\
		".arch i386\n\t"					\
		#name ":\n\t"						\
		asm_code_str "\n\t"					\
		"lret\n\t"						\
		#name "_end:\n\t"					\
		".equ " #name "_size, " #name "_end - " #name "\n\t"	\
		".code32\n\t"						\
		".previous\n\t"						\
		: :							\
	)
#define FRAGMENT_SIZE( fragment ) ( (size_t) fragment ## _size )

/* REAL_CALL: call a real-mode routine via librm */
#define OUT_CONSTRAINTS(...) __VA_ARGS__
#define IN_CONSTRAINTS(...) "m" ( __routine ), ## __VA_ARGS__
#define CLOBBER(...) __VA_ARGS__
#define REAL_CALL( routine, num_out_constraints, out_constraints,	     \
		   in_constraints, clobber )				     \
	do {								     \
		segoff_t __routine = routine;				     \
		__asm__ __volatile__ (					     \
				      "pushl %" #num_out_constraints "\n\t"  \
				      "call 1f\n\t"			     \
				      "jmp 2f\n\t"			     \
				      "\n1:\n\t"			     \
				      "pushl installed_librm\n\t"	     \
				      "addl $_real_call, 0(%%esp)\n\t"	     \
				      "ret\n\t"				     \
				      "\n2:\n\t"			     \
				      "addl $4, %%esp\n\t"		     \
				      : out_constraints			     \
				      : in_constraints			     \
				      : clobber				     \
				      );				     \
	} while ( 0 )

/* REAL_EXEC: combine RM_FRAGMENT and REAL_CALL into one handy unit */
#define PASSTHRU(...) __VA_ARGS__
#define REAL_EXEC( name, asm_code_str, num_out_constraints, out_constraints, \
		   in_constraints, clobber )				     \
	do {								     \
		segoff_t fragment;					     \
									     \
		REAL_FRAGMENT ( name, asm_code_str );			     \
									     \
		fragment.segment = inst_rm_stack.segment;		     \
		fragment.offset =					     \
			copy_to_rm_stack ( name, FRAGMENT_SIZE ( name ) );   \
									     \
		REAL_CALL ( fragment, num_out_constraints,		     \
			    PASSTHRU ( out_constraints ),		     \
			    PASSTHRU ( in_constraints ),		     \
			    PASSTHRU ( clobber ) );			     \
									     \
		remove_from_rm_stack ( NULL, FRAGMENT_SIZE ( name ) );	     \
	} while ( 0 )

#endif /* ASSEMBLY */

#endif /* LIBRM_H */
