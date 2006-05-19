/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <limits.h>
#include <byteswap.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/list.h>
#include <gpxe/blockdev.h>
#include <realmode.h>
#include <bios.h>
#include <biosint.h>
#include <int13.h>

/** @file
 *
 * INT 13 emulation
 *
 * This module provides a mechanism for exporting block devices via
 * the BIOS INT 13 disk interrupt interface.  
 *
 */

/** Vector for chaining to other INT 13 handlers */
static struct segoff __text16 ( int13_vector );
#define int13_vector __use_text16 ( int13_vector )

/** Assembly wrapper */
extern void int13_wrapper ( void );

/** Vector for storing original INT 18 handler
 *
 * We do not chain to this vector, so there is no need to place it in
 * .text16.
 */
static struct segoff int18_vector;

/** Vector for storing original INT 19 handler
 *
 * We do not chain to this vector, so there is no need to place it in
 * .text16.
 */
static struct segoff int19_vector;

/** Restart point for INT 18 or 19 */
extern void int13_exec_fail ( void );

/** List of registered emulated drives */
static LIST_HEAD ( drives );

/**
 * Convert CHS address to linear address
 *
 * @v drive		Emulated drive
 * @v ch		Low bits of cylinder number
 * @v cl (bits 7:6)	High bits of cylinder number
 * @v cl (bits 5:0)	Sector number
 * @v dh		Head number
 * @ret lba		LBA address
 * 
 */
static unsigned long chs_to_lba ( struct int13_drive *drive,
				  struct i386_all_regs *ix86 ) {
	unsigned int cylinder;
	unsigned int head;
	unsigned int sector;
	unsigned long lba;

	cylinder = ( ( ( ix86->regs.cl & 0xc0 ) << 8 ) | ix86->regs.ch );
	head = ix86->regs.dh;
	sector = ( ix86->regs.cl & 0x3f );

	assert ( cylinder < drive->cylinders );
	assert ( head < drive->heads );
	assert ( ( sector >= 1 ) && ( sector <= drive->sectors_per_track ) );

	lba = ( ( ( ( cylinder * drive->heads ) + head )
		  * drive->sectors_per_track ) + sector - 1 );

	DBG ( "C/H/S address %x/%x/%x -> LBA %lx\n",
	      cylinder, head, sector, lba );

	return lba;
}

/**
 * Read from drive to real-mode data buffer
 *
 * @v drive		Emulated drive
 * @v lba		LBA starting sector number
 * @v data		Data buffer
 * @v count		Number of sectors to read
 * @ret status		Status code
 */
static int int13_read ( struct int13_drive *drive, uint64_t lba,
			struct segoff data, unsigned long count ) {
	struct block_device *blockdev = drive->blockdev;
	userptr_t buffer = real_to_user ( data.segment, data.offset );
	int rc;

	DBG ( "Read %lx sectors from %llx to %04x:%04x\n", count,
	      ( unsigned long long ) lba, data.segment, data.offset );
	
	if ( ( rc = blockdev->read ( blockdev, lba, count, buffer ) ) != 0 )
		return INT13_STATUS_READ_ERROR;

	return 0;
}

/**
 * Write from real-mode data buffer to drive
 *
 * @v drive		Emulated drive
 * @v lba		LBA starting sector number
 * @v data		Data buffer
 * @v count		Number of sectors to read
 * @ret status		Status code
 */
static int int13_write ( struct int13_drive *drive, uint64_t lba,
			 struct segoff data, unsigned long count ) {
	struct block_device *blockdev = drive->blockdev;
	userptr_t buffer = real_to_user ( data.segment, data.offset );
	int rc;

	DBG ( "Write %lx sectors from %04x:%04x to %llx\n", count,
	      data.segment, data.offset, ( unsigned long long ) lba );

	if ( ( rc = blockdev->write ( blockdev, lba, count, buffer ) ) != 0 )
		return INT13_STATUS_WRITE_ERROR;

	return 0;
}

