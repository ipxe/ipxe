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

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * BIOS multiprocessor API implementation
 *
 */

#include <registers.h>
#include <librm.h>
#include <ipxe/uaccess.h>
#include <ipxe/timer.h>
#include <ipxe/msr.h>
#include <ipxe/mp.h>

/** Local APIC base address MSR */
#define MSR_APIC_BASE 0x0000001b

/** Local APIC is in x2APIC mode */
#define MSR_APIC_BASE_X2APIC 0x400

/** Local APIC base address mask */
#define MSR_APIC_BASE_MASK ( ~0xfffULL )

/** Interrupt command register */
#define APIC_ICR 0x0300

/** Interrupt command register (x2APIC) */
#define MSR_X2APIC_ICR 0x830

/** Interrupt command register: send to all excluding self */
#define APIC_ICR_ALL_NOT_SELF 0x000c0000

/** Interrupt command register: level mode */
#define APIC_ICR_LEVEL 0x00008000

/** Interrupt command register: level asserted */
#define APIC_ICR_LEVEL_ASSERT 0x00004000

/** Interrupt command register: INIT */
#define APIC_ICR_INIT 0x00000500

/** Interrupt command register: SIPI */
#define APIC_ICR_SIPI( vector ) ( 0x00000600 | (vector) )

/** Time to wait for an IPI to complete */
#define IPI_WAIT_MS 10

/**
 * Startup IPI vector
 *
 * The real-mode startup IPI code must be copied to a page boundary in
 * base memory.  We fairly arbitrarily choose to place this at 0x8000.
 */
#define SIPI_VECTOR 0x08

/** Protected-mode startup IPI handler */
extern void __asmcall mp_jump ( mp_addr_t func, mp_addr_t opaque );

/**
 * Execute a multiprocessor function on the boot processor
 *
 * @v func		Multiprocessor function
 * @v opaque		Opaque data pointer
 */
static void bios_mp_exec_boot ( mp_func_t func, void *opaque ) {

	/* Call multiprocessor function with physical addressing */
	__asm__ __volatile__ ( PHYS_CODE ( "pushl %k2\n\t"
					   "pushl %k1\n\t"
					   "call *%k0\n\t"
					   "addl $8, %%esp\n\t" )
			       : : "R" ( mp_address ( mp_call ) ),
				   "R" ( mp_address ( func ) ),
				   "R" ( mp_address ( opaque ) ) );
}

/**
 * Send an interprocessor interrupt
 *
 * @v apic		APIC base address
 * @v x2apic		x2APIC mode enabled
 * @v icr		Interrupt control register value
 */
static void bios_mp_ipi ( void *apic, int x2apic, uint32_t icr ) {

	/* Write ICR according to APIC/x2APIC mode */
	DBGC ( MSR_APIC_BASE, "BIOSMP sending IPI %#08x\n", icr );
	if ( x2apic ) {
		wrmsr ( MSR_X2APIC_ICR, icr );
	} else {
		writel ( icr, ( apic + APIC_ICR ) );
	}

	/* Allow plenty of time for delivery to complete */
	mdelay ( IPI_WAIT_MS );
}

/**
 * Start a multiprocessor function on all application processors
 *
 * @v func		Multiprocessor function
 * @v opaque		Opaque data pointer
 */
static void bios_mp_start_all ( mp_func_t func, void *opaque ) {
	struct i386_regs regs;
	uint64_t base;
	uint32_t ipi;
	void *apic;
	int x2apic;

	/* Prepare SIPI handler */
	regs.eax = mp_address ( func );
	regs.edx = mp_address ( opaque );
	setup_sipi ( SIPI_VECTOR, virt_to_phys ( mp_jump ), &regs );

	/* Get local APIC base address and mode */
	base = rdmsr ( MSR_APIC_BASE );
	x2apic = ( base & MSR_APIC_BASE_X2APIC );
	DBGC ( MSR_APIC_BASE, "BIOSMP local %sAPIC base %#llx\n",
	       ( x2apic ? "x2" : "" ), ( ( unsigned long long ) base ) );

	/* Map local APIC */
	apic = ioremap ( ( base & MSR_APIC_BASE_MASK ), PAGE_SIZE );
	if ( ! apic )
		goto err_ioremap;

	/* Assert INIT IPI */
	ipi = ( APIC_ICR_ALL_NOT_SELF | APIC_ICR_LEVEL |
		APIC_ICR_LEVEL_ASSERT | APIC_ICR_INIT );
	bios_mp_ipi ( apic, x2apic, ipi );

	/* Clear INIT IPI */
	ipi &= ~APIC_ICR_LEVEL_ASSERT;
	bios_mp_ipi ( apic, x2apic, ipi );

	/* Send SIPI */
	ipi = ( APIC_ICR_ALL_NOT_SELF | APIC_ICR_SIPI ( SIPI_VECTOR ) );
	bios_mp_ipi ( apic, x2apic, ipi );

	iounmap ( apic );
 err_ioremap:
	/* No way to handle errors: caller must check that
	 * multiprocessor function executed as expected.
	 */
	return;
}

PROVIDE_MPAPI_INLINE ( pcbios, mp_address );
PROVIDE_MPAPI ( pcbios, mp_exec_boot, bios_mp_exec_boot );
PROVIDE_MPAPI ( pcbios, mp_start_all, bios_mp_start_all );
