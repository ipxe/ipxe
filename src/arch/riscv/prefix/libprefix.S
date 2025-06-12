/*
 * Copyright (C) 2025 Michael Brown <mbrown@fensystems.co.uk>.
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
 * RISC-V prefix library
 *
 */

#include <config/serial.h>

	.section ".note.GNU-stack", "", @progbits
	.text

	/* Link-time base address of _prefix
	 *
	 * This will be not be updated if runtime relocations are applied.
	 */
	.section ".rodata.prefix_link", "a", @progbits
	.balign	( __riscv_xlen / 8 )
prefix_link:
	.dword	_base
	.size	prefix_link, . - prefix_link

	/* Virtual address of _prefix
	 *
	 * This will be updated if runtime relocations are applied.
	 */
	.section ".rodata.prefix_virt", "a", @progbits
	.balign	( __riscv_xlen / 8 )
prefix_virt:
	.dword	_prefix
	.size	prefix_virt, . - prefix_virt

/*****************************************************************************
 *
 * Print character via debug console extension
 *
 *****************************************************************************
 *
 * Print a single character via the SBI DBCN extension.
 *
 * Parameters:
 *
 *   a0 - Character to print
 *
 * Returns:
 *
 *   a0 - Zero if character printed successfully
 *   a1 - Overwritten
 *   a6 - Overwritten
 *   a7 - Overwritten
 *
 */

/* SBI debug console extension */
#define SBI_DBCN ( ( 'D' << 24 ) | ( 'B' << 16 ) | ( 'C' << 8 ) | 'N' )
#define SBI_DBCN_WRITE_BYTE 0x02

	.macro	print_char_dbcn
	li	a7, SBI_DBCN
	li	a6, SBI_DBCN_WRITE_BYTE
	ecall
	.endm

/*****************************************************************************
 *
 * Print character via legacy extension
 *
 *****************************************************************************
 *
 * Print a single character via the SBI putchar legacy extension.
 *
 * Parameters:
 *
 *   a0 - Character to print
 *
 * Returns:
 *
 *   a0 - Overwritten
 *   a7 - Overwritten
 *
 */

/* SBI legacy console putchar */
#define SBI_LEGACY_PUTCHAR 0x01

	.macro	print_char_legacy
	li	a7, SBI_LEGACY_PUTCHAR
	ecall
	.endm

/*****************************************************************************
 *
 * Print character via early UART
 *
 *****************************************************************************
 *
 * Print a single character via a UART.
 *
 * For devices without a functional SBI console, a UART at a hardcoded
 * address can be used as a last resort mechanism for obtaining debug
 * output from the prefix.
 *
 * Parameters:
 *
 *   a0 - Character to print
 *
 * Returns:
 *
 *   a0 - Preserved
 *   a1 - May be overwritten
 *   a6 - May be overwritten
 *   a7 - May be overwritten
 *
 */

/* Default to no UART, if not specified */
#ifndef EARLY_UART_MODEL
#define EARLY_UART_MODEL none
#endif

/* Default to a register shift of zero, if not specified */
#ifndef EARLY_UART_REG_SHIFT
#define EARLY_UART_REG_SHIFT 0
#endif

#define print_char_uart _C2 ( print_char_uart_, EARLY_UART_MODEL )

#define early_uart_reg_base _C2 ( early_uart_reg_base_, __riscv_xlen )

	/* Print character via nonexistent UART */
	.macro	print_char_uart_none
	.endm

	/*
	 * Get UART base address (64-bit addressing)
	 */
	.macro	early_uart_reg_base_64 reg
	csrr	\reg, satp
	beqz	\reg, early_uart_reg_base_64_nonpaged_\@
	LOADN	\reg, early_uart_reg_base_64_virt
	j	early_uart_reg_base_64_done_\@
early_uart_reg_base_64_nonpaged_\@:
	li	\reg, EARLY_UART_REG_BASE
early_uart_reg_base_64_done_\@:
	.endm

	/*
	 * Get UART base address (32-bit addressing)
	 */
	.macro	early_uart_reg_base_32 reg
	li	\reg, EARLY_UART_REG_BASE
	sub	\reg, \reg, tp
	.endm

/*****************************************************************************
 *
 * Print character via 8250-compatible early UART
 *
 *****************************************************************************
 *
 * Print a single character via an 8250- or 16550-compatible UART.
 *
 * Parameters:
 *
 *   a0 - Character to print
 *
 * Returns:
 *
 *   a0 - Preserved
 *   a1 - Overwritten
 *   a7 - Overwritten
 *
 */

/* 8250-compatible UART transmit registers */
#define EARLY_UART_8250_TX		( 0 << EARLY_UART_REG_SHIFT )
#define EARLY_UART_8250_LSR		( 5 << EARLY_UART_REG_SHIFT )
#define EARLY_UART_8250_LSR_THRE	0x20

	.macro	print_char_uart_8250
	early_uart_reg_base a7
	sb	a0, EARLY_UART_8250_TX(a7)
	fence
early_uart_8250_wait_\@:
	lbu	a1, EARLY_UART_8250_LSR(a7)
	andi	a1, a1, EARLY_UART_8250_LSR_THRE
	beqz	a1, early_uart_8250_wait_\@
	.endm

/*****************************************************************************
 *
 * Print character via SiFive-compatible early UART
 *
 *****************************************************************************
 *
 * Print a single character via a SiFive-compatible UART.
 *
 * Parameters:
 *
 *   a0 - Character to print
 *
 * Returns:
 *
 *   a0 - Preserved
 *   a1 - Overwritten
 *   a7 - Overwritten
 *
 */

