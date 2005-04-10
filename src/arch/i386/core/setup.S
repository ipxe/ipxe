/****************************************************************************
 * This file provides the setup() and setup16() functions.  The
 * purpose of these functions is to set up the internal environment so
 * that C code can execute.  This includes setting up the internal
 * stack and (where applicable) setting up a GDT for virtual
 * addressing.
 *
 * These functions are designed to be called by the prefix.
 *
 * The same basic assembly code is used to compile both setup()
 * and setup16().
 ****************************************************************************
 */

	.text
	.arch i386

#ifdef CODE16
/****************************************************************************
 * setup16 (real-mode far call)
 *
 * This function can be called by a 16-bit prefix in order to set up
 * the internal (either 16-bit or 32-bit) environment.
 *
 * Parameters: none
 *
 * %cs:0000, %ds:0000 and %es:0000 must point to the start of the
 * (decompressed) runtime image.
 *
 * If KEEP_IT_REAL is defined, then %ds:0000 may instead point to the
 * start of the (decompressed) data segment portion of the runtime
 * image.  If %ds==%cs, then it will be assumed that the data segment
 * follows immediately after the code segment.
 ****************************************************************************
 */

#ifdef KEEP_IT_REAL

#define ENTER_FROM_EXTERNAL call ext_to_kir
#define RETURN_TO_EXTERNAL call kir_to_ext
#define ENTRY_POINT kir_call

#else /* KEEP_IT_REAL */

#define ENTER_FROM_EXTERNAL \
	pushw %cs ; \
	call real_to_prot ; \
	.code32
#define RETURN_TO_EXTERNAL \
	call prot_to_real ; \
	.code16
#define ENTRY_POINT _prot_call /* _prot_call = OFFSET ( prot_call ) in librm */

#endif /* KEEP_IT_REAL */
	
#define ENTRY_POINT_REGISTER di
	 
	.section ".text16"
	.code16
	.globl setup16
setup16:
	
#else /* CODE16 */

/****************************************************************************
 * setup (32-bit protected-mode near call)
 *
 * This function can be called by a 32-bit prefix in order to set up
 * the internal 32-bit environment.
 *
 * Parameters: none
 ****************************************************************************
 */

#define ENTER_FROM_EXTERNAL call ext_to_int
#define RETURN_TO_EXTERNAL call int_to_ext
#define ENTRY_POINT int_call
#define ENTRY_POINT_REGISTER edi
	
	.section ".text"
	.code32
	.globl setup
setup:
	
#endif /* CODE16 */
	
	/* Preserve flags (including interrupt status) and registers */
	pushfl
	pushl	%ebp

	/* Switch to (uninitialised) internal environment.  This will
	 * preserve the external environment for when we call
	 * RETURN_TO_EXTERNAL.
	 */
	ENTER_FROM_EXTERNAL
	/* NOTE: We may have only four bytes of stack at this point */

#if defined(CODE16) && defined(KEEP_IT_REAL)
	
	/* If %ds == %cs, then the data segment is located immediately
	 * after the code segment.
	 */
	pushw	%ax
	movw	%cs, %ax
	movw	%ds, %bp
	cmpw	%ax, %bp
	jne	1f
	addw	$_text_load_size_pgh, %ax
	movw	%ax, %ds
1:	popw	%ax
		
	/* Switch to internal stack */
	pushw	%ds
	popw	%ss
	movl	$_estack, %esp

#else /* CODE16 && KEEP_IT_REAL */

	/* Work out where we're running and switch to internal pmode
	 * stack
	 */
	call	1f
1:	popl	%ebp
	leal	(_estack-1b)(%ebp), %esp

	/* Set up GDT for virtual addressing */
	call	run_here

#endif /* CODE16 && KEEP_IT_REAL */
	
	/* Switch back to external environment.  This will preserve
	 * the internal environment ready for the next call.
	 */
	RETURN_TO_EXTERNAL

	/* Set %es:[e]di to point to entry-point function.
	 */
	push	%cs
	pop	%es
	mov	$ENTRY_POINT, %ENTRY_POINT_REGISTER

	/* Far call to arch_initialise via the entry-point function.
	 * arch_initialise() (or the entry-point function itself) may
	 * update %es:[e]di to point to a new entry-point function for
	 * subsequent calls.  librm will use this facility, since
	 * arch_initialise() causes librm to be relocated.
	 */
	pushl	$arch_initialise
	push	%cs		/* lcall %es:[x]di == %cs:[x]di */
	call	*%ENTRY_POINT_REGISTER
	popl	%ebp /* discard */
	
	/* Restore flags (including interrupt status) and return */
	popl	%ebp
	popfl
	lret

/****************************************************************************
 * Internal stack
 ****************************************************************************
 */
	.section ".stack"
	.align 8
_stack:
	.space 4096
_estack:
