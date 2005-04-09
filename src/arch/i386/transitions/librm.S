/*
 * librm: a library for interfacing to real-mode code
 *
 * Michael Brown <mbrown@fensystems.co.uk>
 *
 */

/* Drag in local definitions */
#include "librm.h"

/* Drag in FREE_BASEMEM_HEADER_SIZE */
#include "basemem.h"

/****************************************************************************
 * This file defines librm: a block of code that is designed to reside
 * permanently in base memory and provide the interface between
 * real-mode code running in base memory and protected-mode code
 * running in high memory.  It provides the following functions:
 *
 *   real_to_prot &	switch between real and protected mode 
 *   prot_to_real	while running in base memory, preserving 
 *			all non-segment registers
 *
 *   real_call		issue a call to a real-mode routine from
 *			protected-mode code running in high memory
 *
 *   prot_call		issue a call to a protected-mode routine from
 *			real-mode code running in base memory
 *
 * librm requires the following functions to be present in the
 * protected-mode code:
 *
 *   _phys_to_virt	Switch from physical to virtual addressing.  This
 *			routine must be position-independent and must
 *			*not* assume that it is genuinely running with
 *			flat physical addresses
 *
 *   _virt_to_phys	Switch from virtual to physical addresses.
 *
 *   gateA20_set	Enable the A20 line to permit access to the odd
 *			megabytes of RAM.  (This function will be called
 *			with virtual addresses set up).
 *
 * librm needs to be linked against the protected-mode binary so that
 * it can import the symbols for these functions.
 *
 * librm requires that the protected-mode code set up the following
 * segments:
 *
 *   PHYSICAL_CS	32-bit pmode code and data segments with flat
 *   PHYSICAL_DS	physical addresses.
 *
 *   VIRTUAL_CS		32-bit pmode code segment with virtual
 *			addressing, such that a protected-mode routine
 *			can always be found at $VIRTUAL_CS:routine.
 *
 * These segments must be set as #define constants when compiling
 * librm.  Edit librm.h to change the values.
 *
 * librm does not know the location of the code executing in high
 * memory.  It relies on the code running in high memory setting up a
 * GDT such that the high-memory code is accessible at virtual
 * addresses fixed at compile-time.
 *
 * librm symbols are exported as absolute values and represent offsets
 * into librm.  This is the most useful form of the symbols, since
 * librm is basically a binary blob that you place somewhere in base
 * memory.
 *
 * librm.h provides convenient ways to use these symbols: you simply
 * set the pointer ( char * ) installed_librm to point to wherever
 * librm is installed, and can then use e.g. inst_rm_stack just like
 * any other variable and have it automatically refer to the value of
 * rm_stack in the installed librm.  Macro trickery makes this
 * completely transparent, and the resulting assembler code is
 * amazingly efficient.
 *
 * Note that librm must be called in genuine real mode, not 16:16 or
 * 16:32 protected mode.  It makes the assumption that
 * physical_address = 16*segment+offset, and also that it can use
 * OFFSET(%bp) to access stack variables.  The former assumption will
 * break in either protected mode, the latter may break in 16:32
 * protected mode.
 ****************************************************************************
 */

/*
 * Default values for pmode segments if not defined
 */
#ifndef PHYSICAL_CS
#warning "Assuming PHYSICAL_CS = 0x08"
#define PHYSICAL_CS 0x08
#endif
#ifndef PHYSICAL_DS
#warning "Assuming PHYSICAL_DS = 0x10"
#define PHYSICAL_DS 0x10
#endif
#ifndef VIRTUAL_CS
#warning "Assuming VIRTUAL_CS = 0x18"
#define VIRTUAL_CS 0x18
#endif

/* For switches to/from protected mode */
#define CR0_PE 1

