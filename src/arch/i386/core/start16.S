/*****************************************************************************
 *
 * THIS FILE IS NOW OBSOLETE.
 *
 * The functions of this file are now placed in init.S.
 *
 *****************************************************************************
 */
	
#ifndef PCBIOS
#error	"16bit code is only supported with the PCBIOS"
#endif

#define	CODE_SEG 0x08
#define DATA_SEG 0x10

#define EXEC_IN_SITU_MAGIC	0x45524548	/* 'HERE' */

	.equ CR0_PE, 1

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

/*****************************************************************************
 *
 * start16 : move payload to desired area of memory, set up for exit
 *	     back to prefix, set up for 32-bit code.
 *
 * Enter (from prefix) with es:di = 0x4552:0x4548 if you want to
 * prevent start16 from moving the payload.  There are three
 * motivations for moving the payload:
 * 
 * 1. It may be in ROM, in which case we need to move it to RAM.
 * 2. Whatever loaded us probably didn't know about our memory usage
 *    beyond the end of the image file.  We should claim this memory
 *    before using it.
 *
 * Unless the prefix instructs us otherwise we will move the payload to:
 *
 *    An area of memory claimed from the BIOS via 40:13.
 *
 * We use the main Etherboot stack (within the image target) as our
 * stack; we don't rely on the prefix passing us a stack usable for
 * anything other than the prefix's return address.  The (first 512
 * bytes of the) prefix code segment is copied to a safe archive
 * location.
 *
 * When we return to the prefix (from start32), we copy this code back
 * to a new area of memory, restore the prefix's ss:sp and ljmp back
 * to the copy of the prefix.  The prefix will see a return from
 * start16 *but* may be executing at a new location.  Code following
 * the lcall to start16 must therefore be position-independent and
 * must also be within [cs:0000,cs:01ff].  We make absolutely no
 * guarantees about the stack contents when the prefix regains
 * control.
 * 
 * Trashes just about all registers, including all the segment
 * registers.
 *
 *****************************************************************************
 */

	.text
	.code16
	.arch i386
	.org 0
	.globl _start16
_start16:

/*****************************************************************************
 * Work out where we are going to place our image (image = optional
 * decompressor + runtime).  Exit this stage with %ax containing the
 * runtime target address divided by 16 (i.e. a real-mode segment
 * address).
 *****************************************************************************
 */	
	movw	%es, %ax
	cmpw	$(EXEC_IN_SITU_MAGIC >> 16), %ax
	jne	exec_moved
	cmpw	$(EXEC_IN_SITU_MAGIC & 0xffff), %di
	jne	exec_moved
exec_in_situ: 
	/* Prefix has warned us not to move the payload.  Simply
	 * calculate where the image is going to end up, so we can
	 * work out where to put our stack.
	 */
	movw	%cs, %ax
	addw	$((payload-_start16)/16), %ax
	jmp	99f
exec_moved:
	/* Claim an area of base memory from the BIOS and put the
	 * payload there.  arch_relocated_to() will deal with freeing
	 * up this memory once we've relocated to high memory.
	 */
	movw	$0x40, %ax
	movw	%ax, %es
	movw	%es:(0x13), %ax		/* FBMS in kb to %ax */
	shlw	$6, %ax			/* ... in paragraphs */
	subw	$__image_size_pgh, %ax	/* Subtract space for image */
	shrw	$6, %ax			/* Round down to nearest kb */
	movw	%ax, %es:(0x13)		/* ...and claim memory from BIOS */
	shlw	$6, %ax
99:
	/* At this point %ax contains the segment address for the
	 * start of the image (image = optional decompressor + runtime).
	 */

/*****************************************************************************
 * Set up stack in start32's stack space within the place we're going
 * to copy Etherboot to, reserve space for GDT, copy return address
 * from prefix stack, store prefix stack address
 *****************************************************************************
 */
	popl	%esi			/* Return address */
	mov	%ss, %bx		/* %es:di = prefix stack address   */
	mov	%bx, %es		/* (*after* pop of return address) */
	movw	%sp, %di
	movw	$__offset_stack_pgh, %bx	/* Set up Etherboot stack */
	addw	%ax, %bx
	movw	%bx, %ss
	movw	$__stack_size, %sp
	subw	$(_gdt_end - _gdt), %sp	/* Reserve space for GDT */
	movw	%sp, %bp		/* Record GDT location */	
	/* Set up i386_rm_in_call_data_t structure on stack.  This is
	 * the same structure as is set up by rm_in_call.
	 */
	pushl	$0			/* Dummy opcode */
	pushl	%esi			/* Prefix return address */
	pushfw				/* Flags */
	pushw	%di			/* Prefix %sp */
	pushw	%gs			/* Segment registers */
	pushw	%fs
	pushw	%es
	pushw	%ds
	pushw	%es			/* Prefix %ss */
	pushw	%cs
	/* Stack is now 32-bit aligned */
	
	/* %ax still contains image target segment address */