/* SiFive-compatible UART transmit registers */
#define EARLY_UART_SIFIVE_TXFIFO	( 0 << EARLY_UART_REG_SHIFT )

	.macro	print_char_uart_sifive
	early_uart_reg_base a7
	sw	a0, EARLY_UART_SIFIVE_TXFIFO(a7)
	fence
early_uart_sifive_wait_\@:
	lw	a1, EARLY_UART_SIFIVE_TXFIFO(a7)
	bltz	a1, early_uart_sifive_wait_\@
	.endm

/*****************************************************************************
 *
 * Print single character to early UART (from C code)
 *
 *****************************************************************************
 *
 * This function is called by the SBI console driver to output a
 * character to the early UART (if enabled).
 *
 * The standard C ABI applies to this function.
 *
 * Parameters:
 *
 *   a0 - Character to print
 *
 * Returns: none
 *
 */

	.section ".prefix.early_uart_putchar", "ax", @progbits
	.globl	early_uart_putchar
early_uart_putchar:
	print_char_uart
	ret
	.size	early_uart_putchar, . - early_uart_putchar

/*****************************************************************************
 *
 * Print message to debug console
 *
 *****************************************************************************
 *
 * Print a NUL-terminated string to the debug console.
 *
 * This function prints one character at a time via the "write byte"
 * call (rather than using "write string"), since this avoids any need
 * to know the current virtual-physical address translation.  It does
 * not require a valid stack.
 *
 * Note that the parameter is passed in register t1 (rather than a0)
 * and all non-temporary registers are preserved.
 *
 * Parameters:
 *
 *   t1 - Pointer to string
 *
 * Returns: none
 *
 */

	.section ".prefix.print_message", "ax", @progbits
	.globl	print_message
print_message:
	/* Handle alternate link register */
	mv	t0, ra
print_message_alt:
	/* Register usage:
	 *
	 * a0 - current character
	 * t0 - alternate link register
	 * t1 - character pointer
	 * t2 - preserved a0
	 * t3 - preserved a1
	 * t4 - preserved a6
	 * t5 - preserved a7
	 */
	mv	t2, a0
	mv	t3, a1
	mv	t4, a6
	mv	t5, a7

1:	/* Print each character in turn */
	lbu	a0, (t1)
	addi	t1, t1, 1
	beqz	a0, 2f
	print_char_uart
	print_char_dbcn
	beqz	a0, 1b
	lbu	a0, -1(t1)
	print_char_legacy
	j	1b
2:
	/* Restore registers and return (via alternate link register) */
	mv	a7, t5
	mv	a6, t4
	mv	a1, t3
	mv	a0, t2
	jr	t0
	.size	print_message, . - print_message

	/*
	 * Display progress message (if debugging is enabled)
	 */
	.macro	progress message
#ifndef NDEBUG
	.section ".rodata.progress_\@", "a", @progbits
progress_\@:
	.asciz	"\message"
	.size	progress_\@, . - progress_\@
	.previous
	la	t1, progress_\@
	jal	t0, print_message_alt
#endif
	.endm

/*****************************************************************************
 *
 * Print hexadecimal value to debug console
 *
 *****************************************************************************
 *
 * Print a register value in hexadecimal to the debug console.
 *
 * This function does not require a valid stack.
 *
 * Note that the parameters are passed in registers t1 and t2 (rather
 * than a0) and all non-temporary registers are preserved.
 *
 * Parameters:
 *
 *   t1 - Value to print
 *   t2 - Number of bits to print (must be a multiple of 4)
 *
 * Returns: none
 *
 */

	/*
	 * Convert a single nibble to an ASCII character
	 */
	.macro	nibble_to_ascii reg
	addi	\reg, \reg, -10
	bltz	\reg, dec_\@
	addi	\reg, \reg, ( 'a' - ( '0' + 10 ) )
dec_\@:	addi	\reg, \reg, ( '0' + 10 )
	.endm

	.section ".prefix.print_hex_value", "ax", @progbits
	.globl	print_hex_value
print_hex_value:
	/* Handle alternate link register */
	mv	t0, ra
print_hex_value_alt:
	/* Register usage:
	 *
	 * a0 - current digit / general temporary
	 * t0 - alternate link register
	 * t1 - current value
	 * t2 - digit counter
	 * t3 - preserved a0
	 * t4 - preserved a1
	 * t5 - preserved a6
	 * t6 - preserved a7
	 */
	mv	t3, a0
	mv	t4, a1
	mv	t5, a6
	mv	t6, a7

	/* Skip any unprinted digits */
	li	a0, __riscv_xlen
	sub	a0, a0, t2
	sll	t1, t1, a0

1:	/* Print each digit in turn */
	srli	a0, t1, ( __riscv_xlen - 4 )
	nibble_to_ascii a0
	print_char_uart
	print_char_dbcn
	beqz	a0, 2f
	srli	a0, t1, ( __riscv_xlen - 4 )
	nibble_to_ascii a0
	print_char_legacy
2:	slli	t1, t1, 4
	addi	t2, t2, -4
	bgtz	t2, 1b

	/* Restore registers and return (via alternate link register) */
	mv	a7, t6
	mv	a6, t5
	mv	a1, t4
	mv	a0, t3
	jr	t0
	.size	print_hex_value, . - print_hex_value

	/*
	 * Display hexadecimal register value (if debugging is enabled)
	 */
	.macro	print_hex_reg reg, bits=__riscv_xlen
#ifndef NDEBUG
	mv	t1, \reg
	li	t2, \bits
	jal	t0, print_hex_value_alt
#endif
	.endm

	/*
	 * Display hexadecimal symbol address (if debugging is enabled)
	 */
	.macro	print_hex_addr sym
#ifndef NDEBUG
	la	t1, \sym
	li	t2, __riscv_xlen
	jal	t0, print_hex_value_alt
#endif
	.endm

	/*
	 * Display hexadecimal data value (if debugging is enabled)
	 */
	.macro	print_hex_data sym