/* Size of various C data structures */
#define SIZEOF_I386_SEG_REGS	12
#define SIZEOF_I386_REGS	32
#define SIZEOF_I386_ALL_REGS	( SIZEOF_I386_SEG_REGS + SIZEOF_I386_REGS )
#define SIZEOF_I386_FLAGS	4
#define SIZEOF_REAL_MODE_REGS   ( SIZEOF_I386_ALL_REGS + SIZEOF_I386_FLAGS )
#define SIZEOF_SEGOFF_T		4
#define SIZEOF_REAL_CALL_PARAMS ( SIZEOF_I386_ALL_REGS + 2 * SIZEOF_SEGOFF_T )
	
	.text
	.arch i386
	.section ".librm", "awx", @progbits
	.align 16

	.globl	librm
librm:
	
_librm_start:

#undef OFFSET
#define OFFSET(sym) ( sym - _librm_start )

#undef EXPORT
#define EXPORT(sym) \
	.globl sym ; \
	.globl _ ## sym ;  \
	.equ _ ## sym, OFFSET(sym) ; \
	sym

/****************************************************************************
 * Note that the first sizeof(struct free_base_memory_header) bytes of
 * librm will get vapourised by free_base_memory().  Since we need
 * librm to continue working even when this happens, we put some
 * padding here.
 *
 * We must also ensure that the total size of librm is <1kB, otherwise
 * free_base_memory() will stomp somewhere in the middle of us as
 * well...
 ****************************************************************************
 */
	.fill FREE_BASEMEM_HEADER_SIZE, 1, 0
	
/****************************************************************************
 * GDT for initial transition to protected mode
 *
 * PHYSICAL_CS and PHYSICAL_DS are defined in an external header file.
 * We use only those selectors, and construct our GDT to match the
 * selector values we're asked to use.  Use PHYSICAL_CS=0x08 and
 * PHYSICAL_DS=0x10 to minimise the space occupied by this GDT.
 *
 * Note: pm_gdt is also used to store the location of the
 * protected-mode GDT as recorded on entry to prot_to_real.
 ****************************************************************************
 */
	.align 16
pm_gdt:
pm_gdt_limit:		.word pm_gdt_length - 1
pm_gdt_addr:		.long 0
			.word 0 /* padding */
	
	.org	pm_gdt + PHYSICAL_CS
pm_gdt_pm_cs:
	/* 32 bit protected mode code segment, physical addresses */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0
	
	.org	pm_gdt + PHYSICAL_DS
pm_gdt_pm_ds:
	/* 32 bit protected mode data segment, physical addresses */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0
	
pm_gdt_end:		
	.equ	pm_gdt_length, pm_gdt_end - pm_gdt

/****************************************************************************
 * GDT for transition to real mode
 *
 * This is used primarily to set 64kB segment limits.  Define
 * FLATTEN_REAL_MODE if you want to use so-called "flat real mode"
 * with 4GB limits instead.  The base address of each of the segments
 * will be adjusted at run-time.
 *
 * NOTE: This must be located before prot_to_real, otherwise gas
 * throws a "can't handle non absolute segment in `ljmp'" error due to
 * not knowing the value of RM_CS when the ljmp is encountered.
 *
 * Note also that putting ".word rm_gdt_end - rm_gdt - 1" directly
 * into rm_gdt_limit, rather than going via rm_gdt_length, will also
 * produce the "non absolute segment" error.  This is most probably a
 * bug in gas.
 ****************************************************************************
 */
	
#ifdef FLATTEN_REAL_MODE
#define RM_LIMIT_16_19__AVL__SIZE__GRANULARITY 0x8f
#else
#define RM_LIMIT_16_19__AVL__SIZE__GRANULARITY 0x00
#endif
	.align 16
rm_gdt:
rm_gdt_limit:		.word rm_gdt_length - 1
rm_gdt_base:		.long 0
			.word 0 /* padding */
	
rm_gdt_rm_cs: 	/* 16 bit real mode code segment */
	.equ	RM_CS, rm_gdt_rm_cs - rm_gdt
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x9b,RM_LIMIT_16_19__AVL__SIZE__GRANULARITY,(0>>24)
	
rm_gdt_rm_ds:	/* 16 bit real mode data segment */
	.equ	RM_DS, rm_gdt_rm_ds - rm_gdt
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x93,RM_LIMIT_16_19__AVL__SIZE__GRANULARITY,(0>>24)
	
