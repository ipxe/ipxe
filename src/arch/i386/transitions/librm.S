/*
 * librm: a library for interfacing to real-mode code
 *
 * Michael Brown <mbrown@fensystems.co.uk>
 *
 */

/* Drag in local definitions */
#include "librm.h"

/* For switches to/from protected mode */
#define CR0_PE 1

/* Size of various C data structures */
#define SIZEOF_I386_SEG_REGS	12
#define SIZEOF_I386_REGS	32
#define SIZEOF_REAL_MODE_REGS	( SIZEOF_I386_SEG_REGS + SIZEOF_I386_REGS )
#define SIZEOF_I386_FLAGS	4
#define SIZEOF_I386_ALL_REGS	( SIZEOF_REAL_MODE_REGS + SIZEOF_I386_FLAGS )
	
	.arch i386
	.section ".text16", "awx", @progbits

/****************************************************************************
 * Global descriptor table
 *
 * Call init_gdt to set up the GDT before attempting to use any
 * protected-mode code.
 *
 * Define FLATTEN_REAL_MODE if you want to use so-called "flat real
 * mode" with 4GB limits instead.
 *
 * NOTE: This must be located before prot_to_real, otherwise gas
 * throws a "can't handle non absolute segment in `ljmp'" error due to
 * not knowing the value of REAL_CS when the ljmp is encountered.
 *
 * Note also that putting ".word gdt_end - gdt - 1" directly into
 * gdt_limit, rather than going via gdt_length, will also produce the
 * "non absolute segment" error.  This is most probably a bug in gas.
 ****************************************************************************
 */
	
#ifdef FLATTEN_REAL_MODE
#define RM_LIMIT_16_19__AVL__SIZE__GRANULARITY 0x8f
#else
#define RM_LIMIT_16_19__AVL__SIZE__GRANULARITY 0x00
#endif
	.section ".text16"
	.align 16
gdt:
gdt_limit:		.word gdt_length - 1
gdt_base:		.long 0
			.word 0 /* padding */

	.org	gdt + VIRTUAL_CS, 0
virtual_cs:	/* 32 bit protected mode code segment, virtual addresses */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	.org	gdt + VIRTUAL_DS, 0
virtual_ds:	/* 32 bit protected mode data segment, virtual addresses */
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0
	
	.org	gdt + PHYSICAL_CS, 0
physical_cs:	/* 32 bit protected mode code segment, physical addresses */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	.org	gdt + PHYSICAL_DS, 0
physical_ds:	/* 32 bit protected mode data segment, physical addresses */
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0	

	.org	gdt + REAL_CS, 0
real_cs: 	/* 16 bit real mode code segment */
	.word	0xffff, 0
	.byte	0, 0x9b, RM_LIMIT_16_19__AVL__SIZE__GRANULARITY, 0

	.org	gdt + REAL_DS	
real_ds:	/* 16 bit real mode data segment */
	.word	0xffff, 0
	.byte	0, 0x93, RM_LIMIT_16_19__AVL__SIZE__GRANULARITY, 0
	
gdt_end:
	.equ	gdt_length, gdt_end - gdt

/****************************************************************************
 * init_gdt (real-mode near call, 16-bit real-mode return address)
 *
 * Initialise the GDT ready for transitions to protected mode.
 *
 * Parameters: 
 *   %edi : Physical base of protected-mode code
 ****************************************************************************
 */
	.section ".text16"
	.code16
	.globl init_gdt
init_gdt:
	/* Preserve registers */
	pushl	%eax
	pushw	%bx

	/* Record virt_offset */
	movl	%edi, %cs:virt_offset_rm_copy

	/* Set virtual_cs and virtual_ds base */
	movl	%edi, %eax
	movw	$virtual_cs, %bx
	call	set_seg_base

	/* Set real_cs and real_ds base, and GDT base */
	movw	$real_cs, %bx
	xorl	%eax, %eax
	movw	%cs, %ax
	shll	$4, %eax
	call	set_seg_base
	addl	$gdt, %eax
	movl	%eax, %cs:gdt_base

	/* Restore registers */
	popw	%bx
	popl	%eax
	ret

	.section ".text16"
	.code16
