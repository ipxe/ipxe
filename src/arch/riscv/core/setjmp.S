/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

	FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL )

/** @file
 *
 * Long jumps
 *
 */

	.section ".note.GNU-stack", "", @progbits
	.text

	/* Must match jmp_buf structure layout */
	.struct 0
env_ra:		.space ( __riscv_xlen / 8 )
env_sp:		.space ( __riscv_xlen / 8 )
env_s0:		.space ( __riscv_xlen / 8 )
env_s1:		.space ( __riscv_xlen / 8 )
env_s2:		.space ( __riscv_xlen / 8 )
env_s3:		.space ( __riscv_xlen / 8 )
env_s4:		.space ( __riscv_xlen / 8 )
env_s5:		.space ( __riscv_xlen / 8 )
env_s6:		.space ( __riscv_xlen / 8 )
env_s7:		.space ( __riscv_xlen / 8 )
env_s8:		.space ( __riscv_xlen / 8 )
env_s9:		.space ( __riscv_xlen / 8 )
env_s10:	.space ( __riscv_xlen / 8 )
env_s11:	.space ( __riscv_xlen / 8 )
	.previous

/*
 * Save stack context for non-local goto
 */
	.section ".text.setjmp", "ax", @progbits
	.globl	setjmp
setjmp:
	/* Save registers */
	STOREN	ra, env_ra(a0)
	STOREN	sp, env_sp(a0)
	STOREN	s0, env_s0(a0)
	STOREN	s1, env_s1(a0)
	STOREN	s2, env_s2(a0)
	STOREN	s3, env_s3(a0)
	STOREN	s4, env_s4(a0)
	STOREN	s5, env_s5(a0)
	STOREN	s6, env_s6(a0)
	STOREN	s7, env_s7(a0)
	STOREN	s8, env_s8(a0)
	STOREN	s9, env_s9(a0)
	STOREN	s10, env_s10(a0)
	STOREN	s11, env_s11(a0)
	/* Return zero when returning as setjmp() */
	mv	a0, zero
	ret
	.size	setjmp, . - setjmp

/*
 * Non-local jump to a saved stack context
 */
	.section ".text.longjmp", "ax", @progbits
	.globl	longjmp
longjmp:
	/* Restore registers */
	LOADN	s11, env_s11(a0)
	LOADN	s10, env_s10(a0)
	LOADN	s9, env_s9(a0)
	LOADN	s8, env_s8(a0)
	LOADN	s7, env_s7(a0)
	LOADN	s6, env_s6(a0)
	LOADN	s5, env_s5(a0)
	LOADN	s4, env_s4(a0)
	LOADN	s3, env_s3(a0)
	LOADN	s2, env_s2(a0)
	LOADN	s1, env_s1(a0)
	LOADN	s0, env_s0(a0)
	LOADN	sp, env_sp(a0)
	LOADN	ra, env_ra(a0)
	/* Force result to non-zero */
	seqz	a0, a1
	or	a0, a0, a1
	/* Return to setjmp() caller */
	ret
	.size	longjmp, . - longjmp