#ifndef NDEBUG
	LOADN	t1, \sym
	li	t2, __riscv_xlen
	jal	t0, print_hex_value_alt
#endif
	.endm

/*****************************************************************************
 *
 * Apply compressed relocation records
 *
 *****************************************************************************
 *
 * Apply compressed relocation records to fix up iPXE to run at its
 * current virtual address.
 *
 * This function must run before .bss is zeroed (since the relocation
 * records are overlaid with .bss).  It does not require a valid stack
 * pointer.
 *
 * Parameters: none
 *
 *   a0 - Relocation records
 *
 * Returns: none
 *
 */

/** Number of bits in a skip value */
#define ZREL_SKIP_BITS 19

	.section ".prefix.apply_relocs", "ax", @progbits
	.globl	apply_relocs
apply_relocs:
	/* Register usage:
	 *
	 * a0 - current relocation record pointer
	 * a1 - current relocation target address
	 * a2 - relocation addend
	 * a3 - current relocation record value
	 * a4 - number of bits remaining in current relocation record
	 */
	la	a1, _prefix

	/* Calculate relocation addend */
	LOADN	a2, prefix_virt
	sub	a2, a1, a2

	/* Skip applying relocations if addend is zero */
	beqz	a2, apply_relocs_done
	progress " reloc"

	/* Test writability
	 *
	 * We do this to avoid accidentally sending an undefined
	 * sequence of commands to a flash device, if we are started
	 * from read-only memory with no paging support.
	 *
	 * We attempt to write an all-ones pattern, on the basis that
	 * this pattern will harmlessly cause any flash device
	 * conforming to the CFI01 specification to enter the default
	 * "read array" state.
	 */
	la	t0, apply_relocs_test
	li	t1, -1
	STOREN	t1, (t0)
	LOADN	t2, (t0)
	bne	t1, t2, apply_relocs_failed

apply_relocs_loop:
	/* Read new relocation record */
	LOADN	a3, (a0)
	addi	a0, a0, ( __riscv_xlen / 8 )
	li	a4, ( __riscv_xlen - 1 )

	/* Consume and apply skip, if present (i.e. if MSB=0) */
	bltz	a3, 1f
	addi	a4, a4, -ZREL_SKIP_BITS
	srli	t0, a3, ( __riscv_xlen - ( ZREL_SKIP_BITS + 1 ) )
	slli	t0, t0, ( ( __riscv_xlen / 32 ) + 1 )
	add	a1, a1, t0
1:
	/* Apply relocations corresponding to set bits in record */
1:	andi	t0, a3, 1
	beqz	t0, 2f
	LOADN	t1, (a1)
	add	t1, t1, a2
	STOREN	t1, (a1)
2:	addi	a1, a1, ( __riscv_xlen / 8 )
	srli	a3, a3, 1
	addi	a4, a4, -1
	bnez	a4, 1b

	/* Loop until we have reached a terminator record (MSB=0, offset=0) */
	bnez	a3, apply_relocs_loop

	/* Check that relocations were applied successfully */
	la	t0, _prefix
	LOADN	t1, prefix_virt
	bne	t0, t1, apply_relocs_failed

apply_relocs_done:
	/* Return to caller */
	progress " ok\r\n"
	ret

apply_relocs_failed:
	/* Failure to apply relocations (if relocations were needed)
	 * is a fatal error.
	 */
	progress " failed\r\n"
	j	reset_system
	.size	apply_relocs, . - apply_relocs

	/* Writability test
	 *
	 * Placed within .data rather than .bss, since we need this to
	 * be within the range of the stored iPXE image.
	 */
	.section ".data.apply_relocs_test", "aw", @progbits
	.balign	( __riscv_xlen / 8 )
apply_relocs_test:
	.space	( __riscv_xlen / 8 )
	.size	apply_relocs_test, . - apply_relocs_test

/*****************************************************************************
 *
 * Enable paging
 *
 *****************************************************************************
 *
 * This function must be called with flat physical addressing.  It
 * does not require a valid stack pointer.
 *
 * Parameters:
 *
 *   a0 - Page table to fill in (4kB, must be aligned to a 4kB boundary)
 *
 * Returns:
 *
 *   a0 - Size of accessible physical address space (or zero for no limit)
 *   tp - Virtual address offset
 *   pc - Updated to a virtual address if paging enabled
 *
 */

/** Number of bits in a page offset */
#define PAGE_SHIFT 12

/** Page size */
#define PAGE_SIZE ( 1 << PAGE_SHIFT )

/** Size of a page table entry (log2) */
#define PTE_SIZE_LOG2 ( ( __riscv_xlen / 32 ) + 1 )

/** Size of a page table entry */
#define PTE_SIZE ( 1 << PTE_SIZE_LOG2 )

/** Number of page table entries (log2) */
#define PTE_COUNT_LOG2 ( PAGE_SHIFT - PTE_SIZE_LOG2 )

/** Number of page table entries */
#define PTE_COUNT ( 1 << PTE_COUNT_LOG2 )

/** Number of bits in a virtual or physical page number */
#define VPPN_SHIFT PTE_COUNT_LOG2

/* Page table entry flags */
#define PTE_V		0x00000001	/**< Page table entry is valid */
#define PTE_R		0x00000002	/**< Page is readable */
#define PTE_W		0x00000004	/**< Page is writable */
#define PTE_X		0x00000008	/**< Page is executable */
#define PTE_A		0x00000040	/**< Page has been accessed */
#define PTE_D		0x00000080	/**< Page is dirty */

/* Page table entry flags for our leaf pages */
#define PTE_LEAF ( PTE_D | PTE_A | PTE_X | PTE_W | PTE_R | PTE_V )

