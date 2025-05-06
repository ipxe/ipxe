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

	/* Virtual address of _prefix
	 *
	 * This will be updated if runtime relocations are applied.
	 */
	.section ".rodata.prefix_virt", "a", @progbits
prefix_virt:
	.dword	_prefix
	.size	prefix_virt, . - prefix_virt

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
 * Parameters: none (address is implicit in the program counter)
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
	 * a0 - relocation addend
	 * a1 - current relocation target address
	 * a2 - current relocation record pointer
	 * a3 - current relocation record value
	 * a4 - number of bits remaining in current relocation record
	 */
	la	a1, _prefix
	la	a2, _edata

	/* Calculate relocation addend */
	la	t0, prefix_virt
	LOADN	a0, (t0)
	sub	a0, a1, a0

	/* Skip applying relocations if addend is zero */
	beqz	a0, apply_relocs_done

apply_relocs_loop:
	/* Read new relocation record */
	LOADN	a3, (a2)
	addi	a2, a2, ( __riscv_xlen / 8 )
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
	add	t1, t1, a0
	STOREN	t1, (a1)
2:	addi	a1, a1, ( __riscv_xlen / 8 )
	srli	a3, a3, 1
	addi	a4, a4, -1
	bnez	a4, 1b

	/* Loop until we have reached a terminator record (MSB=0, offset=0) */
	bnez	a3, apply_relocs_loop

apply_relocs_done:
	/* Return to caller */
	ret
	.size	apply_relocs, . - apply_relocs

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

/** Number of bits in a virtual or physical page number */
#define VPPN_SHIFT ( PAGE_SHIFT - PTE_SIZE_LOG2 )

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
	li	t1, PTE_LEAF
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
	ori	t0, t0, PTE_LEAF
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
 *   a0 - Selected paging mode (0=no paging)
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

	.equ	enable_paging_32_xalign, 16

	.section ".prefix.enable_paging_32", "ax", @progbits
enable_paging_32:
	/* Register usage:
	 *
	 * a0 - return value (enabled paging level)
	 * a1 - virtual address offset
	 * a2 - page table base address
	 * a3 - PTE pointer
	 * a4 - saved content of temporarily modified PTE
	 */
	mv	a2, a0

	/* Calculate virtual address offset */
	la	t0, prefix_virt
	LOADN	t0, (t0)
	la	t1, _prefix
	sub	a1, t1, t0

	/* Construct PTEs for circular map */
	mv	a3, a2
	li	t0, PTE_COUNT
	mv	t1, a1
	ori	t1, t1, ( PTE_LEAF << PTE_PPN_SHIFT )
	li	t2, ( 1 << ( PTE_PPN1_LSB + PTE_PPN_SHIFT ) )
1:	srli	t3, t1, PTE_PPN_SHIFT
	STOREN	t3, (a3)
	addi	a3, a3, PTE_SIZE
	add	t1, t1, t2
	addi	t0, t0, -1
	bgtz	t0, 1b

	/* Temporarily modify PTE for transition code to be an identity map */
	la	t0, enable_paging_32_xstart
	srli	t0, t0, VPN1_LSB
	slli	t1, t0, PTE_SIZE_LOG2
	add	a3, a2, t1
	LOADN	a4, (a3)
	slli	t0, t0, PTE_PPN1_LSB
	ori	t0, t0, PTE_LEAF
	STOREN	t0, (a3)

	/* Attempt to enable paging, and read back active paging level */
	la	t0, 1f
	sub	t0, t0, a1
	li	t1, ( SATP_MODE_SV32 << SATP_MODE_SHIFT )
	srli	t2, a2, PAGE_SHIFT
	or	t1, t1, t2
	.balign	enable_paging_32_xalign
	/* Start of transition code */
enable_paging_32_xstart:
	csrrw	zero, satp, t1
	sfence.vma
	csrrw	a0, satp, t1
	beqz	a0, enable_paging_32_done
	jr	t0
1:	/* End of transition code */
	.equ	enable_paging_32_xlen, . - enable_paging_32_xstart
	li	a0, 1

	/* Adjust PTE pointer to a virtual address */
	sub	a3, a3, a1

	/* Restore temporarily modified PTE */
	STOREN	a4, (a3)
	sfence.vma

	/* Adjust return address */
	sub	ra, ra, a1

enable_paging_32_done:
	/* Return, with or without paging enabled */
	ret
	.size	enable_paging_32, . - enable_paging_32

	/* Ensure that transition code did not cross an alignment boundary */
	.section ".bss.enable_paging_32_xcheck", "aw", @nobits
	.org	. + enable_paging_32_xalign - enable_paging_32_xlen