set_seg_base:
	pushl	%eax
	movw	%ax, %cs:(0+2)(%bx)
	movw	%ax, %cs:(8+2)(%bx)
	shrl	$16, %eax
	movb	%al, %cs:(0+4)(%bx)
	movb	%al, %cs:(8+4)(%bx)
	movb	%ah, %cs:(0+7)(%bx)
	movb	%ah, %cs:(8+7)(%bx)
	popl	%eax
	ret
	
/****************************************************************************
 * real_to_prot (real-mode near call, 32-bit virtual return address)
 *
 * Switch from 16-bit real-mode to 32-bit protected mode with virtual
 * addresses.  The real-mode %ss:sp is stored in rm_ss and rm_sp, and
 * the protected-mode %esp is restored from the saved pm_esp.
 * Interrupts are disabled.  All other registers may be destroyed.
 *
 * The return address for this function should be a 32-bit virtual
 * address.
 *
 * Parameters: 
 *   %ecx : number of bytes to move from RM stack to PM stack
 *
 ****************************************************************************
 */
	.section ".text16"
	.code16
real_to_prot:
	/* Protected-mode return address => %ebx */
	popl	%ebx

	/* Real-mode %cs => %dx, %ss => %bp */
	movw	%cs, %dx
	movw	%ss, %bp

	/* virt_offset => %edi */
	movl	%cs:virt_offset_rm_copy, %edi

	/* Switch to protected mode */
	cli
	data32 lgdt	%cs:gdt
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0
	data32 ljmp	$VIRTUAL_CS, $1f
	.section ".text"
	.code32
1:
	/* Set up protected-mode data segments */
	movw	$VIRTUAL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* Record virt_offset */
	movl	%edi, virt_offset

	/* Move data from RM stack to PM stack and set up PM stack */
	movzwl	%sp, %esi
	movl	pm_esp, %esp
	subl	%ecx, %esp
	movl	%esp, %edi
	rep ss movsb
	movw	%ax, %ss

	/* Record real-mode %cs and %ss:sp */
	movw	%dx, rm_cs
	movw	%bp, rm_ss
	movw	%si, rm_sp

	/* Return to virtual address */
	jmp	*%ebx

/****************************************************************************
 * prot_to_real (protected-mode near call, 32-bit real-mode return address)
 *
 * Switch from 32-bit protected mode with virtual addresses to 16-bit
 * real mode.  The protected-mode %esp is stored in pm_esp and the
 * real-mode %ss:sp is restored from the saved rm_ss and rm_sp.  All
 * real-mode data segment registers are set equal to %ss.  Interrupts
 * are *not* enabled, since we want to be able to use prot_to_real in
 * an ISR.  All other registers may be destroyed.
 *
 * The return address for this function should be a 32-bit (sic)
 * real-mode offset within .code16.
 *
 * Parameters: 
 *   %ecx : number of bytes to move from PM stack to RM stack
 *
 ****************************************************************************
 */
	.section ".text"
	.code32
prot_to_real:
	/* Real-mode return address => %ebx */
	popl	%ebx
	
	/* Real-mode %ss:sp => %ebp:edx */
	movzwl	rm_ss, %ebp
	movzwl	rm_sp, %edx
	subl	%ecx, %edx
	
	/* Copy data from PM stack to RM stack */
	movl	%ebp, %eax
	shll	$4, %eax
	leal	(%eax,%edx), %edi
	subl	virt_offset, %edi
	movl	%esp, %esi
	rep movsb
	
	/* Record protected-mode %esp */
	movl	%esi, pm_esp

	/* Real-mode %cs => %di */
	movw	rm_cs, %di

	/* Load real-mode segment limits */
	movw	$REAL_DS, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss
	ljmp	$REAL_CS, $1f
	.section ".text16"
	.code16
1:
	/* Set up real-mode ljmp instruction */
	movw	%di, %ds:(p2r_ljmp + 3)
	
	/* Switch to real mode */
	movl	%cr0, %eax
	andb	$0!CR0_PE, %al
	movl	%eax, %cr0

p2r_ljmp:
	ljmp	$0, $1f /* Segment is filled in by above code */
1:
	/* Set up real-mode stack and data segments, and stack pointer */
	movw	%bp, %ds
	movw	%bp, %es
	movw	%bp, %fs
	movw	%bp, %gs
	movw	%bp, %ss
	movw	%dx, %sp

	/* Return to real-mode address */
	jmp	*%bx
	