/** Physical page number LSB in PTE */
#define PTE_PPN_LSB(x) ( 10 + (x) * VPPN_SHIFT )
#define PTE_PPN4_LSB	PTE_PPN_LSB(4)	/**< PPN[4] LSB (Sv57) */
#define PTE_PPN3_LSB	PTE_PPN_LSB(3)	/**< PPN[3] LSB (Sv57 & Sv48) */
#define PTE_PPN2_LSB	PTE_PPN_LSB(2)	/**< PPN[2] LSB (Sv57, Sv48, & Sv39) */
#define PTE_PPN1_LSB	PTE_PPN_LSB(1)	/**< PPN[1] LSB (all levels) */
#define PTE_PPN0_LSB	PTE_PPN_LSB(0)	/**< PPN[0] LSB (all levels) */

/** Page table entry physical page address shift */
#define PTE_PPN_SHIFT ( PAGE_SHIFT - PTE_PPN0_LSB )

/** Virtual page number LSB */
#define VPN_LSB(x) ( PAGE_SHIFT + (x) * VPPN_SHIFT )
#define VPN4_LSB	VPN_LSB(4)	/**< VPN[4] LSB (Sv57) */
#define VPN3_LSB	VPN_LSB(3)	/**< VPN[3] LSB (Sv57 & Sv48) */
#define VPN2_LSB	VPN_LSB(2)	/**< VPN[2] LSB (Sv57, Sv48, & Sv39) */
#define VPN1_LSB	VPN_LSB(1)	/**< VPN[1] LSB (all levels) */
#define VPN0_LSB	VPN_LSB(0)	/**< VPN[0] LSB (all levels) */

/* Paging modes */
#define SATP_MODE_SV57	10		/**< Five-level paging (Sv57) */
#define SATP_MODE_SV48	9		/**< Four-level paging (Sv48) */
#define SATP_MODE_SV39	8		/**< Three-level paging (Sv39) */
#define SATP_MODE_SV32	1		/**< Two-level paging (Sv32) */

/** Paging mode shift */
#if __riscv_xlen == 64
#define SATP_MODE_SHIFT	60
#else
#define SATP_MODE_SHIFT	31
#endif

	.globl	enable_paging
	.equ	enable_paging, _C2 ( enable_paging_, __riscv_xlen )

	/* Paging mode names (for debug messages) */
	.section ".rodata.paging_mode_names", "a", @progbits
paging_mode_names:
	.asciz	"none"
	.org	( paging_mode_names + 5 * SATP_MODE_SV32 )
	.asciz	"Sv32"
	.org	( paging_mode_names + 5 * SATP_MODE_SV39 )
	.asciz	"Sv39"
	.org	( paging_mode_names + 5 * SATP_MODE_SV48 )
	.asciz	"Sv48"
	.org	( paging_mode_names + 5 * SATP_MODE_SV57 )
	.asciz	"Sv57"
	.size	paging_mode_names, . - paging_mode_names

	/*
	 * Display paging mode name (if debugging is enabled)
	 */
	.macro	paging_mode_name reg
#ifndef NDEBUG
	slli	t0, \reg, 2
	add	t0, t0, \reg
	la	t1, paging_mode_names
	add	t1, t1, t0
	jal	t0, print_message_alt
#endif
	.endm

	/* Maximum physical alignment
	 *
	 * We align to a "megapage" boundary to simplify the task of
	 * setting up page table mappings.
	 */
	.globl	_max_align
	.equ	_max_align, ( 1 << VPN1_LSB )

	/* Space for page table
	 *
	 * This can be used only once .bss is known to be writable.
	 */
	.section ".bss.page_table", "a", @nobits
	.globl	page_table
	.balign	PAGE_SIZE
page_table:
	.space	PAGE_SIZE
	.size	page_table, . - page_table

	/* Convert physical address to virtual address */
	.macro	phys_to_virt rd, rs:vararg
	_C2 ( phys_to_virt_, __riscv_xlen ) \rd, \rs
	.endm

/*****************************************************************************
 *
 * Disable paging
 *
 *****************************************************************************
 *
 * This function may be called with either virtual or flat physical
 * addressing.  It does not require a valid stack pointer.
 *
 * Parameters:
 *
 *   tp - Virtual address offset
 *
 * Returns:
 *
 *   tp - Virtual address offset (zeroed)
 *   pc - Updated to a physical address
 *
 */

	.globl	disable_paging
	.equ	disable_paging, _C2 ( disable_paging_, __riscv_xlen )

