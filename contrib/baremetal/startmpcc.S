/* #defines because ljmp wants a number, probably gas bug */
/*	.equ	KERN_CODE_SEG,_pmcs-_gdt	*/
#define	KERN_CODE_SEG	0x08
	.equ	KERN_DATA_SEG,_pmds-_gdt
/*	.equ	REAL_CODE_SEG,_rmcs-_gdt	*/
#define	REAL_CODE_SEG	0x18
	.equ	REAL_DATA_SEG,_rmds-_gdt
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

#define PIC1_VBS  0x08      /* PIC1 interrupts start at vector 64  */
#define PIC2_VBS  0x70      /* PIC1 interrupts start at vector 112  */

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

/**************************************************************************
START - Where all the fun begins....
**************************************************************************/
/* this must be the first thing in the file because we enter from the top */
	.global	_start
	.code32
_start:
	cli
	
	/* load new IDT and GDT */
	lgdt	gdtarg
	lidt	Idt_Reg
	/* flush prefetch queue, and reload %cs:%eip */
	ljmp	$KERN_CODE_SEG,$1f
1:
	
	/* reload other segment registers */
	movl	$KERN_DATA_SEG,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
        movl    $stktop,%esp

	/* program the PITs in order to stop them */
        mov	$0x30,%al
	out	%al,$0x43
	out	%al,$0x40
        mov	$0x70,%al
	out	%al,$0x43
	out	%al,$0x41
        mov	$0xf0,%al
	out	%al,$0x43
	out	%al,$0x42	

	call	main
	/* fall through */

	.globl	exit
exit:
2:
        ljmp $KERN_CODE_SEG,$2b

/**************************************************************************
MEMSIZE - Determine size of extended memory
**************************************************************************/
	.globl	memsize
memsize:
#if 0
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	call	_prot_to_real
	.code16
	movw	$0xe801,%ax
	stc
	int	$0x15
	jc	1f
	andl	$0xffff,%eax
	andl	$0xffff,%ebx
	shll	$6,%ebx
	addl	%ebx,%eax
	jmp	2f
1:
	movw	$0x8800,%ax
	int	$0x15
	andl	$0xffff,%eax
2:
	movl	%eax,%esi
	DATA32 call	_real_to_prot
	.code32
	movl	%esi,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
#else
	mov	$32768,%eax
#endif
	ret

/**************************************************************************
XSTART - Transfer control to the kernel just loaded
**************************************************************************/
	.code16

	.globl _int08_handler
_int08_handler:
	movb	$0x20, %al
	outb	%al, $0x20
	iret

	.globl _int10_handler
_int10_handler:
	cmp	$0x3, %ah
	jnz	_int10_04
	mov	$0x0, %dx
	mov	$0x0, %cx
	iret
_int10_04:
	cmp	$0x4, %ah
	jnz	_int10_05
	mov	$0x0, %ah
	iret
_int10_05:
	cmp	$0x5, %ah
	jnz	_int10_08
	mov	$0x0, %al
	iret
_int10_08:
	cmp	$0x8, %ah
	jnz	_int10_0D
	mov 	$0x20, %al
	mov 	$0x7,  %ah
	iret
_int10_0D:
	cmp	$0xD, %ah
	jnz	_int10_0F
	mov	$0x0, %al
	iret
_int10_0F:
	cmp	$0xF, %ah
	jnz	_int10_XX
	mov	$0xb, %al
	mov	$80, %ah
	mov	$0, %bh
_int10_XX:
	iret
	
	.globl _int11_handler
_int11_handler:
	mov	$0x22, %ax
	iret
	
	.globl _int12_handler
_int12_handler:
	mov	$640, %ax
	iret
	
	.globl _int13_handler
_int13_handler:
	clc
	mov	$0, %ah
	iret

	.globl _int14_handler
_int14_handler:
	iret

	.globl _int15_handler