/**
 * INT 13, 00 - Reset disk system
 *
 * @v drive		Emulated drive
 * @ret status		Status code
 */
static int int13_reset ( struct int13_drive *drive __unused,
			 struct i386_all_regs *ix86 __unused ) {
	DBG ( "Reset drive\n" );
	return 0;
}

/**
 * INT 13, 01 - Get status of last operation
 *
 * @v drive		Emulated drive
 * @ret status		Status code
 */
static int int13_get_last_status ( struct int13_drive *drive,
				   struct i386_all_regs *ix86 __unused ) {
	DBG ( "Get status of last operation\n" );
	return drive->last_status;
}

/**
 * INT 13, 02 - Read sectors
 *
 * @v drive		Emulated drive
 * @v al		Number of sectors to read (must be nonzero)
 * @v ch		Low bits of cylinder number
 * @v cl (bits 7:6)	High bits of cylinder number
 * @v cl (bits 5:0)	Sector number
 * @v dh		Head number
 * @v es:bx		Data buffer
 * @ret status		Status code
 * @ret al		Number of sectors read
 */
static int int13_read_sectors ( struct int13_drive *drive,
				struct i386_all_regs *ix86 ) {
	unsigned long lba = chs_to_lba ( drive, ix86 );
	unsigned int count = ix86->regs.al;
	struct segoff data = {
		.segment = ix86->segs.es,
		.offset = ix86->regs.bx,
	};

	if ( drive->blockdev->blksize != INT13_BLKSIZE ) {
		DBG ( "Invalid blocksize (%zd) for non-extended read\n",
		      drive->blockdev->blksize );
		return INT13_STATUS_INVALID;
	}

	return int13_read ( drive, lba, data, count );
}

/**
 * INT 13, 03 - Write sectors
 *
 * @v drive		Emulated drive
 * @v al		Number of sectors to write (must be nonzero)
 * @v ch		Low bits of cylinder number
 * @v cl (bits 7:6)	High bits of cylinder number
 * @v cl (bits 5:0)	Sector number
 * @v dh		Head number
 * @v es:bx		Data buffer
 * @ret status		Status code
 * @ret al		Number of sectors written
 */
static int int13_write_sectors ( struct int13_drive *drive,
				 struct i386_all_regs *ix86 ) {
	unsigned long lba = chs_to_lba ( drive, ix86 );
	unsigned int count = ix86->regs.al;
	struct segoff data = {
		.segment = ix86->segs.es,
		.offset = ix86->regs.bx,
	};

	if ( drive->blockdev->blksize != INT13_BLKSIZE ) {
		DBG ( "Invalid blocksize (%zd) for non-extended write\n",
		      drive->blockdev->blksize );
		return INT13_STATUS_INVALID;
	}

	return int13_write ( drive, lba, data, count );
}

/**
 * INT 13, 08 - Get drive parameters
 *
 * @v drive		Emulated drive
 * @ret status		Status code
 * @ret ch		Low bits of maximum cylinder number
 * @ret cl (bits 7:6)	High bits of maximum cylinder number
 * @ret cl (bits 5:0)	Maximum sector number
 * @ret dh		Maximum head number
 * @ret dl		Number of drives
 */
static int int13_get_parameters ( struct int13_drive *drive,
				  struct i386_all_regs *ix86 ) {
	unsigned int max_cylinder = drive->cylinders - 1;
	unsigned int max_head = drive->heads - 1;
	unsigned int max_sector = drive->sectors_per_track; /* sic */

	DBG ( "Get drive parameters\n" );

	ix86->regs.ch = ( max_cylinder & 0xff );
	ix86->regs.cl = ( ( ( max_cylinder >> 8 ) << 6 ) | max_sector );
	ix86->regs.dh = max_head;
	get_real ( ix86->regs.dl, BDA_SEG, BDA_NUM_DRIVES );
	return 0;
}

