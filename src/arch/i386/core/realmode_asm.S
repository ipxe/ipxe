/* Real-mode interface: assembly-language portions.
 *
 * Initial version by Michael Brown <mbrown@fensystems.co.uk>, January 2004.
 */

#include "realmode.h"
#include "callbacks.h"

#if	1	/* CODE16 */
	
#define BOCHSBP xchgw %bx,%bx

#define NUM_PUSHA_REGS (8)
#define NUM_SEG_REGS (6)

	.text
	.arch i386
	.section ".text16.nocompress", "ax", @progbits
	.code16
	
	.equ	CR0_PE,1

#ifdef	GAS291
#define DATA32 data32;
#define ADDR32 addr32;
#define	LJMPI(x)	ljmp	x
#else
#define DATA32 data32
#define ADDR32 addr32
/* newer GAS295 require #define	LJMPI(x)	ljmp	*x */
#define	LJMPI(x)	ljmp	x
#endif

/****************************************************************************
 * REAL-MODE CALLBACK INTERFACE
 *
 * This must be copied down to base memory in order for external
 * programs to be able to make calls in to Etherboot.  Store the
 * current physical address of Etherboot (i.e. virt_to_phys(_text)) in
 * (uint32_t)rm_etherboot_location, then copy
 * (uint16_t)rm_callback_interface_size bytes starting at
 * &((void)rm_callback_interface).
 *
 * There are two defined entry points:
 *   Offset RM_IN_CALL     = 0		Near call entry point
 *   Offset RM_IN_CALL_FAR = 2		Far call entry point
 *
 * Note that the routines _prot_to_real and _real_to_prot double as
 * trampoline fragments for external calls (calls from Etherboot to
 * real-mode code).  _prot_to_real does not automatically re-enable
 * interrupts; this is to allow for the potential of using Etherboot
 * code as an ISR.  _real_to_prot does automatically disable
 * interrupts, since we don't have a protected-mode IDT.
 ****************************************************************************
 */

	.globl	rm_callback_interface
	.code16
rm_callback_interface:
	.globl	_rm_in_call
_rm_in_call:
	jmp	_real_in_call
	.globl	_rm_in_call_far
_rm_in_call_far:
	jmp	_real_in_call_far

/****************************************************************************
 * _real_in_call
 *
 * Parameters:
 *   16-bit real-mode near/far return address (implicit from [l]call
 *   to routine) Other parameters as for _in_call_far().
 *
 * This routine will convert the 16-bit real-mode far return address
 * to a 32-bit real-mode far return address, switch to protected mode
 * using _real_to_prot and call in to _in_call_far.
 ****************************************************************************
 */

#define RIC_PRESERVE ( 8 )
#define RIC_OFFSET_CALLADDR ( RIC_PRESERVE )
#define RIC_OFFSET_CALLADDR_E ( RIC_OFFSET_CALLADDR + 4 )
#define RIC_OFFSET_CONTADDR ( RIC_OFFSET_CALLADDR_E )
#define RIC_OFFSET_CONTADDR_E ( RIC_OFFSET_CONTADDR + 4 )
#define RIC_OFFSET_OPCODE ( RIC_OFFSET_CONTADDR_E )
#define RIC_OFFSET_OPCODE_E ( RIC_OFFSET_OPCODE + 4 )
#define RIC_OFFSET_SEG_REGS ( RIC_OFFSET_OPCODE_E )
#define RIC_OFFSET_SEG_REGS_E ( RIC_OFFSET_SEG_REGS + ( NUM_SEG_REGS * 2 ) )
#define RIC_OFFSET_PAD ( RIC_OFFSET_SEG_REGS_E )
#define RIC_OFFSET_PAD_E ( RIC_OFFSET_PAD + 2 )
#define RIC_OFFSET_FLAGS ( RIC_OFFSET_PAD_E )
#define RIC_OFFSET_FLAGS_E ( RIC_OFFSET_FLAGS + 2 )
#define RIC_OFFSET_RETADDR ( RIC_OFFSET_FLAGS_E )
#define RIC_OFFSET_RETADDR_E ( RIC_OFFSET_RETADDR + 4 )
#define RIC_OFFSET_ORIG_OPCODE ( RIC_OFFSET_RETADDR_E )
#define RIC_INSERT_LENGTH ( RIC_OFFSET_OPCODE_E - RIC_OFFSET_CALLADDR )
	
	.code16
