FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

	.section ".note.GNU-stack", "", %progbits
	.text
/*
   int setjmp(jmp_buf env);
*/
	.globl	setjmp
	.type	setjmp, %function
setjmp:
	/* Store registers */
	st.d	$s0, $a0, 0x0
	st.d	$s1, $a0, 0x8
	st.d	$s2, $a0, 0x10
	st.d	$s3, $a0, 0x18
	st.d	$s4, $a0, 0x20
	st.d	$s5, $a0, 0x28
	st.d	$s6, $a0, 0x30
	st.d	$s7, $a0, 0x38
	st.d	$s8, $a0, 0x40
	st.d	$fp, $a0, 0x48
	st.d	$sp, $a0, 0x50
	st.d	$ra, $a0, 0x58

	move	$a0, $zero
	jirl	$zero, $ra, 0
	.size	setjmp, . - setjmp

/*
   void longjmp(jmp_buf env, int val);
*/
	.globl	longjmp
	.type	longjmp, %function
longjmp:
	/* Restore registers */
	ld.d	$s0, $a0, 0x0
	ld.d	$s1, $a0, 0x8
	ld.d	$s2, $a0, 0x10
	ld.d	$s3, $a0, 0x18
	ld.d	$s4, $a0, 0x20
	ld.d	$s5, $a0, 0x28
	ld.d	$s6, $a0, 0x30
	ld.d	$s7, $a0, 0x38
	ld.d	$s8, $a0, 0x40
	ld.d	$fp, $a0, 0x48
	ld.d	$sp, $a0, 0x50
	ld.d	$ra, $a0, 0x58
	addi.d	$a0, $zero, 1    # a0 = 1
	beqz	$a1, .exit       # if (a1 == 0); goto L0
	move	$a0, $a1         # a0 = a1
.exit:
	jirl	$zero, $ra, 0
	.size	longjmp, . - longjmp
