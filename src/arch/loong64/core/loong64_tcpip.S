FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

	.section ".note.GNU-stack", "", @progbits

	.text
	.section ".text.tcpip_continue_chksum", "ax", @progbits
	.globl tcpip_continue_chksum
	.type tcpip_continue_chksum, @function

/**
 * Calculate continued TCP/IP checksum
 *
 * ABI Call Mappings (Strict):
 *   $a0 = partial (Current 16-bit partial checksum)
 *   $a1 = data    (Data buffer pointer)
 *   $a2 = len     (Length of data buffer)
 *   Ret: $a0 = Updated 16-bit checksum
 *
 * Register Allocation Table:
 *   $t4 = Loop guard bound for 16-byte unrolled trunk (end - 16)
 *   $a3 = Loop guard bound for 8-byte standard trunk (end - 8)
 *   $a4 = Upper carry bank accumulator register
 *   $a5 = Temporary data load / unsigned branch condition evaluation register
 *   $t7 = Temporary secondary data load register for 16-byte unrolling
 *   $t8 = Constant mask register cached with 0xffff
 */
tcpip_continue_chksum:

	/* Step 1: Pre-invert the initial checksum accumulator */
	nor	$a0, $a0, $zero

	/* Register Initialization: Pre-cache mask to erase final negation latencies */
	li.d	$t8, 0xffff

	/* Step 2: Compute the absolute ending boundary pointer */
	add.d	$a2, $a2, $a1

	/* Optimization: Pre-calculate end markers for 16-byte and 8-byte segments */
	addi.d	$t4, $a2, -16
	addi.d	$a3, $a2, -8

	/* Step 3: Initialize the upper overflow carry bank register */
	move	$a4, $zero

	/* Guard: Skip direct word alignments if data chunk is below 8 bytes */
	bltu	$a3, $a1, post_aligned
	b	2f

	/* Step 4: Pre-alignment Loop - Stride via 16-bit words until 8-byte aligned */
1:	ld.hu	$a5, $a1, 0
	addi.d	$a1, $a1, 2
	add.d	$a4, $a4, $a5

2:	bstrpick.d $a5, $a1, 2, 1
	bnez	$a5, 1b
	b	main_loop_check

	/* Step 5: 16-byte Super-scalar Loop Unrolling (Doubleword parallel path) */
unroll_16byte_loop:
	ld.d	$a5, $a1, 0
	ld.d	$t7, $a1, 8
	addi.d	$a1, $a1, 16

	/* Two-way parallel aggregation using tree-reduction */
	add.d	$a5, $a5, $t7
	sltu	$t7, $a5, $t7
	add.d	$a4, $a4, $t7

	/* Feed results and secondary carries back into primary registers */
	add.d	$a0, $a0, $a5
	sltu	$a5, $a0, $a5
	add.d	$a4, $a4, $a5

main_loop_check:
	bgeu	$t4, $a1, unroll_16byte_loop
	b	2f

	/* Step 6: 8-byte Standard Main Trunk Loop (Residual doublewords) */
1:	ld.d	$a5, $a1, 0
	addi.d	$a1, $a1, 8
	add.d	$a0, $a0, $a5
	sltu	$a5, $a0, $a5
	add.d	$a4, $a4, $a5

2:	bgeu	$a3, $a1, 1b

post_aligned:
	/* Step 7: Post-alignment Loop - Consume residual trailing 16-bit chunks */
	addi.d	$a3, $a2, -2
	b	2f

1:	ld.hu	$a5, $a1, 0
	addi.d	$a1, $a1, 2
	add.d	$a4, $a4, $a5

2:	bgeu	$a3, $a1, 1b

	/* Step 8: Tail Fragmentation - Consume standalone odd single byte if present */
	beq	$a1, $a2, 1f
	ld.bu	$a5, $a1, 0
	add.d	$a4, $a4, $a5

1:
	/* Step 9: Carry Bank Consolidation - Collapse accumulated overflow back into a0 */
	add.d	$a0, $a0, $a4
	sltu	$a4, $a0, $a4

	/* Step 10: 32-bit Cross Fold - Split upper/lower halves into separate slots */
	slli.d	$a5, $a0, 32
	srli.d	$a0, $a0, 32
	srli.d	$a5, $a5, 32

	/* Concurrently fuse upper bit extensions alongside remaining carry markers */
	add.d	$a0, $a0, $a4
	add.d	$a0, $a0, $a5

	/* Step 11: End-around Carry Fold - Lock out implicit sign-extension via bstrpick.d */
fold_carry_loop:
	bstrpick.d $a5, $a0, 63, 16
	beqz	$a5, final_negate
	bstrpick.d $a0, $a0, 15, 0
	add.d   $a0, $a0, $a5
	b       fold_carry_loop

final_negate:
	/* Step 12: Complement Phase - Conclude 16-bit negation against pre-cached mask */
	xor	$a0, $a0, $t8

	/* Step 13: ABI Isolation - Clear upper register lines before execution exit */
	bstrpick.d $a0, $a0, 15, 0
	jirl	$zero, $ra, 0

	.size tcpip_continue_chksum, . - tcpip_continue_chksum