_int15_handler:
	cmp	$0xe801,%ax
	jz	_int15_008
	cmp	$0x0, %ah
	jz	_int15_000
	cmp	$0x1, %ah
	jz	_int15_000
	cmp	$0x2, %ah
	jz	_int15_000
	cmp	$0x3, %ah
	jz	_int15_000
	cmp	$0xf, %ah
	jz	_int15_000
	cmp	$0x21, %ah
	jz	_int15_000
	cmp	$0x40, %ah
	jz	_int15_000
	cmp	$0x41, %ah
	jz	_int15_000
	cmp	$0x42, %ah
	jz	_int15_000
	cmp	$0x43, %ah
	jz	_int15_000
	cmp	$0x44, %ah
	jz	_int15_000
	cmp	$0x80, %ah
	jz	_int15_001
	cmp	$0x81, %ah
	jz	_int15_001
	cmp	$0x82, %ah
	jz	_int15_002
	cmp	$0x83, %ah
	jz	_int15_003
	cmp	$0x84, %ah
	jz	_int15_000
	cmp	$0x85, %ah
	jz	_int15_004
	cmp	$0x86, %ah
	jz	_int15_003
	cmp	$0x87, %ah
	jz	_int15_005
	cmp	$0x88, %ah
	jz	_int15_006
	cmp	$0x89, %ah
	jz	_int15_005
	cmp	$0x90, %ah
	jz	_int15_007
	cmp	$0xc0, %ah
	jz	_int15_000
	cmp	$0xc1, %ah
	jz	_int15_000
	cmp	$0xc2, %ah
	jz	_int15_000
	cmp	$0xc3, %ah
	jz	_int15_000
	cmp	$0xc4, %ah
	jz	_int15_000
	iret

_int15_000:
	mov	$0x86, %ah
	stc
	iret

_int15_001:
	mov	$0, %bx
	mov	$0, %cx
	iret

_int15_002:
	mov	$0, %bx
	iret

_int15_003:
	clc
	iret

_int15_004:
	mov	$0, %al
	iret

_int15_005:
	mov	$0, %ah
	clc
	cmp 	$0, %ah
	iret

_int15_006:
	mov	$0xf000, %ax
	iret

_int15_007:
	stc
	iret

_int15_008:
	clc
	mov	$1024, %dx	/* dx -> extended memory size (in 64K chuncks) */
	mov	$640, %cx	/* cx -> conventional memory size (in 1 Kbytes chuncks) */
	iret

	.globl _int16_handler
_int16_handler:
	cmp	$0x0, %ah
	jnz	_int16_01
	mov	$0x20, %al
	mov 	$0x39, %ah
	iret
_int16_01:
	cmp	$0x1, %ah
	jnz	_int16_02
	iret
_int16_02:
	cmp	$0x2, %ah
	jnz	_int16_05
	mov	$0, %al
	iret
_int16_05:
	cmp	$0x5, %ah
	jnz	_int16_10
	mov	$0, %al
	iret
_int16_10:
	cmp	$0x10, %ah
	jnz	_int16_11
	mov	$0x20, %al
	mov 	$0x39, %ah
	iret
_int16_11:
	cmp	$0x11, %ah
	jnz	_int16_12
	iret
_int16_12:
	cmp	$0x12, %ah
	jnz	_int16_XX
	mov $0, %ax
	iret
_int16_XX:
	iret

	.globl _int17_handler
_int17_handler:
	mov $0xd0, %ah
	iret

	.globl _int19_handler
_int19_handler:
	hlt
	iret

	.globl _int1A_handler
_int1A_handler:
	stc
	iret

	.code32
	.globl	xstart
xstart:
	/* reprogram the PICs so that interrupt are masked */
        movb    $0x11,%al	/* ICW1 [ICW4 NEEDED, EDGE TRIGGERED]*/
	outb    %al,$0x20
        movb    $PIC1_VBS, %al
	outb    %al,$0x21
        movb    $0x4,%al
	outb    %al,$0x21
        movb    $0x1,%al
	outb    %al,$0x21
        movb    $0xff,%al
	outb    %al,$0x21
	
        movb    $0x11,%al	/* ICW1 [ICW4 NEEDED, EDGE TRIGGERED]*/
	outb    %al,$0xa0
        movb    $PIC2_VBS, %al
	outb    %al,$0xa1
        movb    $0x2,%al
	outb    %al,$0xa1
        movb    $0x1,%al
	outb    %al,$0xa1
        movb    $0xff,%al
	outb    %al,$0xa1

	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	8(%ebp),%eax
	movl	%eax,_execaddr
	movl	12(%ebp),%ebx
	movl	16(%ebp),%ecx	/* bootp record (32bit pointer) */
	addl	$28,%ecx	/* ip, udp header */
	shll	$12,%ecx
	shrw	$12,%cx
	call	_prot_to_real
	.code16
/* MP: add int10 handler */
	push	%eax
	push	%ebx
	push	%es
	mov	$0,%ax
	mov	%ax,%es
	mov	%cs,%ax
	shl	$16,%eax

	ADDR32 mov	$(_int08_handler-_start),%ax
	mov	$0x20,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int10_handler-_start),%ax
	mov	$0x40,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int11_handler-_start),%ax
	mov	$0x44,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int12_handler-_start),%ax
	mov	$0x48,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int13_handler-_start),%ax
	mov	$0x4c,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int14_handler-_start),%ax
	mov	$0x50,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int15_handler-_start),%ax
	mov	$0x54,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int16_handler-_start),%ax
	mov	$0x58,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int17_handler-_start),%ax
	mov	$0x5c,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int19_handler-_start),%ax
	mov	$0x64,%ebx
	mov	%eax,%es:(%bx)

	ADDR32 mov	$(_int1A_handler-_start),%ax
	mov	$0x68,%ebx
	mov	%eax,%es:(%bx)

	pop	%es
	pop	%ebx
	pop	%eax