/**
 * INT 13, 41 - Extensions installation check
 *
 * @v drive		Emulated drive
 * @v bx		0x55aa
 * @ret bx		0xaa55
 * @ret cx		Extensions API support bitmap
 * @ret status		Status code
 */
static int int13_extension_check ( struct int13_drive *drive __unused,
				   struct i386_all_regs *ix86 ) {
	if ( ix86->regs.bx == 0x55aa ) {
		DBG ( "INT 13 extensions installation check\n" );
		ix86->regs.bx = 0xaa55;
		ix86->regs.cx = INT13_EXTENSION_LINEAR;
		return 0;
	} else {
		return INT13_STATUS_INVALID;
	}
}

/**
 * INT 13, 42 - Extended read
 *
 * @v drive		Emulated drive
 * @v ds:si		Disk address packet
 * @ret status		Status code
 */
static int int13_extended_read ( struct int13_drive *drive,
				 struct i386_all_regs *ix86 ) {
	struct int13_disk_address addr;

	copy_from_real ( &addr, ix86->segs.ds, ix86->regs.si,
			 sizeof ( addr ) );
	return int13_read ( drive, addr.lba, addr.buffer, addr.count );
}

/**
 * INT 13, 43 - Extended write
 *
 * @v drive		Emulated drive
 * @v ds:si		Disk address packet
 * @ret status		Status code
 */
static int int13_extended_write ( struct int13_drive *drive,
				  struct i386_all_regs *ix86 ) {
	struct int13_disk_address addr;

	copy_from_real ( &addr, ix86->segs.ds, ix86->regs.si,
			 sizeof ( addr ) );
	return int13_write ( drive, addr.lba, addr.buffer, addr.count );
}

/**
 * INT 13, 48 - Get extended parameters
 *
 * @v drive		Emulated drive
 * @v ds:si		Drive parameter table
 * @ret status		Status code
 */
static int int13_get_extended_parameters ( struct int13_drive *drive,
					   struct i386_all_regs *ix86 ) {
	struct int13_disk_parameters params = {
		.bufsize = sizeof ( params ),
		.flags = INT13_FL_DMA_TRANSPARENT,
		.cylinders = drive->cylinders,
		.heads = drive->heads,
		.sectors_per_track = drive->sectors_per_track,
		.sectors = drive->blockdev->blocks,
		.sector_size = drive->blockdev->blksize,
	};
	
	DBG ( "Get extended drive parameters to %04x:%04x\n",
	      ix86->segs.ds, ix86->regs.si );

	copy_to_real ( ix86->segs.ds, ix86->regs.si, &params,
		       sizeof ( params ) );
	return 0;
}

/**
 * INT 13 handler
 *
 */
static void int13 ( struct i386_all_regs *ix86 ) {
	struct int13_drive *drive;
	int status;

	list_for_each_entry ( drive, &drives, list ) {
		if ( drive->drive != ix86->regs.dl )
			continue;
		
		DBG ( "INT 13, %02x on drive %02x\n", ix86->regs.ah,
		      ix86->regs.dl );
	
		switch ( ix86->regs.ah ) {
		case INT13_RESET:
			status = int13_reset ( drive, ix86 );
			break;
		case INT13_GET_LAST_STATUS:
			status = int13_get_last_status ( drive, ix86 );
			break;
		case INT13_READ_SECTORS:
			status = int13_read_sectors ( drive, ix86 );
			break;
		case INT13_WRITE_SECTORS:
			status = int13_write_sectors ( drive, ix86 );
			break;
		case INT13_GET_PARAMETERS:
			status = int13_get_parameters ( drive, ix86 );
			break;
		case INT13_EXTENSION_CHECK:
			status = int13_extension_check ( drive, ix86 );
			break;
		case INT13_EXTENDED_READ:
			status = int13_extended_read ( drive, ix86 );
			break;
		case INT13_EXTENDED_WRITE:
			status = int13_extended_write ( drive, ix86 );
			break;
		case INT13_GET_EXTENDED_PARAMETERS:
			status = int13_get_extended_parameters ( drive, ix86 );
			break;
		default:
			DBG ( "Unrecognised INT 13\n" );
			status = INT13_STATUS_INVALID;
			break;
		}

		/* Store status for INT 13,01 */
		drive->last_status = status;
		/* All functions return status via %ah and CF */
		ix86->regs.ah = status;
		if ( status ) {
			ix86->flags |= CF;
			DBG ( "INT13 failed with status %x\n", status );
		}
		/* Set OF to indicate to wrapper not to chain this call */
		ix86->flags |= OF;
	}
}