_real_in_call:
	/* Expand near return address to far return address
	 */
	pushw	%ax		/* Extend stack, store %ax */
	pushfw
	pushw	%bp
	movw	%sp, %bp
	movw	%cs, %ax
	xchgw	%ax, 6(%bp)
	xchgw	%ax, 4(%bp)	/* also restores %ax */
	popw	%bp
	popfw
	/* Fall through to _real_in_call_far */
	
_real_in_call_far:
	/* Store flags and pad */
	pushfw
	pushw	%ax

	/* Store segment registers.  Order matches that of seg_regs_t */
	pushw	%gs
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%ss
	pushw	%cs

	/* Switch to protected mode */
	call _real_to_prot
	.code32

	/* Allow space for expanded stack */
	subl	$RIC_INSERT_LENGTH, %esp
	
	/* Store temporary registers */
	pushl	%ebp
	pushl	%eax

	/* Copy opcode, set EB_CALL_FROM_REAL_MODE and EP_SKIP_OPCODE.
	 * Copy it because _in_call() and i386_in_call() expect it at
	 * a fixed position, not as part of the va_list.
	 */
	movl	RIC_OFFSET_ORIG_OPCODE(%esp), %eax
	orl	$(EB_CALL_FROM_REAL_MODE|EB_SKIP_OPCODE), %eax
	movl	%eax, RIC_OFFSET_OPCODE(%esp)
	
	/* Set up call and return addresses */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp			/* %ebp = offset */
	movl	rm_etherboot_location(%ebp), %eax  /* Etherboot phys addr */
	subl	$_text, %eax
	addl	$_in_call, %eax			/* _in_call phys addr */
	movl	%eax, RIC_OFFSET_CALLADDR(%esp)
	leal	2f(%ebp), %eax			/* continuation address */
	movl	%eax, RIC_OFFSET_CONTADDR(%esp)
	
	/* Restore temporary registers */
	popl	%eax
	popl	%ebp

	/* Call to _in_call */
	ret
	/* opcode will be popped automatically thanks to EB_SKIP_OPCODE */

2:	/* Continuation point */
	call	_prot_to_real			/* Return to real mode */
	/* Note: the first two words of our segment register store
	 * happens to be exactly what we need to pass as parameters to
	 * _prot_to_real.
	 */
	.code16
	popw	%ds				/* Restore segment registers */
	popw	%ds				/* (skip cs&ss since these   */
	popw	%ds				/* have already been set by  */
	popw	%es				/* _prot_to_real	     */
	popw	%fs
	popw	%gs
	addw	$2, %sp				/* skip pad */

	/* Check for EB_SKIP_OPCODE */
	pushw	%bp
	movw	%sp, %bp
	testl	$EB_SKIP_OPCODE, 6(%bp)
	popw	%bp
	jnz	1f
	/* Normal return */
	popfw					/* Restore interrupt status */
	lret					/* Back to caller */
1:	/* Return and skip opcode */
	popfw
	lret	$4

/****************************************************************************
 * rm_etherboot_location: the current physical location of Etherboot.
 * Needed so that real-mode callback routines can locate Etherboot.
 ****************************************************************************
 */
	.globl rm_etherboot_location
rm_etherboot_location:	.long 0
		
/****************************************************************************
 * _prot_to_real_prefix
 *
 * Trampoline fragment.  Switch from 32-bit protected mode with flat
 * physical addresses to 16-bit real mode.  Store registers in the
 * trampoline for restoration by _real_to_prot_suffix.  Switch to
 * stack in base memory.
 ****************************************************************************
 */
	
	.globl _prot_to_real_prefix
	.code32