/* */
	pushl	%ecx		/* bootp record */
	pushl	%ebx		/* file header */
	movl	$((RELOC<<12)+(1f-RELOC)),%eax
	pushl	%eax
	ADDR32	LJMPI(_execaddr-_start)
1:
	addw	$8,%sp		/* XXX or is this 10 in case of a 16bit "ret" */
	DATA32 call	_real_to_prot
	.code32
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

_execaddr:
	.long	0

#ifdef	IMAGE_MULTIBOOT
/**************************************************************************
XEND - Restart Etherboot from the beginning (from protected mode)
**************************************************************************/

	.globl	xend
xend:
	cs
	lidt	idtarg_realmode-_start+RELOC
	cs
	lgdt	gdtarg-_start+RELOC
#ifdef	GAS291
	ljmp	$REAL_CODE_SEG,$1f-RELOC	/* jump to a 16 bit segment */
#else
	ljmp	$REAL_CODE_SEG,$1f-_start	/* jump to a 16 bit segment */
#endif	/* GAS291 */
1:
	.code16
	movw	$REAL_DATA_SEG,%ax
	movw	%ax,%ds
	movw	%ax,%ss
	movw	%ax,%es

	/* clear the PE bit of CR0 */
	movl	%cr0,%eax
	andl	$0!CR0_PE,%eax
	movl	%eax,%cr0

	/* make intersegment jmp to flush the processor pipeline
	 * and reload %cs:%eip (to clear upper 16 bits of %eip).
	 */
	DATA32 ljmp	$(RELOC)>>4,$2f-_start
2:
	/* we are in real mode now
	 * set up the real mode segment registers : %ds, %ss, %es
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	xorl	%esp,%esp
	ADDR32 movw	initsp-RELOC,%sp

	movw	$0,%ax
	movw	%ax,%fs
	movw	%ax,%gs

	sti
	jmp	_start

	.code32
#endif	/* IMAGE_MULTIBOOT */

.global get_cs
get_cs:
	xorl	%eax,%eax
	movw	%cs,%ax
	ret

.global get_ds
get_ds:
	xorl	%eax,%eax
	movw	%ds,%ax
	ret

.global getsp
getsp:
	movl	%esp,%eax	/* GET STACK POINTER */
	subl	$4, %eax 	/* ACCOUNT FOR RETURN ADDRESS ON */
	ret

.global get_gdtbase
get_gdtbase:
	sub	$8,%esp			/* ALLOCATE ROOM ON THE STACK */
	sgdt	(%esp,1)		/*STORE IGDT REGISTER ON STACK */
	mov	2(%esp),%eax		/* READ GDT BASE ADDRESS */
	mov	$KERN_DATA_SEG,%dx 	/* ASSUME UNIVERSAL DS. */
	add	$8,%esp			/* RESTORE STACK */
	ret				/* DONE */

.global get_gdtsize
get_gdtsize:
	sub 	$8,%esp	/* ALLOCATE ROOM ON THE STACK */
	sgdt	(%esp,1)	/*STORE IGDT REGISTER ON STACK */
	xor	%eax,%eax
	mov	2(%esp),%eax	/* READ GDT BASE ADDRESS */
	mov	(%ESP),%ax
	shr	$3,%ax
	add	$8,%esp	/* RESTORE STACK */
	ret			/* DONE */

.global get_idtbase
get_idtbase:
	sub	$8,%esp
	sidt   (%esp,1) 	/* STORE IIDT REGISTER ON STACK */
	mov	2(%esp),%eax
	mov	$KERN_DATA_SEG,%dx
	add	$8,%esp
	ret

.global get_lw
get_lw:
	xor	%edx,%edx
	mov	8(%esp),%eax
	mov	4(%esp),%dx
	ret
	 
/**************************************************************************
SETJMP - Save stack context for non-local goto
**************************************************************************/
	.globl	setjmp
setjmp:
	mov	4(%esp),%ecx
	mov	0(%esp),%edx
	mov	%edx,0(%ecx)
	mov	%ebx,4(%ecx)
	mov	%esp,8(%ecx)
	mov	%ebp,12(%ecx)
	mov	%esi,16(%ecx)
	mov	%edi,20(%ecx)
	mov	%eax,24(%ecx)
	mov	$0,%eax
	ret

/**************************************************************************
LONGJMP - Non-local jump to a saved stack context
**************************************************************************/
	.globl	longjmp