/*****************************************************************************
 * Calculate image target and prefix code physical addresses, store on stack
 * for use in copy routine.
 *****************************************************************************
 */
	movzwl	%es:-2(%di), %ebx	/* Prefix code segment */
	shll	$4, %ebx
	pushl	%ebx			/* Prefix code physical address */
	movzwl	%ax, %edi		/* Image target segment */
	shll	$4, %edi
	pushl	%edi			/* Image target physical address */

/*****************************************************************************
 * Transition to 32-bit protected mode.  Set up all segment
 * descriptors to use flat physical addresses.
 *****************************************************************************
 */
	/* Copy gdt to area reserved on stack
	 */
	push	%cs			/* GDT source location -> %ds:%si */
	pop	%ds
	mov	$(_gdt - _start16), %si
	push	%ss			/* GDT target location -> %es:%di */
	pop	%es
	mov	%bp, %di
	mov	$(_gdt_end - _gdt), %cx
	cld
	rep movsb			/* Copy GDT to stack */
	movl	%ss, %eax
	shll	$4, %eax
	movzwl	%bp, %ebx
	addl	%eax, %ebx		/* Physical addr of GDT copy -> %ebx */
	movl	%ebx, 2(%bp)		/* Fill in addr field in GDT */
		
	/* Compute the offset I am running at.
	 */
	movl	%cs, %ebx 
	shll	$4, %ebx		/* %ebx = offset for start16 symbols */

	/* Switch to 32bit protected mode.
	 */
	cli				/* Disable interrupts */
	lgdt	(%bp)			/* Load GDT from stack */
	movl	%cr0, %eax		/* Set protected mode bit */
	orb	$CR0_PE, %al
	movl	%eax, %cr0
	movl	%ss, %eax		/* Convert stack pointer to 32bit */
	shll	$4, %eax
	movzwl	%sp, %esp
	addl	%eax, %esp
	movl	$DATA_SEG, %eax		/* Reload the segment registers */
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs
	/* Flush prefetch queue, and reload %cs:%eip by effectively ljmping
	 * to code32_start.  Do the jump via pushl and lret because the text
	 * may not be writable/
	 */
	pushl	$CODE_SEG
	ADDR32 leal (code32_start-_start16)(%ebx), %eax
	pushl	%eax	
	DATA32 lret /* DATA32 needed, because we're still in 16-bit mode */

_gdt:
gdtarg:
	.word	_gdt_end - _gdt - 1	/* limit */
	.long 0				/* addr */
	.word 0
_pmcs:
	/* 32 bit protected mode code segment */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0
_pmds:
	/* 32 bit protected mode data segment */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0
_gdt_end:

	.code32
code32_start:	
	
/*****************************************************************************
 * Copy payload to target location.  Do the copy backwards, since if
 * there's overlap with a forward copy then it means start16 is going
 * to get trashed during the copy anyway...
 *****************************************************************************
 */
	popl	%edi		/* Image target physical address */
	pushl	%edi
	leal	(payload-_start16)(%ebx), %esi /* Image source physical addr */
	movl	$__payload_size, %ecx /* Payload size (not image size) */
	addl	%ecx, %edi	/* Start at last byte (length - 1) */
	decl	%edi
	addl	%ecx, %esi
	decl	%esi
	std			/* Backward copy of image */
	rep movsb
	cld
	popl	%edi		/* Restore image target physical address */
	leal	__decompressor_uncompressed(%edi), %ebx
	subl	$_text, %ebx /* %ebx = offset for runtime symbols */

/*****************************************************************************
 * Copy prefix to storage area within Etherboot image.
 *****************************************************************************
 */
	popl	%esi		/* Prefix source physical address */
	pushl	%edi
	leal	_prefix_copy(%ebx), %edi /* Prefix copy phys. addr. */
	leal	_eprefix_copy(%ebx), %ecx
	subl	%edi, %ecx	/* Prefix copy size */
	rep movsb		/* Forward copy of prefix */
	popl	%edi		/* Restore image target physical address */

/*****************************************************************************
 * Record base memory used by Etherboot image
 *****************************************************************************
 */
	movl	%edi, _prefix_image_basemem (%ebx)

/*****************************************************************************
 * Jump to start of the image (i.e. the decompressor, or start32 if
 * non-compressed).
 *****************************************************************************
 */
	pushl	$0		/* Inform start32 that exit path is 16-bit */
	jmpl	*%edi		/* Jump to image */
	
	.balign 16
	/* Etherboot needs to be 16byte aligned or data that
	 * is virtually aligned is no longer physically aligned
	 * which is just nasty in general.  16byte alignment 
	 * should be sufficient though.
	 */
payload:
