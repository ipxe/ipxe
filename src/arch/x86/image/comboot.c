/*
 * Copyright (C) 2008 Daniel Verkamp <daniel@drv.nu>.
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
 */

/**
 * @file
 *
 * SYSLINUX COMBOOT (16-bit) image format
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <realmode.h>
#include <basemem.h>
#include <comboot.h>
#include <ipxe/image.h>
#include <ipxe/segment.h>
#include <ipxe/init.h>
#include <ipxe/features.h>
#include <ipxe/console.h>

FEATURE ( FEATURE_IMAGE, "COMBOOT", DHCP_EB_FEATURE_COMBOOT, 1 );

/**
 * COMBOOT PSP, copied to offset 0 of code segment
 */
struct comboot_psp {
	/** INT 20 instruction, executed if COMBOOT image returns with RET */
	uint16_t int20;
	/** Segment of first non-free paragraph of memory */
	uint16_t first_non_free_para;
};

/** Offset in PSP of command line */
#define COMBOOT_PSP_CMDLINE_OFFSET 0x81

/** Maximum length of command line in PSP
 * (127 bytes minus space and CR) */
#define COMBOOT_MAX_CMDLINE_LEN    125


/**
 * Copy command line to PSP
 *
 * @v image		COMBOOT image
 */
static void comboot_copy_cmdline ( struct image * image, void *seg ) {
	const char *cmdline = ( image->cmdline ? image->cmdline : "" );
	int cmdline_len = strlen ( cmdline );
	uint8_t *psp_cmdline;

	/* Limit length of command line */
	if( cmdline_len > COMBOOT_MAX_CMDLINE_LEN )
		cmdline_len = COMBOOT_MAX_CMDLINE_LEN;

	/* Copy length to byte before command line */
	psp_cmdline = ( seg + COMBOOT_PSP_CMDLINE_OFFSET );
	psp_cmdline[-1] = cmdline_len;

	/* Command line starts with space */
	psp_cmdline[0] = ' ';

	/* Copy command line */
	memcpy ( &psp_cmdline[1], cmdline, cmdline_len );

	/* Command line ends with CR */
	psp_cmdline[ 1 + cmdline_len ] = '\r';
}

/**
 * Initialize PSP
 *
 * @v image		COMBOOT image
 * @v seg		segment to initialize
 */
static void comboot_init_psp ( struct image * image, void *seg ) {
	struct comboot_psp *psp;

	/* Fill PSP */
	psp = seg;

	/* INT 20h instruction, byte order reversed */
	psp->int20 = 0x20CD;

	/* get_fbms() returns BIOS free base memory counter, which is in
	 * kilobytes; x * 1024 / 16 == x * 64 == x << 6 */
	psp->first_non_free_para = get_fbms() << 6;

	DBGC ( image, "COMBOOT %s: first non-free paragraph = 0x%x\n",
	       image->name, psp->first_non_free_para );

	/* Copy the command line to the PSP */
	comboot_copy_cmdline ( image, seg );
}

/**
 * Execute COMBOOT image
 *
 * @v image		COMBOOT image
 * @ret rc		Return status code
 */
static int comboot_exec_loop ( struct image *image ) {
	void *seg = real_to_virt ( COMBOOT_PSP_SEG, 0 );
	int state;

	state = rmsetjmp ( comboot_return );

	switch ( state ) {
	case 0: /* First time through; invoke COMBOOT program */

		/* Initialize PSP */
		comboot_init_psp ( image, seg );

		/* Hook COMBOOT API interrupts */
		hook_comboot_interrupts();

		DBGC ( image, "executing 16-bit COMBOOT image at %4x:0100\n",
		       COMBOOT_PSP_SEG );

		/* Unregister image, so that a "boot" command doesn't
		 * throw us into an execution loop.  We never
		 * reregister ourselves; COMBOOT images expect to be
		 * removed on exit.
		 */
		unregister_image ( image );

		/* Store stack segment at 0x38 and stack pointer at 0x3A
		 * in the PSP and jump to the image */
		__asm__ __volatile__ (
		    REAL_CODE ( /* Save return address with segment on old stack */
				    "popw %%ax\n\t"
				    "pushw %%cs\n\t"
				    "pushw %%ax\n\t"
				    /* Set DS=ES=segment with image */
				    "movw %w0, %%ds\n\t"
				    "movw %w0, %%es\n\t"
				    /* Set SS:SP to new stack (end of image segment) */
				    "movw %w0, %%ss\n\t"
				    "xor %%sp, %%sp\n\t"
				    "pushw $0\n\t"
				    "pushw %w0\n\t"
				    "pushw $0x100\n\t"
				    /* Zero registers (some COM files assume GP regs are 0) */
				    "xorw %%ax, %%ax\n\t"
				    "xorw %%bx, %%bx\n\t"
				    "xorw %%cx, %%cx\n\t"
				    "xorw %%dx, %%dx\n\t"
				    "xorw %%si, %%si\n\t"
				    "xorw %%di, %%di\n\t"
				    "xorw %%bp, %%bp\n\t"
				    "lret\n\t" )
					 : : "R" ( COMBOOT_PSP_SEG ) : "eax" );
		DBGC ( image, "COMBOOT %s: returned\n", image->name );
		break;

	case COMBOOT_EXIT:
		DBGC ( image, "COMBOOT %s: exited\n", image->name );
		break;

	case COMBOOT_EXIT_RUN_KERNEL:
		assert ( image->replacement );
		DBGC ( image, "COMBOOT %s: exited to run kernel %s\n",
		       image->name, image->replacement->name );
		break;

	case COMBOOT_EXIT_COMMAND:
		DBGC ( image, "COMBOOT %s: exited after executing command\n",
		       image->name );
		break;

	default:
		assert ( 0 );
		break;
	}

	unhook_comboot_interrupts();
	comboot_force_text_mode();

	return 0;
}

/**
 * Check image name extension
 *
 * @v image		COMBOOT image
 * @ret rc		Return status code
 */
static int comboot_identify ( struct image *image ) {
	const char *ext;

	ext = strrchr( image->name, '.' );

	if ( ! ext ) {
		DBGC ( image, "COMBOOT %s: no extension\n",
		       image->name );
		return -ENOEXEC;
	}

	++ext;

	if ( strcasecmp( ext, "cbt" ) ) {
		DBGC ( image, "COMBOOT %s: unrecognized extension %s\n",
		       image->name, ext );
		return -ENOEXEC;
	}

	return 0;
}

/**
 * Load COMBOOT image into memory, preparing a segment and returning it
 * @v image		COMBOOT image
 * @ret rc		Return status code
 */
static int comboot_prepare_segment ( struct image *image )
{
	void *seg;
	size_t filesz, memsz;
	int rc;

	/* Load image in segment */
	seg = real_to_virt ( COMBOOT_PSP_SEG, 0 );

	/* Allow etra 0x100 bytes before image for PSP */
	filesz = image->len + 0x100;

	/* Ensure the entire 64k segment is free */
	memsz = 0xFFFF;

	/* Prepare, verify, and load the real-mode segment */
	if ( ( rc = prep_segment ( seg, filesz, memsz ) ) != 0 ) {
		DBGC ( image, "COMBOOT %s: could not prepare segment: %s\n",
		       image->name, strerror ( rc ) );
		return rc;
	}

	/* Zero PSP */
	memset ( seg, 0, 0x100 );

	/* Copy image to segment:0100 */
	memcpy ( ( seg + 0x100 ), image->data, image->len );

	return 0;
}

/**
 * Probe COMBOOT image
 *
 * @v image		COMBOOT image
 * @ret rc		Return status code
 */
static int comboot_probe ( struct image *image ) {
	int rc;

	/* Check if this is a COMBOOT image */
	if ( ( rc = comboot_identify ( image ) ) != 0 ) {

		return rc;
	}

	return 0;
}

/**
 * Execute COMBOOT image
 *
 * @v image		COMBOOT image
 * @ret rc		Return status code
 */
static int comboot_exec ( struct image *image ) {
	int rc;

	/* Sanity check for filesize */
	if( image->len >= 0xFF00 ) {
		DBGC( image, "COMBOOT %s: image too large\n",
		      image->name );
		return -ENOEXEC;
	}

	/* Prepare segment and load image */
	if ( ( rc = comboot_prepare_segment ( image ) ) != 0 ) {
		return rc;
	}

	/* Reset console */
	console_reset();

	return comboot_exec_loop ( image );
}

/** SYSLINUX COMBOOT (16-bit) image type */
struct image_type comboot_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "COMBOOT",
	.probe = comboot_probe,
	.exec = comboot_exec,
};
