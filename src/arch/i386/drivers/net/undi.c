/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
UNDI NIC driver for Etherboot

This file Copyright (C) 2003 Michael Brown <mbrown@fensystems.co.uk>
of Fen Systems Ltd. (http://www.fensystems.co.uk/).  All rights
reserved.

$Id$
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 */

#ifdef PCBIOS

/* to get some global routines like printf */
#include "etherboot.h"
/* to get the interface to the body of the program */
#include "nic.h"
/* to get the PCI support functions, if this is a PCI NIC */
#include "pci.h"
/* UNDI and PXE defines.  Includes pxe.h. */
#include "undi.h"
/* 8259 PIC defines */
#include "pic8259.h"
/* Real-mode calls */
#include "realmode.h"
/* E820 map mangler */
#include "hidemem.h"

/* NIC specific static variables go here */
static undi_t undi = { 
	.pnp_bios         = NULL, 
	.rom              = NULL, 
	.undi_rom_id      = NULL, 
	.pxe              = NULL, 
	.pxs              = NULL, 
	.xmit_data        = NULL,
	.base_mem_data    = NULL, 
	.driver_code      = NULL, 
	.driver_code_size = 0, 
	.driver_data      = NULL, 
	.driver_data_size = 0, 
	.xmit_buffer      = NULL,
	.prestarted       = 0, 
	.started          = 0, 
	.initialized      = 0, 
	.opened           = 0,
	.pci	          = { 0, 0, 0, NULL, 0, 0, 0, 0, 0, 0, 0, NULL },
	.irq              = IRQ_NONE 
};

/* Function prototypes */
static int allocate_base_mem_data ( void );
static int free_base_mem_data ( void );
static int eb_pxenv_undi_shutdown ( void );
static int eb_pxenv_stop_undi ( void );
static int undi_unload_base_code ( void );
static int undi_full_shutdown ( void );

/* Trivial/nontrivial IRQ handler selection */
#ifdef UNDI_NONTRIVIAL_IRQ
static void nontrivial_irq_handler ( void );
static void nontrivial_irq_handler_end ( void );
static int install_nontrivial_irq_handler ( irq_t irq );
static int remove_nontrivial_irq_handler ( irq_t irq );
static int nontrivial_irq_triggered ( irq_t irq );
static int copy_nontrivial_irq_handler ( void *target, size_t target_size );
#define NONTRIVIAL_IRQ_HANDLER_SIZE FRAGMENT_SIZE(nontrivial_irq_handler)
#define install_undi_irq_handler(irq) install_nontrivial_irq_handler(irq)
#define remove_undi_irq_handler(irq) remove_nontrivial_irq_handler(irq)
#define undi_irq_triggered(irq) nontrivial_irq_triggered(irq)
#define UNDI_IRQ_HANDLER_SIZE NONTRIVIAL_IRQ_HANDLER_SIZE
#define copy_undi_irq_handler(dest,size) copy_nontrivial_irq_handler(dest,size)
#else
#define install_undi_irq_handler(irq) install_trivial_irq_handler(irq)
#define remove_undi_irq_handler(irq) remove_trivial_irq_handler(irq)
#define undi_irq_triggered(irq) trivial_irq_triggered(irq)
#define UNDI_IRQ_HANDLER_SIZE TRIVIAL_IRQ_HANDLER_SIZE
#define copy_undi_irq_handler(dest,size) copy_trivial_irq_handler(dest,size)
#endif /* UNDI_NONTRIVIAL_IRQ */

/* Size of variable-length data in base_mem_data */
#define BASE_MEM_VARDATA_SIZE ( UNDI_IRQ_HANDLER_SIZE > e820mangler_size ? \
				UNDI_IRQ_HANDLER_SIZE : e820mangler_size )

/**************************************************************************
 * Utility functions
 **************************************************************************/

/* Checksum a block.
 */

static uint8_t checksum ( void *block, size_t size ) {
	uint8_t sum = 0;
	uint16_t i = 0;
	for ( i = 0; i < size; i++ ) {
		sum += ( ( uint8_t * ) block )[i];
	}
	return sum;
}

/* Print the status of a !PXE structure
 */

static void pxe_dump ( void ) {
	printf ( "API %hx:%hx St %hx:%hx UD %hx:%hx UC %hx:%hx "
		 "BD %hx:%hx BC %hx:%hx\n",
		 undi.pxe->EntryPointSP.segment, undi.pxe->EntryPointSP.offset,
		 undi.pxe->Stack.Seg_Addr, undi.pxe->Stack.Seg_Size,
		 undi.pxe->UNDIData.Seg_Addr, undi.pxe->UNDIData.Seg_Size,
		 undi.pxe->UNDICode.Seg_Addr, undi.pxe->UNDICode.Seg_Size,
		 undi.pxe->BC_Data.Seg_Addr, undi.pxe->BC_Data.Seg_Size,
		 undi.pxe->BC_Code.Seg_Addr, undi.pxe->BC_Code.Seg_Size );
}

/* Allocate/free space for structures that must reside in base memory
 */

static int allocate_base_mem_data ( void ) {
	/* Allocate space in base memory.
	 * Initialise pointers to base memory structures.
	 */
	if ( undi.base_mem_data == NULL ) {
		undi.base_mem_data =
			allot_base_memory ( sizeof(undi_base_mem_data_t) +
					    BASE_MEM_VARDATA_SIZE );
		if ( undi.base_mem_data == NULL ) {
			printf ( "Failed to allocate base memory\n" );
			free_base_mem_data();
			return 0;
		}
		memset ( undi.base_mem_data, 0, sizeof(undi_base_mem_data_t) );
		undi.pxs = &undi.base_mem_data->pxs;
		undi.xmit_data = &undi.base_mem_data->xmit_data;
		undi.xmit_buffer = undi.base_mem_data->xmit_buffer;
	}
	return 1;
}

static int free_base_mem_data ( void ) {
	if ( undi.base_mem_data != NULL ) {
		forget_base_memory ( undi.base_mem_data,
				     sizeof(undi_base_mem_data_t) +
				     BASE_MEM_VARDATA_SIZE );
		undi.base_mem_data = NULL;
		undi.pxs = NULL;
		undi.xmit_data = NULL;
		undi.xmit_buffer = NULL;
		copy_undi_irq_handler ( NULL, 0 );
	}
	return 1;
}

static void assemble_firing_squad ( firing_squad_lineup_t *lineup,
			     void *start, size_t size,
			     firing_squad_shoot_t shoot ) {
	int target;
	int index;
	int bit;
	int start_kb = virt_to_phys(start) >> 10;
	int end_kb = ( virt_to_phys(start+size) + (1<<10) - 1 ) >> 10;
	
	for ( target = start_kb; target <= end_kb; target++ ) {
		index = FIRING_SQUAD_TARGET_INDEX ( target );
		bit = FIRING_SQUAD_TARGET_BIT ( target );
		lineup->targets[index] = ( shoot << bit ) |
			( lineup->targets[index] & ~( 1 << bit ) );
	}
}

