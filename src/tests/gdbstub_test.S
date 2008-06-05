	.arch i386
	.section ".text"
	.code32
gdbstub_test:
	/* 1. Read registers test */
	movl    $0xea010203, %eax
	movl    $0xeb040506, %ebx
	movl    $0xec070809, %ecx
	movl    $0xed0a0b0c, %edx
	movl    $0x510d0e0f, %esi
	movl    $0xd1102030, %edi
	int	$3

	/* 2. Write registers test */
	int	$3

	/* 3. Read memory test */
	subl	$8, %esp
	movl	$0x11223344, 4(%esp)
	movw	$0x5566, 2(%esp)
	movb	$0x77, (%esp)
	int	$3

	/* 4. Write memory test */
	int	$3
	addl	$8, %esp

	/* 5. Step test */
	int	$3
	nop

1:
	jmp	1b