/*****************************************************************************
 *
 * Enable 64-bit paging
 *
 *****************************************************************************
 *
 * Construct a 64-bit page table to identity-map the whole of the
 * mappable physical address space, and to map iPXE itself at its
 * link-time address (which must be 2MB-aligned and be within the
 * upper half of the kernel address space).
 *
 * This function must be called with flat physical addressing.  It
 * does not require a valid stack pointer.
 *
 * Parameters:
 *
 *   a0 - Page table to fill in (4kB, must be aligned to a 4kB boundary)
 *
 * Returns:
 *
 *   a0 - Size of accessible physical address space (or zero for no limit)
 *   tp - Virtual address offset
 *   pc - Updated to a virtual address if paging enabled
 *
 * A 4kB 64-bit page table contains 512 8-byte PTEs.  We choose to use
 * these as:
 *
 *    - PTE[0-255] : Identity map for the physical address space.
 *
 *      This conveniently requires exactly 256 PTEs, regardless of the
 *      paging level.  Higher paging levels are able to identity-map a
 *      larger physical address space:
 *
 *      Sv57 : 256 x 256TB "petapages" (55-bit physical address space)
 *      Sv48 : 256 x 512GB "terapages" (46-bit physical address space)
 *      Sv39 : 256 x   1GB "gigapages" (37-bit physical address space)
 *
 *      Note that Sv48 and Sv39 cannot identity-map the whole of the
 *      available physical address space, since the virtual address
 *      space is not large enough (and is halved by the constraint
 *      that virtual addresses with bit 47/38 set must also have all
 *      higher bits set, and so cannot identity-map to a 55-bit
 *      physical address).
 *
 *    - PTE[x-y] : Virtual address map for iPXE
 *
 *      These are 2MB "megapages" used to map the link-time virtual
 *      address range used by iPXE itself.  We can use any 2MB-aligned
 *      range within 0xffffffffe0000000-0xffffffffffc00000, which
 *      breaks down as:
 *
 *         VPN[4] = 511     (in Sv57, must be all-ones in Sv48 and Sv39)
 *         VPN[3] = 511     (in Sv57 and Sv48, must be all-ones in Sv39)
 *         VPN[2] = 511     (in all paging levels)
 *         VPN[1] = 256-510 (in all paging levels)
 *         VPN[0] = 0       (in all paging levels)
 *
 *      In most builds, only a single 2MB "megapage" will be needed.
 *      We choose a link-time starting address of 0xffffffffeb000000
 *      within the permitted range, since the "eb" pattern is fairly
 *      distinctive and so makes it easy to visually identify any
 *      addresses originating from within iPXE's virtual address
 *      space.
 *
 *    - PTE[511] : Recursive next level page table pointer
 *
 *      This is a non-leaf PTE that points back to the page table
 *      itself.  It acts as the next level page table pointer for:
 *
 *         VPN[4] = 511 (in Sv57)
 *         VPN[3] = 511 (in Sv57 and Sv48)
 *         VPN[2] = 511 (in Sv57, Sv48, and Sv39)
 *
 *      This recursive usage creates some duplicate mappings within
 *      unused portions of the virtual address space, but allows us to
 *      use only a single physical 4kB page table.
 */

/** SBI base extension */
#define SBI_BASE 0x10
#define SBI_BASE_MVENDORID 0x04

/** Non-standard T-Head page table entry additional flags
 *
 * T-Head processors such as the C910 use the high bits of the PTE in
 * a very non-standard way that is incompatible with the RISC-V
 * specification.
 *
 * As per the "Memory Attribute Extension (XTheadMae)", bits 62 and 61
 * represent cacheability and "bufferability" (i.e. write-back
 * cacheability) respectively.  If we do not enable these bits, then
 * the processor gets incredibly confused at the point that paging is
 * enabled.  The symptom is that cache lines will occasionally fail to
 * fill, and so reads from any address may return unrelated data from
 * a previously read cache line for a different address.
 */
#define THEAD_PTE_MAEE ( 0x60 << ( __riscv_xlen - 8 ) )

/** T-Head vendor ID */
#define THEAD_MVENDORID 0x5b7

/** T-Head "sxstatus" CSR */
#define THEAD_CSR_SXSTATUS 0x5c0
#define THEAD_CSR_SXSTATUS_MAEE	0x00200000	/**< XTheadMae enabled */

	.section ".prefix.enable_paging_64", "ax", @progbits
enable_paging_64:
	/* Register usage:
	 *
	 * tp - return value (virtual address offset)
	 * a0 - page table base address
	 * a1 - currently attempted paging level
	 * a2 - enabled paging level
	 * a3 - PTE pointer
	 * a4 - PTE stride
	 * a5 - size of accessible physical address space
	 */
	progress " paging:"

	/* Calculate virtual address offset */
	LOADN	t0, prefix_link
	la	t1, _prefix
	sub	tp, t1, t0

	/* Construct base page table entry for address zero */
	li	t0, PTE_LEAF
	STOREN	t0, (a0)

	/* Check for broken T-Head paging extensions */
	mv	a3, a0
	li	a7, SBI_BASE
	li	a6, SBI_BASE_MVENDORID
	ecall
	bnez	a0, 1f
	li	t0, THEAD_MVENDORID
	bne	a1, t0, 1f
	progress "thead-"
	csrr	t0, THEAD_CSR_SXSTATUS
	li	t1, THEAD_CSR_SXSTATUS_MAEE
	and	t0, t0, t1
	beqz	t0, 1f
	progress "mae-"
	LOADN	t0, (a3)
	li	t1, THEAD_PTE_MAEE
	or	t0, t0, t1
	STOREN	t0, (a3)
1:	mv	a0, a3

	/* Find highest supported paging level */
	li	a1, SATP_MODE_SV57
enable_paging_64_loop:

	/* Calculate PTE stride for identity map at this paging level
	 *
	 * a1 == 10 == Sv57: PPN[4] LSB is PTE bit 46  =>  stride := 1 << 46
	 * a1 ==  9 == Sv48: PPN[3] LSB is PTE bit 37  =>  stride := 1 << 37
	 * a1 ==  8 == Sv39: PPN[2] LSB is PTE bit 28  =>  stride := 1 << 28
	 *
	 * and so we calculate stride a4 := ( 1 << ( 9 * a1 - 44 ) )
	 */
	slli	a4, a1, 3
	add	a4, a4, a1
	addi	a4, a4, -44
	li	t0, 1
	sll	a4, t0, a4

	/* Calculate size of accessible physical address space
	 *
	 * The identity map comprises only the lower half of the PTEs,
	 * since virtual addresses for the higher half must have all
	 * high bits set, and so cannot form part of an identity map.
	 */
	slli	a5, a4, ( PTE_PPN_SHIFT + ( PTE_COUNT_LOG2 - 1 ) )

	/* Construct PTE[0-255] for identity map */
	mv	a3, a0
	li	t0, ( PTE_COUNT / 2 )
	LOADN	t1, (a0)