static void shoot_targets ( firing_squad_lineup_t *lineup ) {
	int shoot_this_target = 0;
	int shoot_last_target = 0;
	int start_target = 0;
	int target;

	for ( target = 0; target <= 640; target++ ) {
		shoot_this_target = ( target == 640 ? 0 : 
		      ( 1 << FIRING_SQUAD_TARGET_BIT(target) ) &
		      lineup->targets[FIRING_SQUAD_TARGET_INDEX(target)] );
		if ( shoot_this_target && !shoot_last_target ) {
			start_target = target;
		} else if ( shoot_last_target && !shoot_this_target ) {
			size_t range_size = ( target - start_target ) << 10;
			forget_base_memory ( phys_to_virt( start_target<<10 ),
					     range_size );
		}
		shoot_last_target = shoot_this_target;
	}
}

/* Debug macros
 */

#ifdef TRACE_UNDI
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif

#define UNDI_STATUS(pxs) ( (pxs)->Status == PXENV_EXIT_SUCCESS ? \
			      "SUCCESS" : \
			      ( (pxs)->Status == PXENV_EXIT_FAILURE ? \
				"FAILURE" : "UNKNOWN" ) )

/**************************************************************************
 * Base memory scanning functions
 **************************************************************************/

/* Locate the $PnP structure indicating a PnP BIOS.
 */

static int hunt_pnp_bios ( void ) {
	uint32_t off = 0x10000;

	printf ( "Hunting for PnP BIOS..." );
	while ( off > 0 ) {
		off -= 16;
		undi.pnp_bios = (pnp_bios_t *) phys_to_virt ( 0xf0000 + off );
		if ( undi.pnp_bios->signature == PNP_BIOS_SIGNATURE ) {
			printf ( "found $PnP at f000:%hx...", off );
			if ( checksum(undi.pnp_bios,sizeof(pnp_bios_t)) !=0) {
				printf ( "invalid checksum\n..." );
				continue;
			}
			printf ( "ok\n" );
			return 1;
		}
	}
	printf ( "none found\n" );
	undi.pnp_bios = NULL;
	return 0;
}

/* Locate the !PXE structure indicating a loaded UNDI driver.
 */

static int hunt_pixie ( void ) {
	static uint32_t ptr = 0;
	pxe_t *pxe = NULL;

	printf ( "Hunting for pixies..." );
	if ( ptr == 0 ) ptr = 0xa0000;
	while ( ptr > 0x10000 ) {
		ptr -= 16;
		pxe = (pxe_t *) phys_to_virt ( ptr );
		if ( memcmp ( pxe->Signature, "!PXE", 4 ) == 0 ) {
			printf ( "found !PXE at %x...", ptr );
			if ( checksum ( pxe, sizeof(pxe_t) ) != 0 ) {
				printf ( "invalid checksum\n..." );
				continue;
			}
			if ( ptr < get_free_base_memory() ) {
				printf ( "in free base memory!\n\n"
					 "WARNING: a valid !PXE structure was "
					 "found in an area of memory marked "
					 "as free!\n\n" );
				undi.pxe = pxe;
				pxe_dump();
				undi.pxe = NULL;
				printf ( "\nIgnoring and continuing, but this "
					 "may cause problems later!\n\n" );
				continue;
			}
			printf ( "ok\n" );
			undi.pxe = pxe;
			pxe_dump();
			printf ( "Resetting pixie...\n" );
			undi_unload_base_code();
			eb_pxenv_stop_undi();
			pxe_dump();
			return 1;
		}
	}
	printf ( "none found\n" );
	ptr = 0;
	return 0;
}

/* Locate PCI PnP ROMs.
 */

static int hunt_rom ( void ) {
	static uint32_t ptr = 0;

	/* If we are not a PCI device, we cannot search for a ROM that
	 * matches us (?)
	 */
	if ( ! undi.pci.vendor )
		return 0;

	printf ( "Hunting for ROMs..." );
	if ( ptr == 0 ) ptr = 0x100000;
	while ( ptr > 0x0c0000 ) {
		ptr -= 0x800;
		undi.rom = ( rom_t * ) phys_to_virt ( ptr );
		if ( undi.rom->signature == ROM_SIGNATURE ) {
			pcir_header_t *pcir_header = NULL;
			pnp_header_t *pnp_header = NULL;

			printf ( "found 55AA at %x...", ptr );
			if ( undi.rom->pcir_off == 0 ) {
				printf ( "not a PCI ROM\n..." );
				continue;
			}
			pcir_header = (pcir_header_t*)( ( void * ) undi.rom +
							undi.rom->pcir_off );
			if ( pcir_header->signature != PCIR_SIGNATURE ) {
				printf ( "invalid PCI signature\n..." );
				continue;
			}
			printf ( "PCI:%hx:%hx...", pcir_header->vendor_id,
				 pcir_header->device_id );
			if ( ( pcir_header->vendor_id != undi.pci.vendor ) ||
			     ( pcir_header->device_id != undi.pci.dev_id ) ) {
				printf ( "not me (%hx:%hx)\n...",
					 undi.pci.vendor,
					 undi.pci.dev_id );
				continue;
			}
			if ( undi.rom->pnp_off == 0 ) {
				printf ( "not a PnP ROM\n..." );
				continue;
			}
			pnp_header = (pnp_header_t*)( ( void * ) undi.rom +
							 undi.rom->pnp_off );
			if ( pnp_header->signature != PNP_SIGNATURE ) {
				printf ( "invalid $PnP signature\n..." );
				continue;
			}
			if ( checksum(pnp_header,sizeof(pnp_header_t)) != 0 ) {
				printf ( "invalid PnP checksum\n..." );
				continue;
			}
			printf ( "ok\nROM contains %s by %s\n",
				 pnp_header->product_str_off==0 ? "(unknown)" :
				 (void*)undi.rom+pnp_header->product_str_off,
				 pnp_header->manuf_str_off==0 ? "(unknown)" :
				 (void*)undi.rom+pnp_header->manuf_str_off );
			return 1;
		}
	}
	printf ( "none found\n" );
	ptr = 0;
	undi.rom = NULL;
	return 0;
}

/* Locate ROMs containing UNDI drivers.
 */

static int hunt_undi_rom ( void ) {
	while ( hunt_rom() ) {
		if ( undi.rom->undi_rom_id_off == 0 ) {
			printf ( "Not a PXE ROM\n" );
			continue;
		}
		undi.undi_rom_id = (undi_rom_id_t *)
			( (void *)undi.rom + undi.rom->undi_rom_id_off );
		if ( undi.undi_rom_id->signature != UNDI_SIGNATURE ) {
			printf ( "Invalid UNDI signature\n" );
			continue;
		}
		if ( checksum ( undi.undi_rom_id,
				undi.undi_rom_id->struct_length ) != 0 ) {
			printf ( "Invalid checksum\n" );
			continue;
		}
		printf ( "Located UNDI ROM supporting revision %d.%d.%d\n",
			 undi.undi_rom_id->undi_rev[2],
			 undi.undi_rom_id->undi_rev[1],
			 undi.undi_rom_id->undi_rev[0] );
		return 1;
	}
	return 0;
}

/**************************************************************************
 * Low-level UNDI API call wrappers
 **************************************************************************/

/* Make a real-mode UNDI API call to the UNDI routine at
 * routine_seg:routine_off, passing in three uint16 parameters on the
 * real-mode stack.
 */