rm_gdt_end:
	.equ	rm_gdt_length, rm_gdt_end - rm_gdt

/****************************************************************************
 * real_to_prot (real-mode far call)
 *
 * Switch from 16-bit real-mode to 32-bit protected mode with flat
 * physical addresses.  %esp is restored from the saved pm_esp.  All
 * segment registers are set to flat physical-mode values.  All other
 * registers are preserved.  Interrupts are disabled.
 *
 * Note that this routine can be called *without* having first set up
 * a stored pm_esp or stored GDT.  If you do this, real_to_prot will
 * return with a temporary stack that is only *FOUR BYTES* in size.
 * This is just enough to enable you to do a "call 1f; popl %ebp"
 * sequence in order to find out your physical address and then load a
 * proper 32-bit protected-mode stack pointer.  Do *NOT* use more than
 * four bytes since this will overwrite code in librm!
 *
 * Parameters: none
 ****************************************************************************
 */

	.code16		
EXPORT(real_to_prot):
	/* Disable interrupts */
	cli

	/* Set %ds = %cs, for easier access to variables */
	pushw	%cs
	popw	%ds
	
	/* Preserve registers */
	movl	%eax, %ds:OFFSET(save_eax)
	movl	%ebx, %ds:OFFSET(save_ebx)

	/* Extract real-mode far return address from stack */
	popl	%ds:OFFSET(save_retaddr)

	/* Record real-mode stack pointer */
	movw	%sp, %ds:OFFSET(rm_sp)
	pushw	%ss
	popw	%ds:OFFSET(rm_ss)

	/* Physical base address of librm to %ebx */
	xorl	%ebx, %ebx
	movw	%cs, %bx
	shll	$4, %ebx
		
	/* Check base address of stored protected-mode GDT.  If it's
	 * zero, set it up to use our internal GDT (with physical
	 * segments only).
	 */
	movl	%ds:OFFSET(pm_gdt_addr), %eax
	testl	%eax, %eax
	jnz	1f
	/* Use internal GDT */
	movl	%ebx, %eax
	addl	$OFFSET(pm_gdt), %eax
	movl	%eax, %ds:OFFSET(pm_gdt_addr)
1:	
	
	/* Set up protected-mode continuation address on real-mode stack */
	pushl	$PHYSICAL_CS
	movl	%ebx, %eax
	addl	$OFFSET(1f), %eax
	pushl	%eax
	
	/* Restore protected-mode GDT */
	lgdt	%ds:OFFSET(pm_gdt)

	/* Switch to protected mode */
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0

	/* Flush prefetch queue and reload %cs:eip */
	data32 lret
1:	.code32

	/* Set up protected-mode stack and data segments */
	movw	$PHYSICAL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* Switch to saved protected-mode stack.  Note that there may
	 * not actually *be* a saved protected-mode stack.
	 */
	movl	OFFSET(pm_esp)(%ebx), %esp
	testl	%esp, %esp
	jnz	1f
	/* No stack - use save_retaddr as a 4-byte temporary stack */
	leal	OFFSET(save_retaddr+4)(%ebx), %esp
1:	
	
	/* Convert real-mode far return address to physical address
	 * and place on stack
	 */
	pushl	OFFSET(save_retaddr)(%ebx)
	xorl	%eax, %eax
	xchgw	2(%esp), %ax
	shll	$4, %eax
	addl	%eax, 0(%esp)

	/* Restore registers and return */
	movl	OFFSET(save_eax)(%ebx), %eax
	movl	OFFSET(save_ebx)(%ebx), %ebx
	ret

