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
 * Read / write sectors
 *
 * @v drive		Emulated drive
 * @v al		Number of sectors to read or write (must be nonzero)
 * @v ch		Low bits of cylinder number
 * @v cl (bits 7:6)	High bits of cylinder number
 * @v cl (bits 5:0)	Sector number
 * @v dh		Head number
 * @v es:bx		Data buffer
 * @v io		Read / write method
 * @ret status		Status code
 * @ret al		Number of sectors read or written
 */
static int int13_rw_sectors ( struct int13_drive *drive,
			      struct i386_all_regs *ix86,
			      int ( * io ) ( struct block_device *blockdev,
					     uint64_t block,
					     unsigned long count,
					     userptr_t buffer ) ) {
	struct block_device *blockdev = drive->blockdev;
	unsigned int cylinder, head, sector;
	unsigned long lba;
	unsigned int count;
	userptr_t buffer;

	/* Calculate parameters */
	cylinder = ( ( ( ix86->regs.cl & 0xc0 ) << 2 ) | ix86->regs.ch );
	assert ( cylinder < drive->cylinders );
	head = ix86->regs.dh;
	assert ( head < drive->heads );
	sector = ( ix86->regs.cl & 0x3f );
	assert ( ( sector >= 1 ) && ( sector <= drive->sectors_per_track ) );
	lba = ( ( ( ( cylinder * drive->heads ) + head )
		  * drive->sectors_per_track ) + sector - 1 );
	count = ix86->regs.al;
	buffer = real_to_user ( ix86->segs.es, ix86->regs.bx );

	DBG ( "C/H/S %d/%d/%d = LBA %#lx <-> %04x:%04x (count %d)\n", cylinder,
	      head, sector, lba, ix86->segs.es, ix86->regs.bx, count );

	/* Validate blocksize */
	if ( blockdev->blksize != INT13_BLKSIZE ) {
		DBG ( "Invalid blocksize (%zd) for non-extended read/write\n",
		      blockdev->blksize );
		return -INT13_STATUS_INVALID;
	}
	
	/* Read from / write to block device */
	if ( io ( blockdev, lba, count, buffer ) != 0 )
		return -INT13_STATUS_READ_ERROR;

	return 0;
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
	DBG ( "Read: " );
	return int13_rw_sectors ( drive, ix86, drive->blockdev->read );
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
	DBG ( "Write: " );
	return int13_rw_sectors ( drive, ix86, drive->blockdev->write );
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
 * @ret status		Status code / API version
 */
static int int13_extension_check ( struct int13_drive *drive __unused,
				   struct i386_all_regs *ix86 ) {
	if ( ix86->regs.bx == 0x55aa ) {
		DBG ( "INT 13 extensions installation check\n" );
		ix86->regs.bx = 0xaa55;
		ix86->regs.cx = INT13_EXTENSION_LINEAR;
		return INT13_EXTENSION_VER_1_X;
	} else {
		return -INT13_STATUS_INVALID;
	}
}

/**
 * Extended read / write
 *
 * @v drive		Emulated drive
 * @v ds:si		Disk address packet
 * @v io		Read / write method
 * @ret status		Status code
 */
static int int13_extended_rw ( struct int13_drive *drive,
			       struct i386_all_regs *ix86,
			       int ( * io ) ( struct block_device *blockdev,
					      uint64_t block,
					      unsigned long count,
					      userptr_t buffer ) ) {
	struct block_device *blockdev = drive->blockdev;
	struct int13_disk_address addr;
	uint64_t lba;
	unsigned long count;
	userptr_t buffer;

	/* Read parameters from disk address structure */
	copy_from_real ( &addr, ix86->segs.ds, ix86->regs.si, sizeof ( addr ));
	lba = addr.lba;
	count = addr.count;
	buffer = real_to_user ( addr.buffer.segment, addr.buffer.offset );

	DBG ( "LBA %#llx <-> %04x:%04x (count %ld)\n", (unsigned long long)lba,
	      addr.buffer.segment, addr.buffer.offset, count );
	
	/* Read from / write to block device */
	if ( io ( blockdev, lba, count, buffer ) != 0 )
		return -INT13_STATUS_READ_ERROR;

	return 0;
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
	DBG ( "Extended read: " );
	return int13_extended_rw ( drive, ix86, drive->blockdev->read );
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
	DBG ( "Extended write: " );
	return int13_extended_rw ( drive, ix86, drive->blockdev->write );
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
	int command = ix86->regs.ah;
	struct int13_drive *drive;
	int status;

	list_for_each_entry ( drive, &drives, list ) {
		if ( drive->drive != ix86->regs.dl )
			continue;
		
		DBG ( "INT 13,%02x (%02x): ", command, drive->drive );

		switch ( command ) {
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
			status = -INT13_STATUS_INVALID;
			break;
		}

		/* Store status for INT 13,01 */
		drive->last_status = status;

		/* Negative status indicates an error */
		if ( status < 0 ) {
			ix86->flags |= CF;
			status = -status;
			DBG ( "INT13 failed with status %x\n", status );
		}
		ix86->regs.ah = status;

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
	__asm__  __volatile__ (
	       TEXT16_CODE ( "\nint13_wrapper:\n\t"
			     "orb $0, %%al\n\t" /* clear CF and OF */
			     "pushl %0\n\t" /* call int13() */
			     "pushw %%cs\n\t"
			     "call prot_call\n\t"
			     "jo 1f\n\t" /* chain if OF not set */
			     "pushfw\n\t"
			     "lcall *%%cs:int13_vector\n\t"
			     "\n1:\n\t"
			     "call 2f\n\t" /* return with flags intact */
			     "lret $2\n\t"
			     "\n2:\n\t"
			     "ret $4\n\t" ) : : "i" ( int13 ) );

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
	int discard_c, discard_d;

	DBG ( "Booting from INT 13 drive %02x\n", drive );

	/* Use INT 13 to read the boot sector */
	__asm__ __volatile__ ( REAL_CODE ( "pushw %%es\n\t"
					   "pushw $0\n\t"
					   "popw %%es\n\t"
					   "stc\n\t"
					   "int $0x13\n\t"
					   "sti\n\t" /* BIOS bugs */
					   "jc 1f\n\t"
					   "xorl %%eax, %%eax\n\t"
					   "\n1:\n\t"
					   "movzwl %%es:0x7dfe, %%ebx\n\t"
					   "popw %%es\n\t" )
			       : "=a" ( status ), "=b" ( signature ),
				 "=c" ( discard_c ), "=d" ( discard_d )
			       : "a" ( 0x0201 ), "b" ( 0x7c00 ),
				 "c" ( 1 ), "d" ( drive ) );
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
	__asm__ __volatile__ ( REAL_CODE ( /* Save segment registers */
					   "pushw %%ds\n\t"
					   "pushw %%es\n\t"
					   "pushw %%fs\n\t"
					   "pushw %%gs\n\t"
					   /* Save stack pointer */
					   "movw %%ss, %%ax\n\t"
					   "movw %%ax, %%cs:int13_saved_ss\n\t"
					   "movw %%sp, %%cs:int13_saved_sp\n\t"
					   "ljmp $0, $0x7c00\n\t"
					   "\nint13_saved_ss: .word 0\n\t"
					   "\nint13_saved_sp: .word 0\n\t"
					   "\nint13_exec_fail:\n\t"
					   "movw %%cs:int13_saved_ss, %%ax\n\t"
					   "movw %%ax, %%ss\n\t"
					   "movw %%cs:int13_saved_sp, %%sp\n\t"
					   "popw %%gs\n\t"
					   "popw %%fs\n\t"
					   "popw %%es\n\t"
					   "popw %%ds\n\t" )
			       : "=d" ( discard_d ) : "d" ( drive )
			       : "eax", "ebx", "ecx", "esi", "edi", "ebp" );

	DBG ( "Booted disk returned via INT 18 or 19\n" );

	/* Unhook INTs 18 and 19 */
	unhook_bios_interrupt ( 0x18, ( unsigned int ) int13_exec_fail,
				&int18_vector );
	unhook_bios_interrupt ( 0x19, ( unsigned int ) int13_exec_fail,
				&int19_vector );
	
	return -ECANCELED;
}