static PXENV_EXIT_t _undi_call ( uint16_t routine_seg,
			  uint16_t routine_off, uint16_t st0,
			  uint16_t st1, uint16_t st2 ) {
	PXENV_EXIT_t ret = PXENV_EXIT_FAILURE;
	struct {
		segoff_t routine;
		uint16_t st0;
		uint16_t st1;
		uint16_t st2;
	} PACKED in_stack = {
		{ routine_off, routine_seg }, st0, st1, st2
	};

	RM_FRAGMENT(rm_undi_call, 
		"popw %di\n\t"			/* %es:di = routine */
		"popw %es\n\t"
		"pushw %cs\n\t"			/* set up return address */
		"call 1f\n\t1:popw %bx\n\t"
		"leaw (2f-1b)(%bx), %ax\n\t"
		"pushw %ax\n\t"
		"pushw %es\n\t"			/* routine address to stack */
		"pushw %di\n\t"
		"lret\n\t"			/* calculated lcall */
		"\n2:\n\t"			/* continuation point */
	);

	/* Parameters are left on stack: set out_stack = in_stack */
	ret = real_call ( rm_undi_call, &in_stack, &in_stack );

	/* UNDI API calls may rudely change the status of A20 and not
	 * bother to restore it afterwards.  Intel is known to be
	 * guilty of this.
	 *
	 * Note that we will return to this point even if A20 gets
	 * screwed up by the UNDI driver, because Etherboot always
	 * resides in an even megabyte of RAM.
	 */
	gateA20_set();

	return ret;
}

/* Make a real-mode call to the UNDI loader routine at
 * routine_seg:routine_off, passing in the seg:off address of a
 * pxenv_structure on the real-mode stack.
 */

static int undi_call_loader ( void ) {
	PXENV_EXIT_t pxenv_exit = PXENV_EXIT_FAILURE;
	
	/* Hide Etherboot around the loader, so that the PXE stack
	 * doesn't trash our memory areas
	 */
	install_e820mangler ( undi.base_mem_data->e820mangler );
	hide_etherboot();
	pxenv_exit = _undi_call ( SEGMENT( undi.rom ),
				  undi.undi_rom_id->undi_loader_off,
				  OFFSET( undi.pxs ),
				  SEGMENT( undi.pxs ),
				  0 /* Unused for UNDI loader API */ );
	if ( !unhide_etherboot() ) {
		printf ( "FATAL: corrupt INT15\n" );
		return 0;
	}

	/* Return 1 for success, to be consistent with other routines */
	if ( pxenv_exit == PXENV_EXIT_SUCCESS ) return 1;
	printf ( "UNDI loader call failed with status %#hx\n",
		 undi.pxs->Status );
	return 0;
}

/* Make a real-mode UNDI API call, passing in the opcode and the
 * seg:off address of a pxenv_structure on the real-mode stack.
 *
 * Two versions: undi_call() will automatically report any failure
 * codes, undi_call_silent() will not.
 */

static int undi_call_silent ( uint16_t opcode ) {
	PXENV_EXIT_t pxenv_exit = PXENV_EXIT_FAILURE;

	pxenv_exit = _undi_call ( undi.pxe->EntryPointSP.segment,
				  undi.pxe->EntryPointSP.offset,
				  opcode,
				  OFFSET( undi.pxs ),
				  SEGMENT( undi.pxs ) );
	/* Return 1 for success, to be consistent with other routines */
	return pxenv_exit == PXENV_EXIT_SUCCESS ? 1 : 0;
}

static int undi_call ( uint16_t opcode ) {
	if ( undi_call_silent ( opcode ) ) return 1;
	printf ( "UNDI API call %#hx failed with status %#hx\n",
		 opcode, undi.pxs->Status );
	return 0;
}

#ifdef UNDI_NONTRIVIAL_IRQ
/* IRQ handler that actually calls PXENV_UNDI_ISR.  It's probably
 * better to use the trivial IRQ handler, since this seems to work for
 * just about all known NICs and doesn't involve making a PXE API call
 * in interrupt context.
 *
 * This routine is mainly used for testing the Etherboot PXE stack's
 * ability to be called in interrupt context.  It is not compiled in
 * by default.
 *
 * This code has fewer safety checks than those in the
 * trivial_irq_handler routines.  These are omitted because this code
 * is not intended for mainstream use.
 */

uint16_t nontrivial_irq_previous_trigger_count = 0;

static int copy_nontrivial_irq_handler ( void *target,
					 size_t target_size __unused ) {
	RM_FRAGMENT(nontrivial_irq_handler,
	/* Will be installed on a paragraph boundary, so access variables
	 * using %cs:(xxx-irqstart)
	 */
		"\n\t"
		"irqstart:\n\t"
	/* Fields here must match those in undi_irq_handler_t */
		"chain_to:\t.word 0,0\n\t"
		"irq_chain:\t.byte 0,0,0,0\n\t"
		"entry:\t.word 0,0\n\t"
		"count_all:\t.word 0\n\t"
		"count_ours:\t.word 0\n\t"
		"undi_isr:\n\t"
		"undi_isr_Status:\t.word 0\n\t"
		"undi_isr_FuncFlag:\t.word 0\n\t"
		"undi_isr_others:\t.word 0,0,0,0,0,0\n\t"
		"handler:\n\t"
	/* Assume that PXE stack will corrupt everything */
		"pushal\n\t"
		"push %ds\n\t"
		"push %es\n\t"
		"push %fs\n\t"
		"push %gs\n\t"
	/* Set DS == CS */	
		"pushw %cs\n\t"
		"popw %ds\n\t"
	/* Set up parameters for call */
		"movw $" RM_STR(PXENV_UNDI_ISR_IN_START) ", %ds:(undi_isr_FuncFlag-irqstart)\n\t"
		"pushw %cs\n\t"
		"popw %es\n\t"
		"movw $(undi_isr-irqstart), %di\n\t"
		"movw $" RM_STR(PXENV_UNDI_ISR) ", %bx\n\t"
		"pushw %es\n\t"    /* Registers for PXENV+, stack for !PXE */
		"pushw %di\n\t"
		"pushw %bx\n\t"
	/* Make PXE API call */
		"lcall *%ds:(entry-irqstart)\n\t"
		"addw $6, %sp\n\t"
	/* Set DS == CS */	
		"pushw %cs\n\t"
		"popw %ds\n\t"
	/* Check return status to see if it's one of our interrupts */
		"cmpw $" RM_STR(PXENV_STATUS_SUCCESS) ", %cs:(undi_isr_Status-irqstart)\n\t"
		"jne 1f\n\t"
		"cmpw $" RM_STR(PXENV_UNDI_ISR_OUT_OURS) ", %cs:(undi_isr_FuncFlag-irqstart)\n\t"
		"jne 1f\n\t"
	/* Increment count_ours if so */
		"incw %ds:(count_ours-irqstart)\n\t"
		"1:\n\t"
	/* Increment count_all anyway */
		"incw %ds:(count_all-irqstart)\n\t"
	/* Restore registers and return */
		"popw %gs\n\t"
		"popw %fs\n\t"
		"popw %es\n\t"
		"popw %ds\n\t"
		"popal\n\t"
		"\n\t"
	/* Chain to acknowledge the interrupt */
		"cmpb $0, %cs:(irq_chain-irqstart)\n\t"
		"jz 2f\n\t"
		"ljmp %cs:(chain_to-irqstart)\n\t"
		"2:\n\t"
		"\n\t"
		"iret\n\t"
		"\n\t"
	);

	/* Copy handler */
	memcpy ( target, nontrivial_irq_handler, NONTRIVIAL_IRQ_HANDLER_SIZE );

	return 1;
}

