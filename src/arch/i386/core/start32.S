#include "virtaddr.h"
	
	.equ	MSR_K6_EFER,   0xC0000080
	.equ	EFER_LME,      0x00000100
	.equ	X86_CR4_PAE,   0x00000020
	.equ	CR0_PG,        0x80000000

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

/*
 * NOTE: if you write a subroutine that is called from C code (gcc/egcs),
 * then you only have to take care of %ebx, %esi, %edi and %ebp.  These
 * registers must not be altered under any circumstance.  All other registers
 * may be clobbered without any negative side effects.  If you don't follow
 * this rule then you'll run into strange effects that only occur on some
 * gcc versions (because the register allocator may use different registers).
 *
 * All the data32 prefixes for the ljmp instructions are necessary, because
 * the assembler emits code with a relocation address of 0.  This means that
 * all destinations are initially negative, which the assembler doesn't grok,
 * because for some reason negative numbers don't fit into 16 bits. The addr32
 * prefixes are there for the same reasons, because otherwise the memory
 * references are only 16 bit wide.  Theoretically they are all superfluous.
 * One last note about prefixes: the data32 prefixes on all call _real_to_prot
 * instructions could be removed if the _real_to_prot function is changed to
 * deal correctly with 16 bit return addresses.  I tried it, but failed.
 */

	.text
	.arch i386
	.code32

	/* This is a struct os_entry_regs */
	.globl os_regs
os_regs:	.space 56
		
/**************************************************************************
XSTART32 - Transfer control to the kernel just loaded
**************************************************************************/
	.globl xstart32
xstart32:
	/* Save the callee save registers */
	movl	%ebp, os_regs + 32
	movl	%esi, os_regs + 36
	movl	%edi, os_regs + 40
	movl	%ebx, os_regs + 44

	/* save the return address */
	popl	%eax
	movl	%eax, os_regs + 48

	/* save the stack pointer */
	movl	%esp, os_regs + 52

	/* Get the new destination address */
	popl	%ecx

	/* Store the physical address of xend on the stack */
	movl	$xend32, %ebx
	addl	virt_offset, %ebx
	pushl	%ebx

	/* Store the destination address on the stack */
	pushl	$PHYSICAL_CS
	pushl	%ecx

	/* Cache virt_offset */
	movl	virt_offset, %ebp
	
	/* Switch to using physical addresses */
	call	_virt_to_phys

	/* Save the target stack pointer */
	movl	%esp, os_regs + 12(%ebp)
	leal	os_regs(%ebp), %esp

	/* Store the pointer to os_regs */
	movl	%esp, os_regs_ptr(%ebp)

	/* Load my new registers */
	popal
	movl	(-32 + 12)(%esp), %esp

	/* Jump to the new kernel
	 * The lret switches to a flat code segment
	 */
	lret

	.balign 4
	.globl xend32
xend32:
	/* Fixup %eflags */
	nop
	cli
	cld
	
	/* Load %esp with &os_regs + virt_offset */
	.byte	0xbc /* movl $0, %esp */
os_regs_ptr:
	.long	0

	/* Save the result registers */
	addl	$32, %esp
	pushal

	/* Compute virt_offset */
	movl	%esp, %ebp
	subl	$os_regs, %ebp
	
	/* Load the stack pointer */
	movl	52(%esp), %esp

	/* Enable the virtual addresses */
	leal	_phys_to_virt(%ebp), %eax
	call	*%eax

	/* Restore the callee save registers */
	movl	os_regs + 32, %ebp
	movl	os_regs + 36, %esi
	movl	os_regs + 40, %edi
	movl	os_regs + 44, %ebx
	movl	os_regs + 48, %edx
	movl	os_regs + 52, %esp

	/* Get the C return value */
	movl	os_regs + 28, %eax

	jmpl	*%edx

#ifdef CONFIG_X86_64
	.arch	sledgehammer
/**************************************************************************
XSTART_lm - Transfer control to the kernel just loaded in long mode
**************************************************************************/
	.globl xstart_lm