/****************************************************************************
 * prot_to_real (protected-mode near call, physical addresses)
 *
 * Switch from 32-bit protected mode with flat physical addresses to
 * 16-bit real mode.  %ss:sp is restored from the saved rm_ss and
 * rm_sp.  %cs is set such that %cs:0000 is the start of librm.  All
 * other segment registers are set to %ss.  All other registers are
 * preserved.  Interrupts are *not* enabled, since we want to be able
 * to use this routine inside an ISR.
 *
 * Note that since %cs:0000 points to the start of librm on exit, it
 * follows that the code calling prot_to_real must be located within
 * 64kB of the start of librm.
 *
 * Parameters: none
 ****************************************************************************
 */

	.code32
EXPORT(prot_to_real):
	/* Calculate physical base address of librm in %ebx, preserve
	 * original %eax and %ebx in save_eax and save_ebx
	 */
	pushl	%ebx
	call	1f
1:	popl	%ebx
	subl	$OFFSET(1b), %ebx
	popl	OFFSET(save_ebx)(%ebx)
	movl	%eax, OFFSET(save_eax)(%ebx)

	/* Extract return address from the stack, convert to offset
	 * within librm and save in save_retaddr
	 */
	popl	%eax
	subl	%ebx, %eax
	movl	%eax, OFFSET(save_retaddr)(%ebx)

	/* Record protected-mode stack pointer */
	movl	%esp, OFFSET(pm_esp)(%ebx)

	/* Record protected-mode GDT */
	sgdt	OFFSET(pm_gdt)(%ebx)

	/* Set up real-mode GDT */
	leal	OFFSET(rm_gdt)(%ebx), %eax
	movl	%eax, OFFSET(rm_gdt_base)(%ebx)
	movl	%ebx, %eax
	rorl	$16, %eax
	movw	%bx, OFFSET(rm_gdt_rm_cs+2)(%ebx)
	movb	%al, OFFSET(rm_gdt_rm_cs+4)(%ebx)
	movw	%bx, OFFSET(rm_gdt_rm_ds+2)(%ebx)
	movb	%al, OFFSET(rm_gdt_rm_ds+4)(%ebx)
	
	/* Switch to real-mode GDT and reload segment registers to get
	 * 64kB limits.  Stack is invalidated by this process.
	 */
	lgdt	OFFSET(rm_gdt)(%ebx)
	ljmp	$RM_CS, $1f
1:	.code16
	movw	$RM_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* Calculate real-mode code segment in %ax and store in ljmp
	 * instruction
	 */
	movl	%ebx, %eax
	shrl	$4, %eax
	movw	%ax, OFFSET(p2r_ljmp) + 3

	/* Switch to real mode */
	movl	%cr0, %ebx
	andb	$0!CR0_PE, %bl
	movl	%ebx, %cr0

	/* Intersegment jump to flush prefetch queue and reload
	 * %cs:eip.  The segment gets filled in by the above code.  We
	 * can't just use lret to achieve this, because we have no
	 * stack at the moment.
	 */
p2r_ljmp:
	ljmp	$0, $OFFSET(1f)
1:	

	/* Set %ds to point to code segment for easier data access */
	movw	%ax, %ds
			
	/* Restore registers */
	movl	OFFSET(save_eax), %eax
	movl	OFFSET(save_ebx), %ebx

	/* Set up real-mode data segments and stack */
	movw	OFFSET(rm_ss), %ss
	movw	OFFSET(rm_sp), %sp
	pushw	%ss
	pushw	%ss
	pushw	%ss
	pushw	%ss
	popw	%ds
	popw	%es
	popw	%fs
	popw	%gs

	/* Set up return address on stack and return */
	pushw	%cs:OFFSET(save_retaddr)
	ret