static int install_nontrivial_irq_handler ( irq_t irq ) {
	undi_irq_handler_t *handler = 
		&undi.base_mem_data->nontrivial_irq_handler;
	segoff_t isr_segoff;

	printf ( "WARNING: using non-trivial IRQ handler [EXPERIMENTAL]\n" );
	/*
	 * This code is deliberately quick and dirty.  The whole
	 * nontrivial IRQ stuff is only present in order to test out
	 * calling our PXE stack in interrupt context.  Do NOT use
	 * this in production code.
	 */

	disable_irq ( irq );
	handler->count_all = 0;
	handler->count_ours = 0;
	handler->entry = undi.pxe->EntryPointSP;
	nontrivial_irq_previous_trigger_count = 0;
	isr_segoff.segment = SEGMENT(handler);
	isr_segoff.offset = (void*)&handler->code - (void*)handler;
	install_irq_handler( irq, &isr_segoff, 
		&handler->irq_chain, &handler->chain_to);
	enable_irq ( irq );

	return 1;
}

static int remove_nontrivial_irq_handler ( irq_t irq ) {
	undi_irq_handler_t *handler = 
		&undi.base_mem_data->nontrivial_irq_handler;
	segoff_t isr_segoff;

	isr_segoff.segment = SEGMENT(handler);
	isr_segoff.offset = (char*)&handler->code - (char*)handler;
	remove_irq_handler( irq, &isr_segoff, 
		&handler->irq_chain, &handler->chain_to);
	return 1;
}

static int nontrivial_irq_triggered ( irq_t irq __unused ) {
	undi_irq_handler_t *handler = 
		&undi.base_mem_data->nontrivial_irq_handler;
	uint16_t nontrivial_irq_this_trigger_count = handler->count_ours;
	int triggered = ( nontrivial_irq_this_trigger_count -
			  nontrivial_irq_previous_trigger_count );

	nontrivial_irq_previous_trigger_count =
		nontrivial_irq_this_trigger_count;
	return triggered ? 1 : 0;
}

static void nontrivial_irq_debug ( irq_t irq ) {
	undi_irq_handler_t *handler = 
		&undi.base_mem_data->nontrivial_irq_handler;

	printf ( "IRQ %d triggered %d times (%d of which were ours)\n",
		 irq, handler->count_all, handler->count_ours );
}
#endif /* UNDI_NONTRIVIAL_IRQ */
	
/**************************************************************************
 * High-level UNDI API call wrappers
 **************************************************************************/

/* Install the UNDI driver from a located UNDI ROM.
 */

static int undi_loader ( void ) {
	pxe_t *pxe = NULL;

	if ( ! undi.pci.vendor ) {
		printf ( "ERROR: attempted to call loader of an ISA ROM?\n" );
		return 0;
	}

	/* AX contains PCI bus:devfn (PCI specification) */
	undi.pxs->loader.ax = ( undi.pci.bus << 8 ) | undi.pci.devfn;
	/* BX and DX set to 0xffff for non-ISAPnP devices
	 * (BIOS boot specification)
	 */
	undi.pxs->loader.bx = 0xffff;
	undi.pxs->loader.dx = 0xffff;
	/* ES:DI points to PnP BIOS' $PnP structure
	 * (BIOS boot specification)
	 */
	if ( undi.pnp_bios ) {
		undi.pxs->loader.es = 0xf000;
		undi.pxs->loader.di = virt_to_phys ( undi.pnp_bios ) - 0xf0000;
	} else {
		/* Set to a NULL pointer and hope that we don't need it */
		undi.pxs->loader.es = 0x0000;
		undi.pxs->loader.di = 0x0000;
	}

	/* Allocate space for UNDI driver's code and data segments */
	undi.driver_code_size = undi.undi_rom_id->code_size;
	undi.driver_code = allot_base_memory ( undi.driver_code_size );
	if ( undi.driver_code == NULL ) {
		printf ( "Could not allocate %d bytes for UNDI code segment\n",
			 undi.driver_code_size );
		return 0;
	}
	undi.pxs->loader.undi_cs = SEGMENT( undi.driver_code );

	undi.driver_data_size = undi.undi_rom_id->data_size;
	undi.driver_data = allot_base_memory ( undi.driver_data_size );
	if ( undi.driver_data == NULL ) {
		printf ( "Could not allocate %d bytes for UNDI code segment\n",
			 undi.driver_data_size );
		return 0;
	}
	undi.pxs->loader.undi_ds = SEGMENT( undi.driver_data );

	printf ( "Installing UNDI driver code to %hx:0000, data at %hx:0000\n",
		undi.pxs->loader.undi_cs, undi.pxs->loader.undi_ds );

	/* Do the API call to install the loader */
	if ( ! undi_call_loader () ) return 0;

	pxe = VIRTUAL( undi.pxs->loader.undi_cs,
		       undi.pxs->loader.pxe_ptr.offset );
	printf ( "UNDI driver created a pixie at %hx:%hx...",
		 undi.pxs->loader.undi_cs, undi.pxs->loader.pxe_ptr.offset );
	if ( memcmp ( pxe->Signature, "!PXE", 4 ) != 0 ) {
		printf ( "invalid signature\n" );
		return 0;
	}
	if ( checksum ( pxe, sizeof(pxe_t) ) != 0 ) {
		printf ( "invalid checksum\n" );
		return 0;
	}
	printf ( "ok\n" );
	undi.pxe = pxe;
	pxe_dump();
	return 1;
}

/* Start the UNDI driver.
 */

