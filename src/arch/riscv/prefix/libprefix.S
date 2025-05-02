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

	.section ".note.GNU-stack", "", @progbits
	.text

/*****************************************************************************
 *
 * Apply relocation records
 *
 *****************************************************************************
 *
 * Apply relocation records from .rel.dyn to fix up iPXE to run at its
 * current address.
 *
 * This function must run before .bss is zeroed (since the relocation
 * records are overlaid with .bss).  It does not require a valid stack
 * pointer.
 *
 * Parameters: none (address is implicit in the program counter)
 *
 * Returns: none
 *
 */

/* Relative relocation type */
#define R_RISCV_RELATIVE 3

	/* Layout of a relocation record */
	.struct 0
rela_offset:	.space ( __riscv_xlen / 8 )
rela_type:	.space ( __riscv_xlen / 8 )
rela_addend:	.space ( __riscv_xlen / 8 )
rela_len:
	.previous

	.section ".prefix.apply_relocs", "ax", @progbits
	.globl	apply_relocs
apply_relocs:

	/* Get relocation records */
	la	t0, _reloc
	la	t1, _ereloc

	/* Determine current location */
	la	t2, reloc_base
	LOADN	t2, (t2)
	sub	t2, t0, t2

1:	/* Read relocation record */
	LOADN	t3, rela_offset(t0)
	LOADN	t4, rela_type(t0)
	LOADN	t5, rela_addend(t0)

	/* Check relocation type */
	addi	t4, t4, -R_RISCV_RELATIVE
	bnez	t4, 2f

	/* Apply relocation */
	add	t3, t3, t2
	add	t5, t5, t2
	STOREN	t5, (t3)
2:
	/* Loop until done */
	addi	t0, t0, rela_len
	blt	t0, t1, 1b

	/* Return to caller */
	ret
	.size	apply_relocs, . - apply_relocs

	/* Link-time address of _reloc */
	.section ".rodata", "a", @progbits
reloc_base:
	.dword	_reloc_base
	.size	reloc_base, . - reloc_base

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
 *   a0 - Selected paging mode (0=no paging)
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

/** Number of page table entries */
#define PTE_COUNT ( PAGE_SIZE / PTE_SIZE )

/* Page table entry flags */
#define PTE_V		0x00000001	/**< Page table entry is valid */
#define PTE_R		0x00000002	/**< Page is readable */
#define PTE_W		0x00000004	/**< Page is writable */
#define PTE_X		0x00000008	/**< Page is executable */
#define PTE_A		0x00000040	/**< Page has been accessed */
#define PTE_D		0x00000080	/**< Page is dirty */

#define PTE_PPN4_LSB	46		/**< PPN[4] LSB (Sv57) */
#define PTE_PPN3_LSB	37		/**< PPN[3] LSB (Sv57 and Sv48) */
#define PTE_PPN2_LSB	28		/**< PPN[2] LSB (all levels) */
#define PTE_PPN1_LSB	19		/**< PPN[1] LSB (all levels) */
#define PTE_PPN0_LSB	10		/**< PPN[0] LSB (all levels) */

/** Page table entry physical page address shift */
#define PTE_PPN_SHIFT ( PAGE_SHIFT - PTE_PPN0_LSB )

#define VPN4_LSB	48		/**< VPN[4] LSB (Sv57) */
#define VPN3_LSB	39		/**< VPN[3] LSB (Sv57 and Sv48) */
#define VPN2_LSB	30		/**< VPN[2] LSB (all levels) */
#define VPN1_LSB	21		/**< VPN[1] LSB (all levels) */
#define VPN0_LSB	12		/**< VPN[0] LSB (all levels) */

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
 *   a0 - Selected paging mode (0=no paging)
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

	.section ".prefix.enable_paging_64", "ax", @progbits
enable_paging_64:
	/* Register usage:
	 *
	 * a0 - return value (enabled paging level)
	 * a1 - currently attempted paging level
	 * a2 - page table base address
	 * a3 - PTE pointer
	 * a4 - PTE stride
	 */
	mv	a2, a0
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

	/* Construct PTE[0-255] for identity map */
	mv	a3, a2
	li	t0, ( PTE_COUNT / 2 )
	li	t1, ( PTE_D | PTE_A | PTE_X | PTE_W | PTE_R | PTE_V )
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
	srli	t0, a2, PTE_PPN_SHIFT
	ori	t0, t0, PTE_V
	STOREN	t0, -PTE_SIZE(a3)

	/* Calculate PTE[x] address for iPXE virtual address map */
	la	t0, prefix_virt
	LOADN	t0, (t0)
	srli	t0, t0, VPN1_LSB
	andi	t0, t0, ( PTE_COUNT - 1 )
	slli	t0, t0, PTE_SIZE_LOG2
	add	a3, a2, t0

	/* Calculate PTE stride for iPXE virtual address map
	 *
	 * PPN[1] LSB is PTE bit 19 in all paging modes, and so the
	 * stride is always ( 1 << 19 )
	 */
	li	a4, 1
	slli	a4, a4, PTE_PPN1_LSB

	/* Construct PTE[x-y] for iPXE virtual address map */
	la	t0, _prefix
	srli	t0, t0, PTE_PPN_SHIFT
	ori	t0, t0, ( PTE_D | PTE_A | PTE_X | PTE_W | PTE_R | PTE_V )
	la	t1, _ebss
	srli	t1, t1, PTE_PPN_SHIFT
1:	STOREN	t0, (a3)
	addi	a3, a3, PTE_SIZE
	add	t0, t0, a4
	ble	t0, t1, 1b

	/* Attempt to enable paging, and read back active paging level */
	slli	t0, a1, SATP_MODE_SHIFT
	srli	t1, a2, PAGE_SHIFT
	or	t0, t0, t1
	csrrw	zero, satp, t0
	sfence.vma
	csrrw	a0, satp, t0
	srli	a0, a0, SATP_MODE_SHIFT

	/* Loop until we successfully enable paging, or run out of levels */
	beq	a0, a1, 1f
	addi	a1, a1, -1
	li	t0, SATP_MODE_SV39
	bge	a1, t0, enable_paging_64_loop
	j	enable_paging_64_done
1:
	/* Adjust return address to a virtual address */
	la	t0, _prefix
	sub	ra, ra, t0
	la	t0, prefix_virt
	LOADN	t0, (t0)
	add	ra, ra, t0

enable_paging_64_done:
	/* Return, with or without paging enabled */
	ret
	.size	enable_paging_64, . - enable_paging_64

	/* Link-time address of _prefix */
	.section ".rodata.prefix_virt", "a", @progbits
prefix_virt:
	.dword	_prefix
	.size	prefix_virt, . - prefix_virt
