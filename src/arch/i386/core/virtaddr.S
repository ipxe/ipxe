/*
 * Functions to support the virtual addressing method of relocation
 * that Etherboot uses.
 *
 */

#include "virtaddr.h"
		
	.arch i386

/****************************************************************************
 * GDT for initial transition to protected mode
 *
 * The segment values, PHYSICAL_CS et al, are defined in an external
 * header file virtaddr.h, since they need to be shared with librm.
 ****************************************************************************
 */
	.data
	.align 16

gdt:
gdt_limit:		.word gdt_length - 1
gdt_addr:		.long 0
			.word 0 /* padding */
	
	.org	gdt + PHYSICAL_CS
physical_cs:
	/* 32 bit protected mode code segment, physical addresses */
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0
	
	.org	gdt + PHYSICAL_DS
physical_ds:
	/* 32 bit protected mode data segment, physical addresses */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0
	
	.org	gdt + VIRTUAL_CS
virtual_cs:
	/* 32 bit protected mode code segment, virtual addresses */
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0

	.org	gdt + VIRTUAL_DS
virtual_ds:	
	/* 32 bit protected mode data segment, virtual addresses */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0

#ifdef CONFIG_X86_64
	
	.org	gdt + LONG_CS
long_cs:
	/* 64bit long mode code segment, base 0 */
	.word	0xffff, 0
	.byte	0x00, 0x9f, 0xaf , 0x00

	.org	gdt + LONG_DS
long_ds:
	/* 64bit long mode data segment, base 0 */
	.word	0xffff, 0
	.byte	0x00, 0x93, 0xcf, 0x00
	
#endif /* CONFIG_X86_64 */
	
gdt_end:		
	.equ	gdt_length, gdt_end - gdt

	/* The virtual address offset  */	
	.globl virt_offset
virt_offset:	.long  	0
		
	.text
	.code32
	
/****************************************************************************
 * run_here (flat physical addressing, position-independent)
 *
 * Set up a GDT to run Etherboot at the current location with virtual
 * addressing.  This call does not switch to virtual addresses or move
 * the stack pointer.  The GDT will be located within the copy of
 * Etherboot.  All registers are preserved.
 *
 * This gets called at startup and at any subsequent relocation of
 * Etherboot.
 *
 * Parameters: none
 ****************************************************************************
 */
	.globl	run_here
run_here:
	/* Preserve registers */
	pushl	%eax
	pushl	%ebp

	/* Find out where we're running */
	call	1f
1:	popl	%ebp
	subl	$1b, %ebp

	/* Store as virt_offset */
	movl	%ebp, virt_offset(%ebp)

	/* Set segment base addresses in GDT */
	leal	virtual_cs(%ebp), %eax
	pushl	%eax
	pushl	%ebp
	call	set_seg_base
	popl	%eax /* discard */
	popl	%eax /* discard */

	/* Set physical location of GDT */
	leal	gdt(%ebp), %eax
	movl	%eax, gdt_addr(%ebp)

	/* Load the new GDT */
	lgdt	gdt(%ebp)

	/* Reload new flat physical segment registers */
	movl	$PHYSICAL_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %fs
	movl	%eax, %gs
	movl	%eax, %ss

	/* Restore registers, convert return address to far return
	 * address.
	 */
	popl	%ebp
	movl	$PHYSICAL_CS, %eax
	xchgl	%eax, 4(%esp)	/* cs now on stack, ret offset now in eax */
	xchgl	%eax, 0(%esp)	/* ret offset now on stack, eax restored */
	
	/* Return to caller, reloading %cs with new value */
	lret

/****************************************************************************
 * set_seg_base (any addressing, position-independent)
 *
 * Set the base address of a pair of segments in the GDT.  This relies
 * on the layout of the GDT being (CS,DS) pairs.
 *
 * Parameters:
 *   uint32_t base_address
 *   struct gdt_entry * code_segment
 * Returns:
 *   none
 ****************************************************************************
 */
	.globl set_seg_base