/****************************************************************************
 * prot_call (real-mode near call, 32-bit real-mode return address)
 *
 * Call a specific C function in the protected-mode code.  The
 * prototype of the C function must be
 *   void function ( struct i386_all_regs *ix86 ); 
 * ix86 will point to a struct containing the real-mode registers
 * at entry to prot_call.  
 *
 * All registers will be preserved across prot_call(), unless the C
 * function explicitly overwrites values in ix86.  Interrupt status
 * will also be preserved.  Gate A20 will be enabled.
 *
 * Parameters:
 *   function : virtual address of protected-mode function to call
 *
 * Example usage:
 *	pushl	$pxe_api_call
 *	call	prot_call
 *	addw	$4, %sp
 * to call in to the C function
 *      void pxe_api_call ( struct i386_all_regs *ix86 );
 ****************************************************************************
 */

#define PC_OFFSET_IX86 ( 0 )
#define PC_OFFSET_RETADDR ( PC_OFFSET_IX86 + SIZEOF_I386_ALL_REGS )
#define PC_OFFSET_FUNCTION ( PC_OFFSET_RETADDR + 4 )
#define PC_OFFSET_END ( PC_OFFSET_FUNCTION + 4 )

	.section ".text16"
	.code16
	.globl prot_call
prot_call:
	/* Preserve registers and flags on external RM stack */
	pushfl
	pushal
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%ss
	pushw	%cs

	/* For sanity's sake, clear the direction flag as soon as possible */
	cld

	/* Switch to protected mode and move register dump to PM stack */
	movl	$PC_OFFSET_END, %ecx
	pushl	$1f
	jmp	real_to_prot
	.section ".text"
	.code32
1:
	/* Set up environment expected by C code */
	call	gateA20_set

	/* Call function */
	pushl	%esp
	call	*(PC_OFFSET_FUNCTION+4)(%esp)
	popl	%eax /* discard */

	/* Switch to real mode and move register dump back to RM stack */
	movl	$PC_OFFSET_END, %ecx
	pushl	$1f
	jmp	prot_to_real
	.section ".text16"
	.code16
1:	
	/* Restore registers and flags and return */
	popw	%ax	/* skip %cs - it is already set */
	popw	%ax	/* skip %ss - it is already set */
	popw	%ds
	popw	%es
	popw	%fs
	popw	%gs
	popal
	popfl
	data32 ret

/****************************************************************************
 * real_call (protected-mode near call, 32-bit virtual return address)
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
 *   (32-bit) near pointer to real-mode function to call
 *
 * Returns: none
 ****************************************************************************
 */

#define RC_OFFSET_PRESERVE_REGS ( 0 )
#define RC_OFFSET_RETADDR ( RC_OFFSET_PRESERVE_REGS + SIZEOF_I386_REGS )
#define RC_OFFSET_FUNCTION ( RC_OFFSET_RETADDR + 4 )
#define RC_OFFSET_END ( RC_OFFSET_FUNCTION + 4 )

	.section ".text"
	.code32
	.globl real_call
real_call:
	/* Create register dump on PM stack */
	pushal

	/* Switch to real mode and move register dump to RM stack */
	movl	$RC_OFFSET_END, %ecx
	pushl	$1f
	jmp	prot_to_real
	.section ".text16"
	.code16
1:
	/* Construct call to real-mode function */
	movw	%sp, %bp
	movw	RC_OFFSET_FUNCTION(%bp), %ax
	movw	%ax, %cs:rc_function

	/* Call real-mode function */
	popal
	call	*%cs:rc_function
	pushal

	/* Switch to protected mode and move register dump back to PM stack */
	movl	$RC_OFFSET_END, %ecx
	pushl	$1f
	jmp	real_to_prot
	.section ".text"
	.code32
1:
	/* Set up environment expected by C code */
	call	gateA20_set

	/* Restore registers and return */
	popal
	ret

	.section ".text16"
rc_function:	.word 0
	
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

	.section ".data"
	.globl rm_sp
rm_sp:	.word 0
	.globl rm_ss
rm_ss:	.word 0
	.globl rm_cs
rm_cs:	.word 0
	.globl pm_esp
pm_esp:	.long _estack

	.section ".text16"
virt_offset_rm_copy:	.long 0
	.section ".data"
	.globl virt_offset
virt_offset:		.long 0	