/****************************************************************************
 * prot_call (real-mode far call)
 *
 * Call a specific C function in the protected-mode code.  The
 * prototype of the C function must be
 *   void function ( struct real_mode_regs *rm_regs,
 *		     void (*retaddr) (void) ); 
 * rm_regs will point to a struct containing the real-mode registers
 * at entry to prot_call.  retaddr will point to the (virtual) return
 * address from "function".  This return address will point into
 * librm.  It is included so that "function" may, if desired, relocate
 * librm and return via the new copy.  It must not be directly called
 * as a function, i.e. you may not do "*retaddr()"; you must instead
 * do something like:
 *	*retaddr += ( new_librm_location - old_librm_location );
 *	return;
 *
 * All registers will be preserved across prot_call(), unless the C
 * function explicitly overwrites values in rm_regs.  Interrupt status
 * will also be preserved.  Gate A20 will be enabled.
 *
 * Parameters:
 *   function : virtual address of protected-mode function to call
 *
 * Example usage:
 *	pushl	$pxe_api_call
 *	lcall	$LIBRM_SEGMENT, $prot_call
 *	addw	$4, %sp
 * to call in to the C function
 *      void pxe_api_call ( struct real_mode_regs *rm_regs );
 ****************************************************************************
 */

#define PC_OFFSET_RM_REGS ( 0 )
#define PC_OFFSET_RETADDR ( PC_OFFSET_RM_REGS + SIZEOF_REAL_MODE_REGS )
#define PC_OFFSET_FUNCTION ( PC_OFFSET_RETADDR + 4 )
	
	.code16
EXPORT(prot_call):
	/* Preserve registers and flags on RM stack */
	pushfl
	pushal
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%ss
	pushw	%cs	
	
	/* Record RM stack pointer */
	xorl	%ebp, %ebp
	movw	%sp, %bp
	
	/* Physical address of RM stack pointer to %esi */
	xorl	%esi, %esi
	pushw	%ss
	popw	%si
	shll	$4, %esi
	addl	%ebp, %esi

	/* Address of pmode function to %ebx */
	movl	%ss:(PC_OFFSET_FUNCTION)(%bp), %ebx
	
	/* Switch to protected mode */
	pushw	%cs
	call	real_to_prot
	.code32

	/* Copy rm_regs from RM stack to PM stack */
	movl	$SIZEOF_REAL_MODE_REGS, %ecx
	subl	%ecx, %esp
	movl	%esp, %edi
	pushl	%esi
	cld
	rep movsb
	popl	%edi		/* %edi = phys addr of RM copy of rm_regs */
	
	/* Switch to virtual addresses. */
	call	1f
	jmp	2f
1:	ljmp	$VIRTUAL_CS, $_phys_to_virt
2:	

	/* Enable A20 line */
	pushal
	lcall	$VIRTUAL_CS, $gateA20_set
	popl	%eax	/* discard */
	popal

	/* Push &rm_regs and &retaddr on the stack, and call function */
	movl	%esp, %ebp
	pushl	%esp
	subl	$12, 0(%esp)
	pushl	%ebp
	call	*%ebx
	popl	%eax /* discard */
	popl	%eax /* discard */

	/* Switch to physical addresses, discard PM register store */
	lcall	$VIRTUAL_CS, $_virt_to_phys
	popl	%eax /* discard */

	/* Copy rm_regs from PM stack to RM stack, and remove rm_regs
	 * from PM stack.  (%edi still contains physical address of
	 * rm_regs on RM stack from earlier, since C code preserves
	 * %edi).
	 */
	movl	%esp, %esi
	movl	$SIZEOF_REAL_MODE_REGS, %ecx
	cld
	rep movsb
	movl	%esi, %esp	/* remove rm_regs from PM stack */

	/* Switch to real mode */
	call	prot_to_real
	.code16

	/* Restore registers and flags, and return */
	popw	%ax	/* skip %cs */
	popw	%ax	/* skip %ss */
	popw	%ds
	popw	%es
	popw	%fs
	popw	%gs
	popal
	popfl
	lret

/****************************************************************************
 * real_call (protected-mode near call, virtual addresses)
 *
 * Call a real-mode function from protected-mode code.
 *
 * The non-segment register values will be passed directly to the
 * real-mode code.  The segment registers will be set as per
 * prot_to_real.  The non-segment register values set by the real-mode
 * function will be passed back to the protected-mode caller.  A
 * result of this is that this routine cannot be called directly from
 * C code, since it clobbers registers that the C ABI expects the
 * callee to preserve.  Gate A20 will be re-enabled in case the
 * real-mode routine disabled it.
 *
 * librm.h defines two convenient macros for using real_call:
 * REAL_CALL and REAL_EXEC.  See librm.h and realmode.h for details
 * and examples.
 *
 * Parameters:
 *   far pointer to real-mode function to call
 *
 * Returns: none
 ****************************************************************************
 */

