#include <linux/unistd.h>

	.section ".text"
	.code64
	.globl _start
	.type _start, @function

_start:
	xorq	%rbp, %rbp

	popq	%rdi       // argc -> C arg1
	movq	%rsp, %rsi // argv -> C arg2

	andq	$~15, %rsp // 16-byte align the stack

	call	save_args

	/* Our main doesn't use any arguments */
	call	main

	movq	%rax, %rdi // rc -> syscall arg1
	movq	$__NR_exit, %rax
	syscall

	.size _start, . - _start