xstart_lm:
	/* Save the callee save registers */
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx

	/* Cache virt_offset && (virt_offset & 0xfffff000) */
	movl	virt_offset, %ebp
	movl	%ebp, %ebx
	andl	$0xfffff000, %ebx

	/* Switch to using physical addresses */
	call	_virt_to_phys

	/* Initialize the page tables */
	/* Level 4 */
	leal	0x23 + pgt_level3(%ebx), %eax
	leal	pgt_level4(%ebx), %edi
	movl	%eax, (%edi)

	/* Level 3 */
	leal	0x23 + pgt_level2(%ebx), %eax
	leal	pgt_level3(%ebx), %edi
	movl	%eax, 0x00(%edi)
	addl	$4096, %eax
	movl	%eax, 0x08(%edi)
	addl	$4096, %eax
	movl	%eax, 0x10(%edi)
	addl	$4096, %eax
	movl	%eax, 0x18(%edi)

	/* Level 2 */
	movl	$0xe3, %eax
	leal	pgt_level2(%ebx), %edi
	leal	16384(%edi), %esi
pgt_level2_loop:
	movl	%eax, (%edi)
	addl	$8, %edi
	addl	$0x200000, %eax
	cmp	%esi, %edi
	jne	pgt_level2_loop

	/* Point at the x86_64 page tables */
	leal	pgt_level4(%ebx), %edi
	movl	%edi, %cr3


	/* Setup for the return from 64bit mode */
	/* 64bit align the stack */
	movl	%esp, %ebx		/* original stack pointer + 16 */
	andl	$0xfffffff8, %esp

	/* Save original stack pointer + 16 */
	pushl	%ebx

	/* Save virt_offset */
	pushl	%ebp

	/* Setup for the jmp to 64bit long mode */
	leal	start_lm(%ebp), %eax
	movl	%eax, 0x00 + start_lm_addr(%ebp)
	movl	$LM_CODE_SEG, %eax
	movl	%eax, 0x04 + start_lm_addr(%ebp)

	/* Setup for the jump out of 64bit long mode */
	leal	end_lm(%ebp), %eax
	movl	%eax, 0x00 + end_lm_addr(%ebp)
	movl	$FLAT_CODE_SEG, %eax
	movl	%eax, 0x04 + end_lm_addr(%ebp)

	/* Enable PAE mode */
	movl	%cr4, %eax
	orl	$X86_CR4_PAE, %eax
	movl	%eax, %cr4

	/* Enable long mode */
	movl	$MSR_K6_EFER, %ecx
	rdmsr
	orl	$EFER_LME, %eax
	wrmsr

	/* Start paging, entering 32bit compatiblity mode */
	movl	%cr0, %eax
	orl	$CR0_PG, %eax
	movl	%eax, %cr0

	/* Enter 64bit long mode */
	ljmp	*start_lm_addr(%ebp)
	.code64
start_lm:
	/* Load 64bit data segments */
	movl	$LM_DATA_SEG, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss

	andq	$0xffffffff, %rbx
	/* Get the address to jump to */
	movl	20(%rbx), %edx
	andq	$0xffffffff, %rdx
	
	/* Get the argument pointer */
	movl	24(%rbx), %ebx
	andq	$0xffffffff, %rbx

	/* Jump to the 64bit code */
	call	*%rdx

	/* Preserve the result */
	movl	%eax, %edx

	/* Fixup %eflags */
	cli
	cld

	/* Switch to 32bit compatibility mode */
	ljmp	*end_lm_addr(%rip)

	.code32
end_lm:
	/* Disable paging */
	movl	%cr0, %eax
	andl	$~CR0_PG, %eax
	movl	%eax, %cr0

	/* Disable long mode */
	movl	$MSR_K6_EFER, %ecx
	rdmsr
	andl	$~EFER_LME, %eax
	wrmsr

	/* Disable PAE */
	movl	%cr4, %eax
	andl	$~X86_CR4_PAE, %eax
	movl	%eax, %cr4
	
	/* Compute virt_offset */
	popl	%ebp

	/* Compute the original stack pointer + 16 */
	popl	%ebx
	movl	%ebx, %esp

	/* Enable the virtual addresses */
	leal	_phys_to_virt(%ebp), %eax
	call	*%eax

	/* Restore the callee save registers */
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp

	/* Get the C return value */
	movl	%edx, %eax

	/* Return */
	ret

	.arch i386
#endif /* CONFIG_X86_64 */

#ifdef CONFIG_X86_64
	.section ".bss"
	.p2align 12
	/* Include a dummy space in case we are loaded badly aligned */
	.space 4096
	/* Reserve enough space for a page table convering 4GB with 2MB pages */
pgt_level4:	
	.space 4096
pgt_level3:	
	.space 4096
pgt_level2:	
	.space 16384
start_lm_addr:
	.space	8
end_lm_addr:
	.space	8
#endif