_prot_to_real_prefix:
	/* Registers to preserve */
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp

	/* Calculate offset */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp		/* %ebp = offset for labels in p2r*/

	/* Preserve registers and return address in r2p_params */
	movl	p2r_r2p_params(%ebp), %ebx
	subl	$r2p_params, %ebx	/* %ebx = offset for labels in r2p */
	popl	r2p_ebp(%ebx)
	popl	r2p_edi(%ebx)
	popl	r2p_esi(%ebx)
	popl	r2p_ebx(%ebx)
	popl	r2p_ret_addr(%ebx)
	movl	%esp, r2p_esp(%ebx)

	/* Switch stacks */
	movl	p2r_esp(%ebp), %esp

	/* Switch to real mode */
	pushl	p2r_segments(%ebp)
	call	_prot_to_real
	.code16
	addw	$4, %sp
		
	/* Fall through to next trampoline fragment */
	jmp	_prot_to_real_prefix_end
	
/****************************************************************************
 * _prot_to_real
 *
 * Switch from 32-bit protected mode with flat physical addresses to
 * 16-bit real mode.  Stack and code must be in base memory when
 * called.  %cs, %ss, %eip, %esp are changed to real-mode values,
 * other segment registers are destroyed, all other registers are
 * preserved.  Interrupts are *not* enabled.
 *
 * Parameters:
 *   %cs		Real-mode code segment (word)
 *   %ss		Real-mode stack segment (word)
 ****************************************************************************
 */

#define P2R_PRESERVE ( 12 )
#define P2R_OFFSET_RETADDR ( P2R_PRESERVE )
#define P2R_OFFSET_RETADDR_E ( P2R_OFFSET_RETADDR + 4 )
#define P2R_OFFSET_CS ( P2R_OFFSET_RETADDR_E )
#define P2R_OFFSET_CS_E ( P2R_OFFSET_CS + 2 )
#define P2R_OFFSET_SS ( P2R_OFFSET_CS_E )
#define P2R_OFFSET_SS_E ( P2R_OFFSET_SS + 2 )

	.globl _prot_to_real
	.code32
_prot_to_real:
	/* Preserve registers */
	pushl	%ebp
	pushl	%ebx
	pushl	%eax
	
	/* Calculate offset */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp		/* %ebp = offset for labels in p2r*/

	/* Set up GDT with real-mode limits and appropriate bases for
	 * real-mode %cs and %ss.  Set up protected-mode continuation
	 * point on stack.
	 */
	/* Fixup GDT */
	leal	p2r_gdt(%ebp), %eax
	movl	%eax, p2r_gdt_addr(%ebp)

	/* Calculate CS base address: set GDT code segment, adjust
	 * return address, set up continuation address on stack.
	 */
	movzwl	P2R_OFFSET_CS(%esp), %eax
	shll	$4, %eax
	/* Change return address to real-mode far address */
	subl	%eax, P2R_OFFSET_RETADDR(%esp)
	movl	%eax, %ebx
	shrl	$4, %ebx
	movw	%bx, (P2R_OFFSET_RETADDR+2)(%esp)
	/* First real mode address */
	movl	%eax, %ebx
	shrl	$4, %ebx
	pushw	%bx
	leal	3f(%ebp), %ebx
	subl	%eax, %ebx
	pushw	%bx
	/* Continuation address */
	pushl	$(p2r_rmcs - p2r_gdt)
	leal	2f(%ebp), %ebx
	subl	%eax, %ebx
	pushl	%ebx
	/* Code segment in GDT */
	movw	%ax, (p2r_rmcs+2)(%ebp)
	shrl	$16, %eax			/* Remainder of cs base addr */
	movb	%al, (p2r_rmcs+4)(%ebp)
	movb	%ah, (p2r_rmcs+7)(%ebp)

	/* Calculate SS base address: set GDT data segment, retain to
	 * use for adjusting %esp.
	 */
	movzwl	(12+P2R_OFFSET_SS)(%esp), %eax	/* Set ss base address */
	shll	$4, %eax
	movw	%ax, (p2r_rmds+2)(%ebp)
	movl	%eax, %ebx
	shrl	$16, %ebx
	movb	%bl, (p2r_rmds+4)(%ebp)
	movb	%bh, (p2r_rmds+7)(%ebp)

	/* Load GDT */
	lgdt	p2r_gdt(%ebp)
	/* Reload all segment registers and adjust %esp */
	movw	$(p2r_rmds - p2r_gdt), %bx /* Pmode DS */
	movw	%bx, %ss
	subl	%eax, %esp		/* %esp now less than 0x10000 */
	movw	%bx, %ds
	movw	%bx, %es
	movw	%bx, %fs
	movw	%bx, %gs
	lret				/* %cs:eip */