#define RC_OFFSET_PRESERVE_REGS ( 0 )
#define RC_OFFSET_RETADDR ( RC_OFFSET_PRESERVE_REGS + 8 )
#define RC_OFFSET_RM_FUNCTION ( RC_OFFSET_RETADDR + 4 )
	
	.code32
EXPORT(real_call):
	/* Preserve registers */
	pushl	%ebp
	pushl	%eax
	
	/* Switch to physical addresses */
	lcall	$VIRTUAL_CS, $_virt_to_phys
	addl	$4, %esp

	/* Extract real-mode function address and store in ljmp instruction */
	call	1f
1:	popl	%ebp
	movl	RC_OFFSET_RM_FUNCTION(%esp), %eax
	movl	%eax, (rc_ljmp + 1 - 1b)(%ebp)

	/* Restore registers */
	popl	%eax
	popl	%ebp

	/* Switch to real mode, preserving non-segment registers */
	call	prot_to_real
	.code16

	/* Far call to real-mode routine */
	pushw	%cs
	call	rc_ljmp
	jmp	2f
rc_ljmp:	
	ljmp	$0, $0	/* address filled in by above code */
2:	
	
	/* Switch to protected mode */
	pushw	%cs
	call	real_to_prot
	.code32

	/* Switch to virtual addresses */
	call	1f
	jmp	2f
1:	ljmp	$VIRTUAL_CS, $_phys_to_virt
2:	

	/* Enable A20 line */
	pushal
	lcall	$VIRTUAL_CS, $gateA20_set
	popl	%eax	/* discard */
	popal

	/* Return */
	ret
	
/****************************************************************************
 * Relocation lock counter
 *
 * librm may be moved in base memory only when this counter is zero.
 * The counter gets incremented whenever a reference to librm is
 * generated (e.g. a real_call is made, resulting in a return address
 * pointing to librm being placed on the stack), and decremented when
 * the reference goes out of scope (e.g. the real_call returns).
 ****************************************************************************
 */
EXPORT(librm_ref_count):	.byte 0

/****************************************************************************
 * Stored real-mode and protected-mode stack pointers
 *
 * The real-mode stack pointer is stored here whenever real_to_prot
 * is called and restored whenever prot_to_real is called.  The
 * converse happens for the protected-mode stack pointer.
 *
 * Despite initial appearances this scheme is, in fact re-entrant,
 * because program flow dictates that we always return via the point
 * we left by.  For example:
 *    PXE API call entry
 *  1   real => prot
 *        ...
 *        Print a text string
 *	    ...
 *  2       prot => real
 *            INT 10
 *  3       real => prot
 *	    ...
 *        ...
 *  4   prot => real
 *    PXE API call exit
 *
 * At point 1, the RM mode stack value, say RPXE, is stored in
 * rm_ss,sp.  We want this value to still be present in rm_ss,sp when
 * we reach point 4.
 *
 * At point 2, the RM stack value is restored from RPXE.  At point 3,
 * the RM stack value is again stored in rm_ss,sp.  This *does*
 * overwrite the RPXE that we have stored there, but it's the same
 * value, since the code between points 2 and 3 has managed to return
 * to us.
 ****************************************************************************
 */

EXPORT(rm_stack):	/* comprises rm_ss and rm_sp */
rm_sp:		.word 0
rm_ss:		.word 0

EXPORT(pm_stack):
pm_esp:		.long 0

/****************************************************************************
 * Temporary variables
 ****************************************************************************
 */
save_eax:	.long 0
save_ebx:	.long 0
save_retaddr:	.long 0
	
/****************************************************************************
 * End of librm
 ****************************************************************************
 */
_librm_end:
	.globl _librm_size
	.equ _librm_size, _librm_end - _librm_start