static int eb_pxenv_start_undi ( void ) {
	int success = 0;

	/* AX contains PCI bus:devfn (PCI specification) */
	undi.pxs->start_undi.ax = ( undi.pci.bus << 8 ) | undi.pci.devfn;

	/* BX and DX set to 0xffff for non-ISAPnP devices
	 * (BIOS boot specification)
	 */
	undi.pxs->start_undi.bx = 0xffff;
	undi.pxs->start_undi.dx = 0xffff;
	/* ES:DI points to PnP BIOS' $PnP structure
	 * (BIOS boot specification)
	 */
	if ( undi.pnp_bios ) {
		undi.pxs->start_undi.es = 0xf000;
		undi.pxs->start_undi.di =
			virt_to_phys ( undi.pnp_bios ) - 0xf0000;
	} else {
		/* Set to a NULL pointer and hope that we don't need it */
		undi.pxs->start_undi.es = 0x0000;
		undi.pxs->start_undi.di = 0x0000;
	}

	DBG ( "PXENV_START_UNDI => AX=%hx BX=%hx DX=%hx ES:DI=%hx:%hx\n",
	      undi.pxs->start_undi.ax,
	      undi.pxs->start_undi.bx, undi.pxs->start_undi.dx,
	      undi.pxs->start_undi.es, undi.pxs->start_undi.di );
	success = undi_call ( PXENV_START_UNDI );
	DBG ( "PXENV_START_UNDI <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	if ( success ) undi.prestarted = 1;
	return success;
}

static int eb_pxenv_undi_startup ( void ) {
	int success = 0;

	DBG ( "PXENV_UNDI_STARTUP => (void)\n" );
	success = undi_call ( PXENV_UNDI_STARTUP );
	DBG ( "PXENV_UNDI_STARTUP <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	if ( success ) undi.started = 1;
	return success;
}

static int eb_pxenv_undi_cleanup ( void ) {
	int success = 0;

	DBG ( "PXENV_UNDI_CLEANUP => (void)\n" );
	success = undi_call ( PXENV_UNDI_CLEANUP );
	DBG ( "PXENV_UNDI_CLEANUP <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	return success;
}

static int eb_pxenv_undi_initialize ( void ) {
	int success = 0;

	undi.pxs->undi_initialize.ProtocolIni = 0;
	memset ( &undi.pxs->undi_initialize.reserved, 0,
		 sizeof ( undi.pxs->undi_initialize.reserved ) );
	DBG ( "PXENV_UNDI_INITIALIZE => ProtocolIni=%x\n" );
	success = undi_call ( PXENV_UNDI_INITIALIZE );
	DBG ( "PXENV_UNDI_INITIALIZE <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	if ( success ) undi.initialized = 1;
	return success;
}

static int eb_pxenv_undi_shutdown ( void ) {
	int success = 0;

	DBG ( "PXENV_UNDI_SHUTDOWN => (void)\n" );
	success = undi_call ( PXENV_UNDI_SHUTDOWN );
	DBG ( "PXENV_UNDI_SHUTDOWN <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	if ( success ) {
		undi.initialized = 0;
		undi.started = 0;
	}
	return success;
}

static int eb_pxenv_undi_open ( void ) {
	int success = 0;

	undi.pxs->undi_open.OpenFlag = 0;
	undi.pxs->undi_open.PktFilter = FLTR_DIRECTED | FLTR_BRDCST;
	
	/* Multicast support not yet implemented */
	undi.pxs->undi_open.R_Mcast_Buf.MCastAddrCount = 0;
	DBG ( "PXENV_UNDI_OPEN => OpenFlag=%hx PktFilter=%hx "
	      "MCastAddrCount=%hx\n",
	      undi.pxs->undi_open.OpenFlag, undi.pxs->undi_open.PktFilter,
	      undi.pxs->undi_open.R_Mcast_Buf.MCastAddrCount );
	success = undi_call ( PXENV_UNDI_OPEN );
	DBG ( "PXENV_UNDI_OPEN <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	if ( success ) undi.opened = 1;
	return success;	
}

static int eb_pxenv_undi_close ( void ) {
	int success = 0;

	DBG ( "PXENV_UNDI_CLOSE => (void)\n" );
	success = undi_call ( PXENV_UNDI_CLOSE );
	DBG ( "PXENV_UNDI_CLOSE <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	if ( success ) undi.opened = 0;
	return success;
}

static int eb_pxenv_undi_transmit_packet ( void ) {
	int success = 0;
	static const uint8_t broadcast[] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

	/* XMitFlag selects unicast / broadcast */
	if ( memcmp ( undi.xmit_data->destaddr, broadcast,
		      sizeof(broadcast) ) == 0 ) {
		undi.pxs->undi_transmit.XmitFlag = XMT_BROADCAST;
	} else {
		undi.pxs->undi_transmit.XmitFlag = XMT_DESTADDR;
	}

	/* Zero reserved dwords */
	undi.pxs->undi_transmit.Reserved[0] = 0;
	undi.pxs->undi_transmit.Reserved[1] = 0;

	/* Segment:offset pointer to DestAddr in base memory */
	undi.pxs->undi_transmit.DestAddr.segment =
		SEGMENT( undi.xmit_data->destaddr );
	undi.pxs->undi_transmit.DestAddr.offset =
		OFFSET( undi.xmit_data->destaddr );

	/* Segment:offset pointer to TBD in base memory */
	undi.pxs->undi_transmit.TBD.segment = SEGMENT( &undi.xmit_data->tbd );
	undi.pxs->undi_transmit.TBD.offset = OFFSET( &undi.xmit_data->tbd );

	/* Use only the "immediate" part of the TBD */
	undi.xmit_data->tbd.DataBlkCount = 0;
	
	DBG ( "PXENV_UNDI_TRANSMIT_PACKET => Protocol=%hx XmitFlag=%hx ...\n"
	      "... DestAddr=%hx:%hx TBD=%hx:%hx ...\n",
	      undi.pxs->undi_transmit.Protocol,
	      undi.pxs->undi_transmit.XmitFlag,
	      undi.pxs->undi_transmit.DestAddr.segment,
	      undi.pxs->undi_transmit.DestAddr.offset,
	      undi.pxs->undi_transmit.TBD.segment,
	      undi.pxs->undi_transmit.TBD.offset );
	DBG ( "... TBD { ImmedLength=%hx Xmit=%hx:%hx DataBlkCount=%hx }\n",
	      undi.xmit_data->tbd.ImmedLength,
	      undi.xmit_data->tbd.Xmit.segment,
	      undi.xmit_data->tbd.Xmit.offset,
	      undi.xmit_data->tbd.DataBlkCount );
	success = undi_call ( PXENV_UNDI_TRANSMIT );
	DBG ( "PXENV_UNDI_TRANSMIT_PACKET <= Status=%s\n",
	      UNDI_STATUS(undi.pxs) );
	return success;
}

static int eb_pxenv_undi_set_station_address ( void ) {
	/* This will spuriously fail on some cards.  Ignore failures.
	 * We only ever use it to set the MAC address to the card's
	 * permanent value anyway, so it's a useless call (although we
	 * make it because PXE spec says we should).
	 */
	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS => "
	      "StationAddress=%!\n",
	      undi.pxs->undi_set_station_address.StationAddress );
	undi_call_silent ( PXENV_UNDI_SET_STATION_ADDRESS );
	DBG ( "PXENV_UNDI_SET_STATION_ADDRESS <= Status=%s\n",
	      UNDI_STATUS(undi.pxs) );
	return 1;
}

static int eb_pxenv_undi_get_information ( void ) {
	int success = 0;
	memset ( undi.pxs, 0, sizeof ( undi.pxs ) );
	DBG ( "PXENV_UNDI_GET_INFORMATION => (void)\n" );
	success = undi_call ( PXENV_UNDI_GET_INFORMATION );
	DBG ( "PXENV_UNDI_GET_INFORMATION <= Status=%s "
	      "BaseIO=%hx IntNumber=%hx ...\n"
	      "... MaxTranUnit=%hx HwType=%hx HwAddrlen=%hx ...\n"
	      "... CurrentNodeAddress=%! PermNodeAddress=%! ...\n"
	      "... ROMAddress=%hx RxBufCt=%hx TxBufCt=%hx\n",
	      UNDI_STATUS(undi.pxs),
	      undi.pxs->undi_get_information.BaseIo,
	      undi.pxs->undi_get_information.IntNumber,
	      undi.pxs->undi_get_information.MaxTranUnit,
	      undi.pxs->undi_get_information.HwType,
	      undi.pxs->undi_get_information.HwAddrLen,
	      undi.pxs->undi_get_information.CurrentNodeAddress,
	      undi.pxs->undi_get_information.PermNodeAddress,
	      undi.pxs->undi_get_information.ROMAddress,
	      undi.pxs->undi_get_information.RxBufCt,
	      undi.pxs->undi_get_information.TxBufCt );
	return success;
}

static int eb_pxenv_undi_get_iface_info ( void ) {
	int success = 0;

	DBG ( "PXENV_UNDI_GET_IFACE_INFO => (void)\n" );
	success = undi_call ( PXENV_UNDI_GET_IFACE_INFO );
	DBG ( "PXENV_UNDI_GET_IFACE_INFO <= Status=%s IfaceType=%s ...\n"
	      "... LinkSpeed=%x ServiceFlags=%x\n",
	      UNDI_STATUS(undi.pxs),
	      undi.pxs->undi_get_iface_info.IfaceType,
	      undi.pxs->undi_get_iface_info.LinkSpeed,
	      undi.pxs->undi_get_iface_info.ServiceFlags );
	return success;
}

static int eb_pxenv_undi_isr ( void ) {
	int success = 0;

	DBG ( "PXENV_UNDI_ISR => FuncFlag=%hx\n",
	      undi.pxs->undi_isr.FuncFlag );	
	success = undi_call ( PXENV_UNDI_ISR );
	DBG ( "PXENV_UNDI_ISR <= Status=%s FuncFlag=%hx BufferLength=%hx ...\n"
	      "... FrameLength=%hx FrameHeaderLength=%hx Frame=%hx:%hx "
	      "ProtType=%hhx ...\n... PktType=%hhx\n",
	      UNDI_STATUS(undi.pxs), undi.pxs->undi_isr.FuncFlag,
	      undi.pxs->undi_isr.BufferLength,
	      undi.pxs->undi_isr.FrameLength,
	      undi.pxs->undi_isr.FrameHeaderLength,
	      undi.pxs->undi_isr.Frame.segment,
	      undi.pxs->undi_isr.Frame.offset,
	      undi.pxs->undi_isr.ProtType,
	      undi.pxs->undi_isr.PktType );
	return success;
}

static int eb_pxenv_stop_undi ( void ) {
	int success = 0;

	DBG ( "PXENV_STOP_UNDI => (void)\n" );
	success = undi_call ( PXENV_STOP_UNDI );
	DBG ( "PXENV_STOP_UNDI <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	if ( success ) undi.prestarted = 0;
	return success;
}

static int eb_pxenv_unload_stack ( void ) {
	int success = 0;

	memset ( undi.pxs, 0, sizeof ( undi.pxs ) );
	DBG ( "PXENV_UNLOAD_STACK => (void)\n" );
	success = undi_call_silent ( PXENV_UNLOAD_STACK );
	DBG ( "PXENV_UNLOAD_STACK <= Status=%s ...\n... (%s)\n",
	      UNDI_STATUS(undi.pxs),
	      ( undi.pxs->Status == PXENV_STATUS_SUCCESS ?
		"base-code is ready to be removed" :
		( undi.pxs->Status == PXENV_STATUS_FAILURE ?
		  "the size of free base memory has been changed" :
		  ( undi.pxs->Status == PXENV_STATUS_KEEP_ALL ?
		    "the NIC interrupt vector has been changed" :
		    "UNEXPECTED STATUS CODE" ) ) ) );
	return success;
}

static int eb_pxenv_stop_base ( void ) {
	int success = 0;

	DBG ( "PXENV_STOP_BASE => (void)\n" );
	success = undi_call ( PXENV_STOP_BASE );
	DBG ( "PXENV_STOP_BASE <= Status=%s\n", UNDI_STATUS(undi.pxs) );
	return success;
}

/* Unload UNDI base code (if any present) and free memory.
 */
static int undi_unload_base_code ( void ) {
	void *bc_code = VIRTUAL( undi.pxe->BC_Code.Seg_Addr, 0 );
	size_t bc_code_size = undi.pxe->BC_Code.Seg_Size;
	void *bc_data = VIRTUAL( undi.pxe->BC_Data.Seg_Addr, 0 );
	size_t bc_data_size = undi.pxe->BC_Data.Seg_Size;
	void *bc_stck = VIRTUAL( undi.pxe->Stack.Seg_Addr, 0 );
	size_t bc_stck_size = undi.pxe->Stack.Seg_Size;
	firing_squad_lineup_t lineup;

	/* Since we never start the base code, the only time we should
	 * reach this is if we were loaded via PXE.  There are many
	 * different and conflicting versions of the "correct" way to
	 * unload the PXE base code, several of which appear within
	 * the PXE specification itself.  This one seems to work for
	 * our purposes.
	 *
	 * We always call PXENV_STOP_BASE and PXENV_UNLOAD_STACK even
	 * if the !PXE structure indicates that no base code is
	 * present.  We do this for the case that there is a
	 * base-code-less UNDI driver loaded that has hooked some
	 * interrupts.  If the base code really is absent, then these
	 * calls will fail, we will ignore the failure, and our
	 * subsequent memory-freeing code is robust enough to handle
	 * whatever's thrown at it.
	 */
	eb_pxenv_stop_base();
	eb_pxenv_unload_stack();
	if ( ( undi.pxs->unload_stack.Status != PXENV_STATUS_SUCCESS ) &&
	     ( undi.pxs->unload_stack.Status != PXENV_STATUS_FAILURE ) &&
	     ( undi.pxe->BC_Code.Seg_Addr != 0 ) )
	{
		printf ( "Could not free memory allocated to PXE base code: "
			 "possible memory leak\n" );
		return 0;
	}
	/* Free data structures.  Forget what the PXE specification
	 * says about how to calculate the new size of base memory;
	 * basemem.c takes care of all that for us.  Note that we also
	 * have to free the stack (even though PXE spec doesn't say
	 * anything about it) because nothing else is going to do so.
	 *
	 * Structures will almost certainly not be kB-aligned and
	 * there's a reasonable chance that the UNDI code or data
	 * portions will lie in the same kB as the base code.  Since
	 * forget_base_memory works only in 1kB increments, this means
	 * we have to do some arcane trickery.
	 */
	memset ( &lineup, 0, sizeof(lineup) );
	if ( SEGMENT(bc_code) != 0 )
		assemble_firing_squad( &lineup, bc_code, bc_code_size, SHOOT );
	if ( SEGMENT(bc_data) != 0 )
		assemble_firing_squad( &lineup, bc_data, bc_data_size, SHOOT );
	if ( SEGMENT(bc_stck) != 0 )
		assemble_firing_squad( &lineup, bc_stck, bc_stck_size, SHOOT );
	/* Don't shoot any bits of the UNDI driver code or data */
	assemble_firing_squad ( &lineup,
				VIRTUAL(undi.pxe->UNDICode.Seg_Addr, 0),
				undi.pxe->UNDICode.Seg_Size, DONTSHOOT );
	assemble_firing_squad ( &lineup,
				VIRTUAL(undi.pxe->UNDIData.Seg_Addr, 0),
				undi.pxe->UNDIData.Seg_Size, DONTSHOOT );
	shoot_targets ( &lineup );
	undi.pxe->BC_Code.Seg_Addr = 0;
	undi.pxe->BC_Data.Seg_Addr = 0;
	undi.pxe->Stack.Seg_Addr = 0;

	/* Free and reallocate our own base memory data structures, to
	 * allow the freed base-code blocks to be fully released.
	 */
	free_base_mem_data();
	if ( ! allocate_base_mem_data() ) {
		printf ( "FATAL: memory unaccountably lost\n" );
		while ( 1 ) {};
	}

	return 1;
}

/* UNDI full initialization
 *
 * This calls all the various UNDI initialization routines in sequence.
 */

static int undi_full_startup ( void ) {
	if ( ! eb_pxenv_start_undi() ) return 0;
	if ( ! eb_pxenv_undi_startup() ) return 0;
	if ( ! eb_pxenv_undi_initialize() ) return 0;
	if ( ! eb_pxenv_undi_get_information() ) return 0;
	undi.irq = undi.pxs->undi_get_information.IntNumber;
	copy_undi_irq_handler ( undi.base_mem_data->irq_handler,
				UNDI_IRQ_HANDLER_SIZE );
	if ( ! install_undi_irq_handler ( undi.irq ) ) {
		undi.irq = IRQ_NONE;
		return 0;
	}
	memmove ( &undi.pxs->undi_set_station_address.StationAddress,
		  &undi.pxs->undi_get_information.PermNodeAddress,
		  sizeof (undi.pxs->undi_set_station_address.StationAddress) );
	if ( ! eb_pxenv_undi_set_station_address() ) return 0;
	if ( ! eb_pxenv_undi_open() ) return 0;
	return 1;
}

/* UNDI full shutdown
 *
 * This calls all the various UNDI shutdown routines in sequence and
 * also frees any memory that it can.
 */

static int undi_full_shutdown ( void ) {
	if ( undi.pxe != NULL ) {
		/* In case we didn't allocate the driver's memory in the first
		 * place, try to grab the code and data segments and sizes
		 * from the !PXE structure.
		 */
		if ( undi.driver_code == NULL ) {
			undi.driver_code = VIRTUAL(undi.pxe->UNDICode.Seg_Addr,
						   0 );
			undi.driver_code_size = undi.pxe->UNDICode.Seg_Size;
		}
		if ( undi.driver_data == NULL ) {
			undi.driver_data = VIRTUAL(undi.pxe->UNDIData.Seg_Addr,
						   0 );
			undi.driver_data_size = undi.pxe->UNDIData.Seg_Size;
		}
		
		/* Ignore errors and continue in the hope of shutting
		 * down anyway
		 */
		if ( undi.opened ) eb_pxenv_undi_close();
		if ( undi.started ) {
			eb_pxenv_undi_cleanup();
			/* We may get spurious UNDI API errors at this
			 * point.  If startup() succeeded but
			 * initialize() failed then according to the
			 * spec, we should call shutdown().  However,
			 * some NICS will fail with a status code
			 * 0x006a (INVALID_STATE).
			 */
			eb_pxenv_undi_shutdown();
		}
		if ( undi.irq != IRQ_NONE ) {
			remove_undi_irq_handler ( undi.irq );
			undi.irq = IRQ_NONE;
		}
		undi_unload_base_code();
		if ( undi.prestarted ) {
			eb_pxenv_stop_undi();
			/* Success OR Failure indicates that memory
			 * can be freed.  Any other status code means
			 * that it can't.
			 */
			if (( undi.pxs->Status == PXENV_STATUS_KEEP_UNDI ) ||
			    ( undi.pxs->Status == PXENV_STATUS_KEEP_ALL ) ) {
				printf ("Could not free memory allocated to "
					"UNDI driver: possible memory leak\n");
				return 0;
			}
		}
	}
	/* Free memory allocated to UNDI driver */
	if ( undi.driver_code != NULL ) {
		/* Clear contents in order to eliminate !PXE and PXENV
		 * signatures to prevent spurious detection via base
		 * memory scan.
		 */
		memset ( undi.driver_code, 0, undi.driver_code_size );
		forget_base_memory ( undi.driver_code, undi.driver_code_size );
		undi.driver_code = NULL;
		undi.driver_code_size = 0;
	}
	if ( undi.driver_data != NULL ) {
		forget_base_memory ( undi.driver_data, undi.driver_data_size );
		undi.driver_data = NULL;
		undi.driver_data_size = 0;
	}
	/* !PXE structure now gone; memory freed */
	undi.pxe = NULL;
	return 1;
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static int undi_poll(struct nic *nic, int retrieve)
{
	/* Fun, fun, fun.  UNDI drivers don't use polling; they use
	 * interrupts.  We therefore cheat and pretend that an
	 * interrupt has occurred every time undi_poll() is called.
	 * This isn't too much of a hack; PCI devices share IRQs and
	 * so the first thing that a proper ISR should do is call
	 * PXENV_UNDI_ISR to determine whether or not the UNDI NIC
	 * generated the interrupt; there is no harm done by spurious
	 * calls to PXENV_UNDI_ISR.  Similarly, we wouldn't be
	 * handling them any more rapidly than the usual rate of
	 * undi_poll() being called even if we did implement a full
	 * ISR.  So it should work.  Ha!
	 *
	 * Addendum (21/10/03).  Some cards don't play nicely with
	 * this trick, so instead of doing it the easy way we have to
	 * go to all the hassle of installing a genuine interrupt
	 * service routine and dealing with the wonderful 8259
	 * Programmable Interrupt Controller.  Joy.
	 */

	/* See if a hardware interrupt has occurred since the last poll().
	 */
	if ( ! undi_irq_triggered ( undi.irq ) ) return 0;

	/* Given the frailty of PXE stacks, it's probably not safe to
	 * risk calling PXENV_UNDI_ISR with
	 * FuncFlag=PXENV_UNDI_ISR_START twice for the same interrupt,
	 * so we cheat slightly and assume that there is something
	 * ready to retrieve as long as an interrupt has occurred.
	 */
	if ( ! retrieve ) return 1;

#ifdef UNDI_NONTRIVIAL_IRQ
	/* With the nontrivial IRQ handler, we have already called
	 * PXENV_UNDI_ISR with PXENV_UNDI_ISR_IN_START and determined
	 * that it is one of ours.
	 */
#else
	/* Ask the UNDI driver if this is "our" interrupt.
	 */
	undi.pxs->undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_START;
	if ( ! eb_pxenv_undi_isr() ) return 0;
	if ( undi.pxs->undi_isr.FuncFlag == PXENV_UNDI_ISR_OUT_NOT_OURS ) {
		/* "Not our interrupt" translates to "no packet ready
		 * to read".
		 */
		/* FIXME: Technically, we shouldn't be the one sending
		 * EOI.  However, since our IRQ handlers don't yet
		 * support chaining, nothing else gets the chance to.
		 * One nice side-effect of doing this is that it means
		 * we can cheat and claim the timer interrupt as our
		 * NIC interrupt; it will be inefficient but will
		 * work.
		 */
		send_specific_eoi ( undi.irq );
		return 0;
	}
#endif

	/* At this stage, the device should have cleared its interrupt
	 * line so we can send EOI to the 8259.
	 */
	send_specific_eoi ( undi.irq );

	/* We might have received a packet, or this might be a
	 * "transmit completed" interrupt.  Zero nic->packetlen,
	 * increment whenever we receive a bit of a packet, test
	 * nic->packetlen when we're done to see whether or not we
	 * actually received anything.
	 */
	nic->packetlen = 0;
	undi.pxs->undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_PROCESS;
	if ( ! eb_pxenv_undi_isr() ) return 0;
	while ( undi.pxs->undi_isr.FuncFlag != PXENV_UNDI_ISR_OUT_DONE ) {
		switch ( undi.pxs->undi_isr.FuncFlag ) {
		case PXENV_UNDI_ISR_OUT_TRANSMIT:
			/* We really don't care about transmission complete
			 * interrupts.
			 */
			break;
		case PXENV_UNDI_ISR_OUT_BUSY:
			/* This should never happen.
			 */
			printf ( "UNDI ISR thinks it's being re-entered!\n"
				 "Aborting receive\n" );
			return 0;
		case PXENV_UNDI_ISR_OUT_RECEIVE:
			/* Copy data to receive buffer */
			memcpy ( nic->packet + nic->packetlen,
				 VIRTUAL( undi.pxs->undi_isr.Frame.segment,
					  undi.pxs->undi_isr.Frame.offset ),
				 undi.pxs->undi_isr.BufferLength );
			nic->packetlen += undi.pxs->undi_isr.BufferLength;
			break;
		default:
			printf ( "UNDI ISR returned bizzare status code %d\n",
				 undi.pxs->undi_isr.FuncFlag );
		}
		undi.pxs->undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
		if ( ! eb_pxenv_undi_isr() ) return 0;
	}
	return nic->packetlen > 0 ? 1 : 0;
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static void undi_transmit(
	struct nic *nic __unused,
	const char *d,			/* Destination */
	unsigned int t,			/* Type */
	unsigned int s,			/* size */
	const char *p)			/* Packet */
{
	/* Copy destination to buffer in base memory */
	memcpy ( undi.xmit_data->destaddr, d, sizeof(MAC_ADDR) );

	/* Translate packet type to UNDI packet type */
	switch ( t ) {
	case IP :  undi.pxs->undi_transmit.Protocol = P_IP;   break;
	case ARP:  undi.pxs->undi_transmit.Protocol = P_ARP;  break;
	case RARP: undi.pxs->undi_transmit.Protocol = P_RARP; break;
	default: printf ( "Unknown packet type %hx\n", t );
		return;
	}

	/* Store packet length in TBD */
	undi.xmit_data->tbd.ImmedLength = s;

	/* Check to see if data to be transmitted is currently in base
	 * memory.  If not, allocate temporary storage in base memory
	 * and copy it there.
	 */
	if ( SEGMENT( p ) <= 0xffff ) {
		undi.xmit_data->tbd.Xmit.segment = SEGMENT( p );
		undi.xmit_data->tbd.Xmit.offset = OFFSET( p );
	} else {
		memcpy ( undi.xmit_buffer, p, s );
		undi.xmit_data->tbd.Xmit.segment = SEGMENT( undi.xmit_buffer );
		undi.xmit_data->tbd.Xmit.offset = OFFSET( undi.xmit_buffer );
	}

	eb_pxenv_undi_transmit_packet();
}

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void undi_disable ( struct dev *dev __unused ) {
	undi_full_shutdown();
	free_base_mem_data();
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/

/* Locate an UNDI driver by first scanning through base memory for an
 * installed driver and then by scanning for UNDI ROMs and attempting
 * to install their drivers.
 */

static int hunt_pixies_and_undi_roms ( void ) {
	static uint8_t hunt_type = HUNT_FOR_PIXIES;
	
	if ( hunt_type == HUNT_FOR_PIXIES ) {
		if ( hunt_pixie() ) {
			return 1;
		}
	}
	hunt_type = HUNT_FOR_UNDI_ROMS;
	while ( hunt_undi_rom() ) {
		if ( undi_loader() ) {
			return 1;
		}
		undi_full_shutdown(); /* Free any allocated memory */
	}
	hunt_type = HUNT_FOR_PIXIES;
	return 0;
}

/* The actual Etherboot probe routine.
 */

static int undi_probe(struct dev *dev, struct pci_device *pci)
{
	struct nic *nic = (struct nic *)dev;

	/* Zero out global undi structure */
	memset ( &undi, 0, sizeof(undi) );

	/* Store PCI parameters; we will need them to initialize the
	 * UNDI driver later.  If not a PCI device, leave as 0.
	 */
	if ( pci ) {
		memcpy ( &undi.pci, pci, sizeof(undi.pci) );
	}

	/* Find the BIOS' $PnP structure */
	if ( ! hunt_pnp_bios() ) {
		/* Not all PXE stacks actually insist on a PnP BIOS.
		 * In particular, an Etherboot PXE stack will work
		 * just fine without one.
		 *
		 * We used to make this a fatal error, but now we just
		 * warn and continue.  Note that this is necessary in
		 * order to be able to debug the Etherboot PXE stack
		 * under Bochs, since Bochs' BIOS is non-PnP.
		 */
		printf ( "WARNING: No PnP BIOS found\n" );
	}

	/* Allocate base memory data structures */
	if ( ! allocate_base_mem_data() ) return 0;

	/* Search thoroughly for UNDI drivers */
	for ( ; hunt_pixies_and_undi_roms(); undi_full_shutdown() ) {
		/* Try to initialise UNDI driver */
		printf ( "Initializing UNDI driver.  Please wait...\n" );
		if ( ! undi_full_startup() ) {
			if ( undi.pxs->Status ==
			     PXENV_STATUS_UNDI_MEDIATEST_FAILED ) {
				printf ( "Cable not connected (code %#hx)\n",
					 PXENV_STATUS_UNDI_MEDIATEST_FAILED );
			}
			continue;
		}
		/* Basic information: MAC, IO addr, IRQ */
		if ( ! eb_pxenv_undi_get_information() ) continue;
		printf ( "Initialized UNDI NIC with IO %#hx, IRQ %d, MAC %!\n",
			 undi.pxs->undi_get_information.BaseIo,
			 undi.pxs->undi_get_information.IntNumber,
			 undi.pxs->undi_get_information.CurrentNodeAddress );
		/* Fill out MAC address in nic structure */
		memcpy ( nic->node_addr,
			 undi.pxs->undi_get_information.CurrentNodeAddress,
			 ETH_ALEN );
		/* More diagnostic information including link speed */
		if ( ! eb_pxenv_undi_get_iface_info() ) continue;
		printf ( "NDIS type %s interface at %d Mbps\n",
			 undi.pxs->undi_get_iface_info.IfaceType,
			 undi.pxs->undi_get_iface_info.LinkSpeed / 1000000 );
		dev->disable  = undi_disable;
		nic->poll     = undi_poll;
		nic->transmit = undi_transmit;
		return 1;
	}
	undi_disable ( dev ); /* To free base memory structures */
	return 0;
}

static int undi_isa_probe ( struct dev *dev,
			    unsigned short *probe_addrs __unused ) {
	return undi_probe ( dev, NULL );
}


/* UNDI driver states that it is suitable for any PCI NIC (i.e. any
 * PCI device of class PCI_CLASS_NETWORK_ETHERNET).  If there are any
 * obscure UNDI NICs that have the incorrect PCI class, add them to
 * this list.
 */
static struct pci_id undi_nics[] = {
	/* PCI_ROM(0x0000, 0x0000, "undi", "UNDI adaptor"), */
};

static struct pci_driver undi_driver __pci_driver = {
	.type     = NIC_DRIVER,
	.name     = "UNDI",
	.probe    = undi_probe,
	.ids      = undi_nics,
 	.id_count = sizeof(undi_nics)/sizeof(undi_nics[0]),
	.class    = PCI_CLASS_NETWORK_ETHERNET,
};

static struct isa_driver undi_isa_driver __isa_driver = {
	.type     = NIC_DRIVER,
	.name     = "UNDI",
	.probe    = undi_isa_probe,
	.ioaddrs  = 0,
};

#endif /* PCBIOS */