1:	STOREN	t1, (a3)
	addi	a3, a3, PTE_SIZE
	add	t1, t1, a4
	addi	t0, t0, -1
	bgtz	t0, 1b

	/* Zero PTE[256-511] */
	li	t0, ( PTE_COUNT / 2 )
1:	STOREN	zero, (a3)
	addi	a3, a3, PTE_SIZE
	addi	t0, t0, -1
	bgtz	t0, 1b

	/* Construct PTE[511] as next level page table pointer */
	srli	t0, a0, PTE_PPN_SHIFT
	ori	t0, t0, PTE_V
	STOREN	t0, -PTE_SIZE(a3)

	/* Calculate PTE[x] address for iPXE virtual address map */
	LOADN	t0, prefix_link
	srli	t0, t0, VPN1_LSB
	andi	t0, t0, ( PTE_COUNT - 1 )
	slli	t0, t0, PTE_SIZE_LOG2
	add	a3, a0, t0

	/* Calculate PTE stride for iPXE virtual address map
	 *
	 * PPN[1] LSB is PTE bit 19 in all paging modes, and so the
	 * stride is always ( 1 << 19 )
	 */
	li	a4, 1
	slli	a4, a4, PTE_PPN1_LSB

	/* Construct PTE[x-1] for early UART, if applicable */
#ifdef EARLY_UART_REG_BASE
	li	t0, ( EARLY_UART_REG_BASE & ~( ( 1 << VPN1_LSB ) - 1 ) )
	srli	t0, t0, PTE_PPN_SHIFT
	ori	t0, t0, ( PTE_LEAF & ~PTE_X )
	STOREN	t0, -PTE_SIZE(a3)
#endif

	/* Construct PTE[x-y] for iPXE virtual address map */
	la	t0, _prefix
	srli	t0, t0, PTE_PPN_SHIFT
	LOADN	t1, (a0)
	or	t0, t0, t1
	la	t2, _ebss
	srli	t2, t2, PTE_PPN_SHIFT
1:	STOREN	t0, (a3)
	addi	a3, a3, PTE_SIZE
	add	t0, t0, a4
	ble	t0, t2, 1b

	/* Attempt to enable paging, and read back active paging level */
	slli	t0, a1, SATP_MODE_SHIFT
	srli	t1, a0, PAGE_SHIFT
	or	t0, t0, t1
	csrw	satp, t0
	sfence.vma
	csrr	a2, satp
	srli	a2, a2, SATP_MODE_SHIFT

	/* Loop until we successfully enable paging, or run out of levels */
	beq	a2, a1, 1f
	csrw	satp, zero
	addi	a1, a1, -1
	li	t0, SATP_MODE_SV39
	bge	a1, t0, enable_paging_64_loop
	mv	tp, zero
	mv	a5, zero
1:
	/* Adjust return address to a virtual address */
	sub	ra, ra, tp

	/* Return, with or without paging enabled */
	paging_mode_name a2
	mv	a0, a5
	ret
	.size	enable_paging_64, . - enable_paging_64

	/* Convert 64-bit physical address to virtual address */
	.macro	phys_to_virt_64 rd, rs:vararg
	.ifnb	\rs
	mv	\rd, \rs
	.endif
	.endm

	/* Early UART base address when 64-bit paging is enabled
	 *
	 * When an early UART is in use, we choose to use the 2MB
	 * "megapage" immediately below iPXE itself to map the UART.
	 */
#ifdef EARLY_UART_REG_BASE
	.section ".rodata.early_uart_reg_base_64_virt", "a", @progbits
	.balign	8
early_uart_reg_base_64_virt:
	.dword	( _base - ( 1 << VPN1_LSB ) + \
	          ( EARLY_UART_REG_BASE & ( ( 1 << VPN1_LSB ) - 1 ) ) )
	.size	early_uart_reg_base_64_virt, . - early_uart_reg_base_64_virt
#endif

/*****************************************************************************
 *
 * Disable 64-bit paging
 *
 *****************************************************************************
 *
 * This function may be called with either virtual or flat physical
 * addressing.  It does not require a valid stack pointer.
 *
 * Parameters:
 *
 *   tp - Virtual address offset
 *
 * Returns:
 *
 *   tp - Virtual address offset (zeroed)
 *   pc - Updated to a physical address
 *
 */

	.section ".prefix.disable_paging_64", "ax", @progbits
disable_paging_64:
	/* Register usage:
	 *
	 * tp - virtual address offset
	 */

	/* Jump to physical address */
	la	t0, 1f
	bgez	t0, 1f
	add	t0, t0, tp
	jr	t0
1:
	/* Disable paging */
	csrw	satp, zero
	sfence.vma

	/* Update return address to a physical address */
	bgez	ra, 1f
	add	ra, ra, tp
1:
	/* Return with paging disabled and virtual offset zeroed */
	mv	tp, zero
	ret
	.size	disable_paging_64, . - disable_paging_64

/*****************************************************************************
 *
 * Enable 32-bit paging
 *
 *****************************************************************************
 *
 * Construct a 32-bit page table to map the whole of the 32-bit
 * address space with a fixed offset selected to map iPXE itself at
 * its link-time address (which must be 4MB-aligned).
 *
 * This function must be called with flat physical addressing.  It
 * does not require a valid stack pointer.
 *
 * Parameters:
 *
 *   a0 - Page table to fill in (4kB, must be aligned to a 4kB boundary)
 *
 * Returns:
 *
 *   a0 - Size of accessible physical address space (or zero for no limit)
 *   tp - Virtual address offset
 *   pc - Updated to a virtual address if paging enabled
 *
 * A 4kB 32-bit page table contains 1024 4-byte PTEs.  We choose to
 * use these to produce a circular map of the 32-bit address space
 * using 4MB "megapages", with a fixed offset to align the virtual and
 * link-time addresses.
 *
 * To handle the transition from physical to virtual addresses, we
 * temporarily adjust the PTE covering the current program counter to
 * be a direct physical map (so that the program counter remains valid
 * at the moment when paging is enabled), then jump to a virtual
 * address, then restore the temporarily modified PTE.
 */

	.equ	enable_paging_32_xalign, 32

	.section ".prefix.enable_paging_32", "ax", @progbits