2:	/* Segment registers now have 16-bit limits. */
	.code16

	/* Switch to real mode */
	movl	%cr0, %ebx
	andb	$0!CR0_PE, %bl
	movl	%ebx, %cr0

	/* Make intersegment jmp to flush the processor pipeline
	 * and reload %cs:%eip (to clear upper 16 bits of %eip).
	 */
	lret
3:		
	
	/* Load real-mode segment value to %ss.  %sp already OK */
	shrl	$4, %eax
	movw	%ax, %ss

	/* Restore registers */
	popl	%eax
	popl	%ebx
	popl	%ebp

	/* Return to caller in real-mode */
	lret

#ifdef FLATTEN_REAL_MODE
#define RM_LIMIT_16_19__AVL__SIZE__GRANULARITY 0x8f
#else
#define RM_LIMIT_16_19__AVL__SIZE__GRANULARITY 0x00
#endif

p2r_gdt:
p2r_gdtarg:
p2r_gdt_limit:		.word p2r_gdt_end - p2r_gdt - 1
p2r_gdt_addr:		.long 0
p2r_gdt_padding:	.word 0
p2r_rmcs:
	/* 16 bit real mode code segment */
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x9b,RM_LIMIT_16_19__AVL__SIZE__GRANULARITY,(0>>24)
p2r_rmds:
	/* 16 bit real mode data segment */
	.word	0xffff,(0&0xffff)
	.byte	(0>>16),0x93,RM_LIMIT_16_19__AVL__SIZE__GRANULARITY,(0>>24)
p2r_gdt_end:

	/* This is the end of the trampoline prefix code.  When used
	 * as a prefix, fall through to the following code in the
	 * trampoline.
	 */
p2r_params: /* Structure must match prot_to_real_params_t in realmode.h */
p2r_esp:	.long 0
p2r_segments:
p2r_cs:		.word 0
p2r_ss:		.word 0
p2r_r2p_params:	.long 0
	.globl	_prot_to_real_prefix_end
_prot_to_real_prefix_end:
	
	.globl	_prot_to_real_prefix_size
	.equ	_prot_to_real_prefix_size, _prot_to_real_prefix_end - _prot_to_real_prefix
	.globl	prot_to_real_prefix_size
prot_to_real_prefix_size:	
	.word	_prot_to_real_prefix_size
	
/****************************************************************************
 * _real_to_prot_suffix
 *
 * Trampoline fragment.  Switch from 16-bit real-mode to 32-bit
 * protected mode with flat physical addresses.  Copy returned stack
 * parameters to output_stack.  Restore registers preserved by
 * _prot_to_real_prefix.  Restore stack to previous location.
 ****************************************************************************
 */

	.globl _real_to_prot_suffix
	.code16
_real_to_prot_suffix:

	/* Switch to protected mode */
	call	_real_to_prot
	.code32

	/* Calculate offset */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp		/* %ebp = offset for labels in r2p */

	/* Copy stack to out_stack */
	movl	r2p_out_stack(%ebp), %edi
	movl	r2p_out_stack_len(%ebp), %ecx
	movl	%esp, %esi
	cld
	rep movsb

	/* Switch back to original stack */
	movl	r2p_esp(%ebp), %esp

	/* Restore registers and return */
	pushl	r2p_ret_addr(%ebp)	/* Set up return address on stack */
	movl	r2p_ebx(%ebp), %ebx
	movl	r2p_esi(%ebp), %esi
	movl	r2p_edi(%ebp), %edi
	movl	r2p_ebp(%ebp), %ebp
	ret