longjmp:
	mov	4(%esp),%edx
	mov	8(%esp),%eax
	mov	0(%edx),%ecx
	mov	4(%edx),%ebx
	mov	8(%edx),%esp
	mov	12(%edx),%ebp
	mov	16(%edx),%esi
	mov	20(%edx),%edi
	cmp	$0,%eax
	jne	1f
	mov	$1,%eax
1:	mov	%ecx,0(%esp)
	ret

/**************************************************************************
_REAL_TO_PROT - Go from REAL mode to Protected Mode
**************************************************************************/
	.globl	_real_to_prot
_real_to_prot:
	.code16
	cli
	cs
	ADDR32 lgdt	gdtarg-_start
	movl	%cr0,%eax
	orl	$CR0_PE,%eax
	movl	%eax,%cr0		/* turn on protected mode */

	/* flush prefetch queue, and reload %cs:%eip */
	DATA32 ljmp	$KERN_CODE_SEG,$1f
1:
	.code32
	/* reload other segment registers */
	movl	$KERN_DATA_SEG,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	addl	$RELOC,%esp		/* Fix up stack pointer */
	xorl	%eax,%eax
	movl	%eax,%fs
	movl	%eax,%gs
	popl	%eax			/* Fix up return address */
	addl	$RELOC,%eax
	pushl	%eax
	ret

/**************************************************************************
_PROT_TO_REAL - Go from Protected Mode to REAL Mode
**************************************************************************/
	.globl	_prot_to_real
_prot_to_real:
	.code32
	popl	%eax
	subl	$RELOC,%eax		/* Adjust return address */
	pushl	%eax
	subl	$RELOC,%esp		/* Adjust stack pointer */
#ifdef	GAS291
	ljmp	$REAL_CODE_SEG,$1f-RELOC	/* jump to a 16 bit segment */
#else
	ljmp	$REAL_CODE_SEG,$1f-_start	/* jump to a 16 bit segment */
#endif	/* GAS291 */
1:
	.code16
	movw	$REAL_DATA_SEG,%ax
	movw	%ax,%ds
	movw	%ax,%ss
	movw	%ax,%es
	movw	%ax,%fs
	movw	%ax,%gs
	cli

	/* clear the PE bit of CR0 */
	movl	%cr0,%eax
	andl	$0!CR0_PE,%eax
	movl	%eax,%cr0

	/* make intersegment jmp to flush the processor pipeline
	 * and reload %cs:%eip (to clear upper 16 bits of %eip).
	 */
	DATA32 ljmp	$(RELOC)>>4,$2f-_start
2:
	/* we are in real mode now
	 * set up the real mode segment registers : %ds, $ss, %es
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
#if 0
	sti
#endif
	DATA32 ret	/* There is a 32 bit return address on the stack */
	.code32

/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
	.align	4
Idt_Reg:
	.word 0x3ff
	.long 0

	.align	4
_gdt:
gdtarg:
Gdt_Table:
	.word	0x27			/* limit */
	.long	_gdt			/* addr */
	.word	0
_pmcs:
	/* 32 bit protected mode code segment */
	.word	0xffff,0
	.byte	0,0x9f,0xcf,0

_pmds:
	/* 32 bit protected mode data segment */
	.word	0xffff,0
	.byte	0,0x93,0xcf,0

_rmcs:
	/* 16 bit real mode code segment */
	.word	0xffff,(RELOC&0xffff)
	.byte	(RELOC>>16),0x9b,0x00,(RELOC>>24)

_rmds:
	/* 16 bit real mode data segment */
	.word	0xffff,(RELOC&0xffff)
	.byte	(RELOC>>16),0x93,0x00,(RELOC>>24)

	.align	4
RUN_GDT: 			/* POINTER TO GDT IN RAM */
	 .byte	 0x7f,0		/* [BSP_GDT_NUM*8]-1 */
	 .long	 Gdt_Table

	.align	4

	.section ".rodata"
err_not386:
	.ascii	"Etherboot/32 requires 386+"
	.byte	0x0d, 0x0a
err_not386_end:

days:	.long	0
irq_num: .long

        .data
	.align	4
        .org 2048
.global stktop
stktop:
	.long

.section ".armando"
/*                 1:::::::::2:::::::::3:::::::3 */
/*        12345678901234567890123456789012345678 */
/*       v----+----v----+----v----+----v----+--- */

.global EtherbootString
EtherbootString:
.ascii	"EtherBoot MPCC  "	/* fw identifier */

.byte	0, 0		/* mandatory hole */

.long	_start		/* entry point */
.word	0
.byte	'E'		/* type */
.byte	0		/* selector */
.word	0		/* CRC */