/**
 * Hook INT 13 handler
 *
 */
static void hook_int13 ( void ) {
	/* Assembly wrapper to call int13().  int13() sets OF if we
	 * should not chain to the previous handler.  (The wrapper
	 * clears CF and OF before calling int13()).
	 */
	__asm__  __volatile__ ( ".section \".text16\", \"ax\", @progbits\n\t"
				".code16\n\t"
				"\nint13_wrapper:\n\t"
				"orb $0, %%al\n\t" /* clear CF and OF */
				"pushl %0\n\t" /* call int13() */
				"data32 call prot_call\n\t"
				"jo 1f\n\t" /* chain if OF not set */
				"pushfw\n\t"
				"lcall *%%cs:int13_vector\n\t"
				"\n1:\n\t"
				"call 2f\n\t" /* return with flags intact */
				"lret $2\n\t"
				"\n2:\n\t"
				"ret $4\n\t"
				".previous\n\t"
				".code32\n\t" : :
				"i" ( int13 ) );

	hook_bios_interrupt ( 0x13, ( unsigned int ) int13_wrapper,
			      &int13_vector );
}

/**
 * Unhook INT 13 handler
 */
static void unhook_int13 ( void ) {
	unhook_bios_interrupt ( 0x13, ( unsigned int ) int13_wrapper,
				&int13_vector );
}

/**
 * Register INT 13 emulated drive
 *
 * @v drive		Emulated drive
 *
 * Registers the drive with the INT 13 emulation subsystem, and hooks
 * the INT 13 interrupt vector (if not already hooked).
 *
 * The underlying block device must be valid.  A drive number and
 * geometry will be assigned if left blank.
 */
void register_int13_drive ( struct int13_drive *drive ) {
	uint8_t num_drives;
	unsigned long blocks;
	unsigned long blocks_per_cyl;

	/* Give drive a default geometry if none specified */
	if ( ! drive->heads )
		drive->heads = 255;
	if ( ! drive->sectors_per_track )
		drive->sectors_per_track = 63;
	if ( ! drive->cylinders ) {
		/* Avoid attempting a 64-bit divide on a 32-bit system */
		blocks = ( ( drive->blockdev->blocks <= ULONG_MAX ) ?
			   drive->blockdev->blocks : ULONG_MAX );
		blocks_per_cyl = ( drive->heads * drive->sectors_per_track );
		assert ( blocks_per_cyl != 0 );
		drive->cylinders = ( blocks / blocks_per_cyl );
	}

	/* Assign drive number if none specified, update BIOS drive count */
	get_real ( num_drives, BDA_SEG, BDA_NUM_DRIVES );
	if ( ! drive->drive )
		drive->drive = ( num_drives | 0x80 );
	if ( num_drives <= ( drive->drive & 0x7f ) )
		num_drives = ( ( drive->drive & 0x7f ) + 1 );
	put_real ( num_drives, BDA_SEG, BDA_NUM_DRIVES );

	DBG ( "Registered INT13 drive %02x with C/H/S geometry %d/%d/%d\n",
	      drive->drive, drive->cylinders, drive->heads,
	      drive->sectors_per_track );

	/* Hook INT 13 vector if not already hooked */
	if ( list_empty ( &drives ) )
		hook_int13();

	/* Add to list of emulated drives */
	list_add ( &drive->list, &drives );
}