/****************************************************************************
 * _real_to_prot
 *
 * Switch from 16-bit real-mode to 32-bit protected mode with flat
 * physical addresses.  All segment registers are destroyed, %eip and
 * %esp are changed to flat physical values, all other registers are
 * preserved.  Interrupts are disabled.
 *
 * Parameters: none
 ****************************************************************************
 */

#define R2P_PRESERVE ( 12 )
#define R2P_OFFSET_RETADDR ( R2P_PRESERVE )
#define R2P_OFFSET_ORIG_RETADDR ( R2P_OFFSET_RETADDR + 2 )

	.globl _real_to_prot
	.code16		
_real_to_prot:
	/* Disable interrupts */
	cli
	/* zero extend the return address */
	pushw	$0

	/* Preserve registers */
	pushl	%ebp
	pushl	%ebx
	pushl	%eax

	/* Convert 16-bit real-mode near return address to
	 * 32-bit pmode physical near return address
	 */
	movw	%sp, %bp
	xorl	%ebx, %ebx
	push	%cs
	popw	%bx
	movw	%bx, %ds
	shll	$4, %ebx
	movzwl	%ss:R2P_OFFSET_ORIG_RETADDR(%bp), %eax
	addl	%ebx, %eax
	movl	%eax, %ss:(R2P_OFFSET_RETADDR)(%bp)

	/* Store the code segment physical base address in %ebp */
	movl	%ebx, %ebp

	/* Find the offset within the code segment that I am running at */
	xorl	%ebx, %ebx
	call	1f
1:	popw	%bx

	/* Set up GDT */
	leal	(r2p_gdt-1b)(%bx), %eax	/* %ds:ebx = %ds:bx = &(r2p_gdt) */
	addl	%ebp, %eax		/* %eax = &r2p_gdt (physical) */
	movl	%eax, %ds:(r2p_gdt-1b+2)(%bx) /* Set phys. addr. in r2p_gdt */

	/* Compute the first protected mode physical address */
	leal	(2f-1b)(%bx), %eax
	addl	%ebp, %eax
	movl	%eax, %ds:(r2p_paddr-1b)(%bx)

	/* Calculate new %esp */
	xorl	%eax, %eax
	push	%ss
	popw	%ax
	shll	$4, %eax
	movzwl	%sp, %ebp
	addl	%eax, %ebp		/* %ebp = new %esp */
	
	/* Load GDT */
	DATA32 lgdt %ds:(r2p_gdt-1b)(%bx)	/* Load GDT */

	/* Switch to protected mode */
	movl	%cr0, %eax
	orb	$CR0_PE, %al
	movl	%eax, %cr0

	/* flush prefetch queue, and reload %cs:%eip */
	DATA32 ljmp %ds:(r2p_paddr-1b)(%bx)
	.code32
2:
	
	/* Load segment registers, adjust %esp */
	movw	$(r2p_pmds-r2p_gdt), %ax
	movw	%ax, %ss
	movl	%ebp, %esp
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	/* Restore registers */
	popl	%eax
	popl	%ebx
	popl	%ebp

	/* return to caller */
	ret

r2p_gdt:
	.word	r2p_gdt_end - r2p_gdt - 1	/* limit */
	.long 0					/* addr */
	.word 0
r2p_pmcs:
	/* 32 bit protected mode code segment, physical addresses */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0
r2p_pmds:
	/* 32 bit protected mode data segment, physical addresses */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0
r2p_gdt_end:

r2p_paddr:
	.long 2b
	.word r2p_pmcs - r2p_gdt, 0


	/* This is the end of the trampoline suffix code.
	 */
r2p_params: /* Structure must match real_to_prot_params_t in realmode.h */
r2p_ret_addr:		.long 0
r2p_esp:		.long 0
r2p_ebx:		.long 0
r2p_esi:		.long 0
r2p_edi:		.long 0
r2p_ebp:		.long 0
r2p_out_stack:		.long 0
r2p_out_stack_len:	.long 0
	.globl	_real_to_prot_suffix_end