set_seg_base:
	pushl	%eax
	pushl	%ebx
	movl	12(%esp), %eax		/* %eax = base address */
	movl	16(%esp), %ebx		/* %ebx = &code_descriptor */
	movw	%ax, (0+2)(%ebx)	/* CS base bits 0-15 */
	movw	%ax, (8+2)(%ebx)	/* DS base bits 0-15 */
	shrl	$16, %eax
	movb	%al, (0+4)(%ebx)	/* CS base bits 16-23 */
	movb	%al, (8+4)(%ebx)	/* DS base bits 16-23 */
	movb	%ah, (0+7)(%ebx)	/* CS base bits 24-31 */
	movb	%ah, (8+7)(%ebx)	/* DS base bits 24-31 */
	popl	%ebx
	popl	%eax
	ret
	
/****************************************************************************
 * _virt_to_phys (virtual addressing)
 *
 * Switch from virtual to flat physical addresses.  %esp is adjusted
 * to a physical value.  Segment registers are set to flat physical
 * selectors.  All other registers are preserved.  Flags are
 * preserved.
 *
 * Parameters: none
 * Returns: none
 ****************************************************************************
 */
	.globl _virt_to_phys
_virt_to_phys:
	/* Preserve registers and flags */
	pushfl
	pushl	%eax
	pushl	%ebp

	/* Change return address to a physical address */
	movl	virt_offset, %ebp
	addl	%ebp, 12(%esp)

	/* Switch to physical code segment */
	pushl	$PHYSICAL_CS
	leal	1f(%ebp), %eax
	pushl	%eax
	lret
1:
	/* Reload other segment registers and adjust %esp */
	movl	$PHYSICAL_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es	
	movl	%eax, %fs	
	movl	%eax, %gs
	movl	%eax, %ss	
	addl	%ebp, %esp

	/* Restore registers and flags, and return */
	popl	%ebp
	popl	%eax
	popfl
	ret

/****************************************************************************
 * _phys_to_virt (flat physical addressing)
 *
 * Switch from flat physical to virtual addresses.  %esp is adjusted
 * to a virtual value.  Segment registers are set to virtual
 * selectors.  All other registers are preserved.  Flags are
 * preserved.
 *
 * Note that this depends on the GDT already being correctly set up
 * (e.g. by a call to run_here()).
 *
 * Parameters: none
 * Returns: none
 ****************************************************************************
 */
	.globl _phys_to_virt
_phys_to_virt:
	/* Preserve registers and flags */
	pushfl
	pushl	%eax
	pushl	%ebp

	/* Switch to virtual code segment */
	ljmp	$VIRTUAL_CS, $1f
1:	
	/* Reload data segment registers */
	movl	$VIRTUAL_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es	
	movl	%eax, %fs	
	movl	%eax, %gs

	/* Reload stack segment and adjust %esp */
	movl	virt_offset, %ebp
	movl	%eax, %ss	
	subl	%ebp, %esp

	/* Change the return address to a virtual address */
	subl	%ebp, 12(%esp)

	/* Restore registers and flags, and return */
	popl	%ebp
	popl	%eax
	popfl
	ret

/****************************************************************************
 * relocate_to (virtual addressing)
 *
 * Relocate Etherboot to the specified address.  The runtime image
 * (excluding the prefix, decompressor and compressed image) is copied
 * to a new location, and execution continues in the new copy.  This
 * routine is designed to be called from C code.
 *
 * Parameters:
 *	uint32_t new_phys_addr
 ****************************************************************************
 */
	.globl relocate_to
relocate_to:
	/* Save the callee save registers */
	pushl	%ebp
	pushl	%esi
	pushl	%edi

	/* Compute the physical source address and data length */
	movl	$_text, %esi
	movl	$_end, %ecx
	subl	%esi, %ecx
	addl	virt_offset, %esi

	/* Compute the physical destination address */
	movl	16(%esp), %edi

	/* Switch to flat physical addressing */
	call	_virt_to_phys
	
	/* Do the copy */
	cld
	rep movsb

	/* Calculate offset to new image */
	subl	%esi, %edi

	/* Switch to executing in new image */
	call	1f
1:	popl	%ebp
	leal	(2f-1b)(%ebp,%edi), %eax
	jmpl	*%eax
2:
	/* Switch to stack in new image */
	addl	%edi, %esp
	
	/* Call run_here() to set up GDT */
	call	run_here

	/* Switch to virtual addressing */
	call	_phys_to_virt
	
	/* Restore the callee save registers */
	popl	%edi
	popl	%esi
	popl	%ebp

	/* return */
	ret