/**
 * Unregister INT 13 emulated drive
 *
 * @v drive		Emulated drive
 *
 * Unregisters the drive from the INT 13 emulation subsystem.  If this
 * is the last emulated drive, the INT 13 vector is unhooked (if
 * possible).
 */
void unregister_int13_drive ( struct int13_drive *drive ) {
	/* Remove from list of emulated drives */
	list_del ( &drive->list );

	DBG ( "Unregistered INT13 drive %02x\n", drive->drive );

	/* Unhook INT 13 vector if no more drives */
	if ( list_empty ( &drives ) )
		unhook_int13();
}

/**
 * Attempt to boot from an INT 13 drive
 *
 * @v drive		Drive number
 * @ret rc		Return status code
 *
 * This boots from the specified INT 13 drive by loading the Master
 * Boot Record to 0000:7c00 and jumping to it.  INT 18 is hooked to
 * capture an attempt by the MBR to boot the next device.  (This is
 * the closest thing to a return path from an MBR).
 *
 * Note that this function can never return success, by definition.
 */
int int13_boot ( unsigned int drive ) {
	int status, signature;
	int d0, d1;

	DBG ( "Booting from INT 13 drive %02x\n", drive );

	/* Use INT 13 to read the boot sector */
	REAL_EXEC ( rm_int13_boot,
		    "pushw $0\n\t"
		    "popw %%es\n\t"
		    "int $0x13\n\t"
		    "jc 1f\n\t"
		    "xorl %%eax, %%eax\n\t"
		    "\n1:\n\t"
		    "movzwl %%es:0x7dfe, %%ebx\n\t",
		    4,
		    OUT_CONSTRAINTS ( "=a" ( status ), "=b" ( signature ),
				      "=c" ( d0 ), "=d" ( drive ) ),
		    IN_CONSTRAINTS ( "0" ( 0x0201 ), "1" ( 0x7c00 ),
				     "2" ( 0x0001 ), "3" ( drive ) ),
		    CLOBBER ( "ebp" ) );
	if ( status )
		return -EIO;

	/* Check signature is correct */
	if ( signature != be16_to_cpu ( 0x55aa ) ) {
		DBG ( "Invalid disk signature %#04x (should be 0x55aa)\n",
		      cpu_to_be16 ( signature ) );
		return -ENOEXEC;
	}

	/* Hook INTs 18 and 19 to capture failure paths */
	hook_bios_interrupt ( 0x18, ( unsigned int ) int13_exec_fail,
			      &int18_vector );
	hook_bios_interrupt ( 0x19, ( unsigned int ) int13_exec_fail,
			      &int19_vector );

	/* Boot the loaded sector */
	REAL_EXEC ( rm_int13_exec,
		    "movw %%ss, %%ax\n\t" /* Preserve stack pointer */
		    "movw %%ax, %%cs:int13_exec_saved_ss\n\t"
		    "movw %%sp, %%cs:int13_exec_saved_sp\n\t"
		    "ljmp $0, $0x7c00\n\t"
		    "\nint13_exec_saved_ss: .word 0\n\t"
		    "\nint13_exec_saved_sp: .word 0\n\t"
		    "\nint13_exec_fail:\n\t"
		    "movw %%cs:int13_exec_saved_ss, %%ax\n\t"
		    "movw %%ax, %%ss\n\t"
		    "movw %%cs:int13_exec_saved_sp, %%sp\n\t"
		    "\n99:\n\t",
		    1,
		    OUT_CONSTRAINTS ( "=d" ( d1 ) ),
		    IN_CONSTRAINTS ( "0" ( drive ) ),
		    CLOBBER ( "eax", "ebx", "ecx", "esi", "edi", "ebp" ) );

	DBG ( "Booted disk returned via INT 18 or 19\n" );

	/* Unhook INTs 18 and 19 */
	unhook_bios_interrupt ( 0x18, ( unsigned int ) int13_exec_fail,
				&int18_vector );
	unhook_bios_interrupt ( 0x19, ( unsigned int ) int13_exec_fail,
				&int19_vector );
	
	return -ECANCELED;
}