_real_to_prot_suffix_end:

	.globl	_real_to_prot_suffix_size
	.equ	_real_to_prot_suffix_size, _real_to_prot_suffix_end - _real_to_prot_suffix
	.globl	real_to_prot_suffix_size
real_to_prot_suffix_size:
	.word	_real_to_prot_suffix_size

rm_callback_interface_end:

	.globl	_rm_callback_interface_size
	.equ	_rm_callback_interface_size, rm_callback_interface_end - rm_callback_interface
	.globl	rm_callback_interface_size
rm_callback_interface_size:
	.word	_rm_callback_interface_size

/****************************************************************************
 * END OF REAL-MODE CALLBACK INTERFACE
 ****************************************************************************
 */


#ifdef PXE_EXPORT
/****************************************************************************
 * PXE CALLBACK INTERFACE
 *
 * Prepend this to rm_callback_interface to create a real-mode PXE
 * callback interface.
 ****************************************************************************
 */
	.section ".text16", "ax", @progbits
	.globl	pxe_callback_interface
	.code16
pxe_callback_interface:

/* Macro to calculate offset of labels within code segment in
 * installed copy of code.
 */
#define INSTALLED(x) ( (x) - pxe_callback_interface )

/****************************************************************************
 * PXE entry points (!PXE and PXENV+ APIs)
 ****************************************************************************
 */
	/* in_call mechanism for !PXE API calls */
	.globl	_pxe_in_call_far
_pxe_in_call_far:
	/* Prepend "PXE API call" and "API version 0x201" to stack */
	pushl	$0x201
	jmp	1f
	/* in_call mechanism for PXENV+ API calls */
	.globl	_pxenv_in_call_far
_pxenv_in_call_far:
	/* Prepend "PXE API call" and "API version 0x200" to stack */
	pushl	$0x200
1:	pushl	$EB_OPCODE_PXE
	/* Perform real-mode in_call */
	call	pxe_rm_in_call
	/* Return */
	addw	$8, %sp
	lret

/****************************************************************************
 * PXE installation check (INT 1A) code
 ****************************************************************************
 */
	.globl	_pxe_intercept_int1a
_pxe_intercept_int1a:
	pushfw
	cmpw	$0x5650, %ax
	jne	2f
1:	/* INT 1A,5650 - Intercept */
	popfw
	/* Set up return values according to PXE spec: */
	movw	$0x564e, %ax		/* AX := 564Eh (VN) */
	pushw	%cs:INSTALLED(_pxe_pxenv_segment)
	popw	%es			/* ES:BX := &(PXENV+ structure) */
	movw	%cs:INSTALLED(_pxe_pxenv_offset), %bx
	clc				/* CF is cleared */
	lret	$2			/* 'iret' without reloading flags */
2:	/* INT 1A,other - Do not intercept */
	popfw
	ljmp	%cs:*INSTALLED(_pxe_intercepted_int1a)

	.globl	_pxe_intercepted_int1a
_pxe_intercepted_int1a:	.word 0,0
	.globl	_pxe_pxenv_location
_pxe_pxenv_location:
_pxe_pxenv_offset:	.word 0
_pxe_pxenv_segment:	.word 0
	
pxe_rm_in_call:	
pxe_attach_rm:
	/* rm_callback_interface must be appended here */

pxe_callback_interface_end:

	.globl	_pxe_callback_interface_size
	.equ	_pxe_callback_interface_size, pxe_callback_interface_end - pxe_callback_interface
	.globl	pxe_callback_interface_size
pxe_callback_interface_size:
	.word	_pxe_callback_interface_size

#else	/* PXE_EXPORT */
	
/* Define symbols used by the linker scripts, to prevent link errors */
	.globl	_pxe_callback_interface_size
	.equ	_pxe_callback_interface_size, 0	
	
#endif	/* PXE_EXPORT */

#else	/* CODE16 */

/* Define symbols used by the linker scripts, to prevent link errors */
	.globl	_rm_callback_interface_size
	.equ	_rm_callback_interface_size, 0	
	.globl	_pxe_callback_interface_size
	.equ	_pxe_callback_interface_size, 0	
		
#endif	/* CODE16 */
