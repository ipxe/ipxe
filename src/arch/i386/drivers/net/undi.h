/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
UNDI NIC driver for Etherboot - header file

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

#include "pxe.h"
#include "pic8259.h"

/* A union that can function as the parameter block for any UNDI API call.
 */
typedef t_PXENV_ANY pxenv_structure_t;

/* BIOS PnP parameter block.  We scan for this so that we can pass it
 * to the UNDI driver.
 */

#define PNP_BIOS_SIGNATURE ( ('$'<<0) + ('P'<<8) + ('n'<<16) + ('P'<<24) )
typedef struct pnp_bios {
	uint32_t	signature;
	uint8_t		version;
	uint8_t		length;
	uint16_t	control;
	uint8_t		checksum;
	uint8_t		dontcare[24];
} PACKED pnp_bios_t;

/* Structures within the PXE ROM.
 */

#define ROM_SIGNATURE 0xaa55
typedef struct rom {
	uint16_t	signature;
	uint8_t		unused[0x14];
	uint16_t	undi_rom_id_off;
	uint16_t	pcir_off;
	uint16_t	pnp_off;
} PACKED rom_t;	

#define PCIR_SIGNATURE ( ('P'<<0) + ('C'<<8) + ('I'<<16) + ('R'<<24) )
typedef struct pcir_header {
	uint32_t	signature;
	uint16_t	vendor_id;
	uint16_t	device_id;
} PACKED pcir_header_t;

#define PNP_SIGNATURE ( ('$'<<0) + ('P'<<8) + ('n'<<16) + ('P'<<24) )
typedef struct pnp_header {
	uint32_t	signature;
	uint8_t		struct_revision;
	uint8_t		length;
	uint16_t	next;
	uint8_t		reserved;
	uint8_t		checksum;
	uint16_t	id[2];
	uint16_t	manuf_str_off;
	uint16_t	product_str_off;
	uint8_t		base_type;
	uint8_t		sub_type;
	uint8_t		interface_type;
	uint8_t		indicator;
	uint16_t	boot_connect_off;
	uint16_t	disconnect_off;
	uint16_t	initialise_off;
	uint16_t	reserved2;
	uint16_t	info;
} PACKED pnp_header_t;

#define UNDI_SIGNATURE ( ('U'<<0) + ('N'<<8) + ('D'<<16) + ('I'<<24) )
typedef struct undi_rom_id {
	uint32_t	signature;
	uint8_t		struct_length;
	uint8_t		struct_cksum;
	uint8_t		struct_rev;
	uint8_t		undi_rev[3];
	uint16_t	undi_loader_off;
	uint16_t	stack_size;
	uint16_t	data_size;
	uint16_t	code_size;
} PACKED undi_rom_id_t;

/* Nontrivial IRQ handler structure */
typedef struct {
	segoff_t		chain_to;
	uint8_t			irq_chain, pad1, pad2, pad3;
	segoff_t		entry;
	uint16_t		count_all;
	uint16_t		count_ours;
	t_PXENV_UNDI_ISR	undi_isr;
	char			code[0];
} PACKED undi_irq_handler_t ;

/* Storage buffers that we need in base memory.  We collect these into
 * a single structure to make allocation simpler.
 */

typedef struct undi_base_mem_xmit_data {
	MAC_ADDR		destaddr;
	t_PXENV_UNDI_TBD	tbd;
} undi_base_mem_xmit_data_t;

typedef struct undi_base_mem_data {
	pxenv_structure_t	pxs;
	undi_base_mem_xmit_data_t xmit_data;
	char			xmit_buffer[ETH_FRAME_LEN];
	/* Must be last in structure and paragraph-aligned */
	union {
		char			e820mangler[0];
		char			irq_handler[0];
		undi_irq_handler_t	nontrivial_irq_handler;
	}  __attribute__ ((aligned(16)));
} undi_base_mem_data_t;

/* Macros and data structures used when freeing bits of base memory
 * used by the UNDI driver.
 */

#define FIRING_SQUAD_TARGET_SIZE 8
#define FIRING_SQUAD_TARGET_INDEX(x) ( (x) / FIRING_SQUAD_TARGET_SIZE )
#define FIRING_SQUAD_TARGET_BIT(x) ( (x) % FIRING_SQUAD_TARGET_SIZE )
typedef struct firing_squad_lineup {
	uint8_t targets[ 640 / FIRING_SQUAD_TARGET_SIZE ];
} firing_squad_lineup_t;
typedef enum firing_squad_shoot {
	DONTSHOOT = 0,
	SHOOT = 1
} firing_squad_shoot_t;

/* Driver private data structure.
 */

typedef struct undi {
	/* Pointers to various data structures */
	pnp_bios_t		*pnp_bios;
	rom_t			*rom;
	undi_rom_id_t		*undi_rom_id;
	pxe_t			*pxe;
	pxenv_structure_t	*pxs;
	undi_base_mem_xmit_data_t *xmit_data;
	/* Pointers and sizes to keep track of allocated base memory */
	undi_base_mem_data_t	*base_mem_data;
	void			*driver_code;
	size_t			driver_code_size;
	void			*driver_data;
	size_t			driver_data_size;
	char			*xmit_buffer;
	/* Flags.  We keep our own instead of trusting the UNDI driver
	 * to have implemented PXENV_UNDI_GET_STATE correctly.  Plus
	 * there's the small issue of PXENV_UNDI_GET_STATE being the
	 * same API call as PXENV_STOP_UNDI...
	 */
	uint8_t prestarted;	/* pxenv_start_undi() has been called */
	uint8_t started;	/* pxenv_undi_startup() has been called */
	uint8_t	initialized;	/* pxenv_undi_initialize() has been called */
	uint8_t opened;		/* pxenv_undi_open() has been called */
	/* Parameters that we need to store for future reference
	 */
	struct pci_device	pci;
	irq_t			irq;
} undi_t;

/* Constants
 */

#define HUNT_FOR_PIXIES 0
#define HUNT_FOR_UNDI_ROMS 1