enable_paging_32:
	/* Register usage:
	 *
	 * tp - return value (virtual address offset)
	 * a0 - page table base address
	 * a1 - enabled paging level
	 * a2 - PTE pointer
	 * a3 - saved content of temporarily modified PTE
	 */
	progress " paging:"

	/* Calculate virtual address offset */
	LOADN	t0, prefix_link
	la	t1, _prefix
	sub	tp, t1, t0

	/* Construct PTEs for circular map */
	mv	a2, a0
	li	t0, PTE_COUNT
	mv	t1, tp
	ori	t1, t1, ( PTE_LEAF << PTE_PPN_SHIFT )
	li	t2, ( 1 << ( PTE_PPN1_LSB + PTE_PPN_SHIFT ) )
1:	srli	t3, t1, PTE_PPN_SHIFT
	STOREN	t3, (a2)
	addi	a2, a2, PTE_SIZE
	add	t1, t1, t2
	addi	t0, t0, -1
	bgtz	t0, 1b

	/* Temporarily modify PTE for transition code to be an identity map */
	la	t0, enable_paging_32_xstart
	srli	t0, t0, VPN1_LSB
	slli	t1, t0, PTE_SIZE_LOG2
	add	a2, a0, t1
	LOADN	a3, (a2)
	slli	t0, t0, PTE_PPN1_LSB
	ori	t0, t0, PTE_LEAF
	STOREN	t0, (a2)

	/* Adjust PTE pointer to a virtual address */
	sub	a2, a2, tp

	/* Attempt to enable paging, and read back active paging level */
	la	t0, 1f
	sub	t0, t0, tp
	li	t1, ( SATP_MODE_SV32 << SATP_MODE_SHIFT )
	srli	t2, a0, PAGE_SHIFT
	or	t1, t1, t2
	.balign	enable_paging_32_xalign
	/* Start of transition code */
enable_paging_32_xstart:
	csrw	satp, t1
	sfence.vma
	csrr	a1, satp
	beqz	a1, 2f
	jr	t0
1:	/* Restore temporarily modified PTE */
	STOREN	a3, (a2)
	sfence.vma
	/* End of transition code */
	.equ	enable_paging_32_xlen, . - enable_paging_32_xstart
2:	srli	a1, a1, SATP_MODE_SHIFT

	/* Zero SATP and virtual address offset if paging is not enabled */
	bnez	a1, 1f
	csrw	satp, zero
	mv	tp, zero
1:
	/* Adjust return address to a virtual address */
	sub	ra, ra, tp

	/* Return, with or without paging enabled */
	paging_mode_name a1
	mv	a0, zero
	ret
	.size	enable_paging_32, . - enable_paging_32

	/* Ensure that transition code did not cross an alignment boundary */
	.section ".bss.enable_paging_32_xcheck", "aw", @nobits
	.org	. + enable_paging_32_xalign - enable_paging_32_xlen

	/* Convert 32-bit physical address to virtual address */
	.macro	phys_to_virt_32 rd, rs:vararg
	.ifnb	\rs
	sub	\rd, \rs, tp
	.else
	sub	\rd, \rd, tp
	.endif
	.endm

/*****************************************************************************
 *
 * Disable 32-bit paging
 *
 *****************************************************************************
 *
 * This function may be called with either virtual or flat physical
 * addressing.  It does not require a valid stack pointer.
 *
 * Parameters:
 *
 *   tp - Virtual address offset
 *
 * Returns:
 *
 *   tp - Virtual address offset (zeroed)
 *   pc - Updated to a physical address
 *
 */

	.equ	disable_paging_32_xalign, 16

	.section ".prefix.disable_paging_32", "ax", @progbits
disable_paging_32:
	/* Register usage:
	 *
	 * tp - virtual address offset
	 * a0 - page table address
	 * a1 - transition PTE pointer
	 * a2 - transition PTE content
	 */

	/* Get page table address, and exit if paging is already disabled */
	csrr	a0, satp
	beqz	a0, 99f
	slli	a0, a0, PAGE_SHIFT
	sub	a0, a0, tp

	/* Prepare for modifying transition PTE */
	la	t0, disable_paging_32_xstart
	add	t0, t0, tp
	srli	t0, t0, VPN1_LSB
	slli	a1, t0, PTE_SIZE_LOG2
	add	a1, a1, a0
	slli	a2, t0, PTE_PPN1_LSB
	ori	a2, a2, PTE_LEAF

	/* Jump to physical address in transition PTE, and disable paging */
	la	t0, 1f
	add	t0, t0, tp
	.balign	disable_paging_32_xalign
	/* Start of transition code */
disable_paging_32_xstart:
	STOREN	a2, (a1)
	sfence.vma
	jr	t0
1:	csrw	satp, zero
	sfence.vma
	/* End of transition code */
	.equ	disable_paging_32_xlen, . - disable_paging_32_xstart

	/* Update return address to a physical address */
	add	ra, ra, tp

99:	/* Return with paging disabled and virtual offset zeroed */
	mv	tp, zero
	ret
	.size	disable_paging_32, . - disable_paging_32

	/* Ensure that transition code did not cross an alignment boundary */
	.section ".bss.disable_paging_32_xcheck", "aw", @nobits
	.org	. + disable_paging_32_xalign - disable_paging_32_xlen

/*****************************************************************************
 *
 * Install iPXE to a suitable runtime address
 *
 *****************************************************************************
 *
 * Identify a suitable runtime address for iPXE, relocate there, and
 * set up for running normal C code.
 *
 * A valid temporary stack pointer is required.  A 4kB space for a
 * temporary page table may be provided, and must be provided if the
 * iPXE image is running from read-only memory.
 *
 * Note that this function does not preserve the callee-save registers.
 *
 * Parameters:
 *
 *   a0 - Boot hart ID
 *   a1 - Device tree physical address
 *   a2 - Optional temporary page table space (4kB, aligned to a 4kB boundary)
 *   sp - Valid temporary stack pointer
 *
 * Returns:
 *
 *   pc - Updated to be within the relocated iPXE
 *   sp - Top of internal stack
 *   tp - Virtual address offset
 *
 */

	.section ".prefix.install", "ax", @progbits
	.globl	install
install:
	/* Register usage:
	 *
	 * s0 - boot hart ID
	 * s1 - device tree physical address
	 * s2 - saved return address
	 * s3 - relocation records physical address
	 * s4 - maximum accessible physical address
	 * s5 - relocation physical address
	 * s6 - relocation offset
	 * tp - virtual address offset
	 */
	mv	tp, zero
	progress "\r\nSBI->iPXE hart:"
	print_hex_reg a0
	progress " temp:"
	print_hex_reg a2
	progress " fdt:"
	print_hex_reg a1
	progress "\r\nSBI->iPXE phys:"
	print_hex_addr _prefix
	progress " virt:"
	print_hex_data prefix_virt
	mv	s0, a0
	mv	s1, a1
	mv	s2, ra
	la	s3, _edata

	/* Attempt to enable paging, if we have temporary page table space */
	mv	a0, a2
	beqz	a2, 1f
	call	enable_paging
1:	addi	s4, a0, -1

	/* Apply relocations, if still needed after enabling paging */
	mv	a0, s3
	call	apply_relocs

	/* Find a suitable address for relocation (using temporary stack) */
	phys_to_virt a0, s1
	mv	a1, s4
	phys_to_virt sp
	call	fdtmem_relocate
	mv	s5, a0
	progress "SBI->iPXE dest:"
	print_hex_reg a0

	/* Disable paging */
	call	disable_paging

	/* Determine relocation offset */
	la	s6, _prefix
	sub	s6, s5, s6

	/* Copy iPXE image to new location and zero .bss */
	mv	t0, s5
	la	t1, _prefix
	la	t2, _edata
1:	LOADN	t3, (t1)
	STOREN	t3, (t0)
	addi	t0, t0, ( __riscv_xlen / 8 )
	addi	t1, t1, ( __riscv_xlen / 8 )
	blt	t1, t2, 1b
	la	t1, _ebss
	add	t1, t1, s6
2:	STOREN	zero, (t0)
	addi	t0, t0, ( __riscv_xlen / 8 )
	blt	t0, t1, 2b

	/* Jump to relocated copy */
	la	t0, 1f
	add	t0, t0, s6
	jr	t0
1:
	/* Attempt to re-enable paging */
	la	a0, page_table
	call	enable_paging

	/* Reapply relocations, if still needed after enabling paging */
	phys_to_virt a0, s3
	call	apply_relocs

	/* Load stack pointer */
	la	sp, _estack

	/* Store boot hart */
	STOREN	s0, boot_hart, t0

	/* Copy and register system device tree */
	phys_to_virt a0, s1
	mv	a1, s4
	call	fdtmem_register

	/* Return to a virtual address in the relocated copy */
	add	ra, s2, s6
	sub	ra, ra, tp
	progress "\r\n"
	ret
	.size	install, . - install

/*****************************************************************************
 *
 * Reset (or lock up) system
 *
 *****************************************************************************
 *
 * Reset via system via SBI, as a means of exiting from a prefix that
 * has no other defined exit path.  If the reset fails, lock up the
 * system since there is nothing else that can sensibly be done.
 *
 * This function does not require a valid stack pointer.
 *
 * Parameters: none
 *
 * Returns: n/a (does not return)
 *
 */

/* SBI system reset extension */
#define SBI_SRST ( ( 'S' << 24 ) | ( 'R' << 16 ) | ( 'S' << 8 ) | 'T' )
#define SBI_SRST_SYSTEM_RESET 0x00
#define SBI_RESET_COLD 0x00000001

/* SBI legacy shutdown */
#define SBI_LEGACY_SHUTDOWN 0x08

	.section ".prefix.reset_system", "ax", @progbits
	.globl	reset_system
reset_system:
	/* Register usage: irrelevant (does not return) */
	progress "\r\niPXE->SBI reset\r\n"

	/* Attempt reset */
	li	a7, SBI_SRST
	li	a6, SBI_SRST_SYSTEM_RESET
	li	a0, SBI_RESET_COLD
	mv	a1, zero
	ecall
	progress "(reset failed)\r\n"

	/* Attempt legacy shutdown */
	li	a7, SBI_LEGACY_SHUTDOWN
	ecall
	progress "(legacy shutdown failed)\r\n"

	/* If reset failed, lock the system */
1:	wfi
	j	1b
	.size	reset_system, . - reset_system

/*****************************************************************************
 *
 * File split information for the compressor
 *
 *****************************************************************************
 */

/* ELF machine type */
#define EM_RISCV 243

	.section ".zinfo", "a", @progbits
	.org	0
	/* Copy initialised-data portion of image */
	.ascii	"COPY"
	.word	0
	.word	_filesz
	.word	1
	/* Notify compressor of link-time base address */
	.ascii	"BASE"
	.word	0
	.dword	_base
	/* Construct compressed relocation records */
	.ascii	"ZREL"
	.word	_reloc_offset
	.word	_reloc_filesz
	.word	EM_RISCV
