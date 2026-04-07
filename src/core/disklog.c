/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ipxe/disklog.h>

/** @file
 *
 * Disk log console
 *
 */

/**
 * Open disk log console
 *
 * @v disklog		Disk log
 * @ret rc		Return status code
 *
 * The data buffer must already contain the initial logical block.
 */
int disklog_open ( struct disklog *disklog ) {
	struct disklog_header *hdr =
		( ( struct disklog_header * ) disklog->buffer );

	/* Sanity checks */
	assert ( disklog->console != NULL );
	assert ( disklog->buffer != NULL );
	assert ( disklog->blksize > 0 );
	assert ( ( disklog->blksize & ( disklog->blksize - 1 ) ) == 0 );
	assert ( disklog->lba <= disklog->max_lba );
	assert ( disklog->op != NULL );
	assert ( disklog->op->write != NULL );

	/* Check magic signature */
	if ( ( disklog->blksize < sizeof ( *hdr ) ) ||
	     ( memcmp ( hdr->magic, DISKLOG_MAGIC,
			sizeof ( hdr->magic ) ) != 0 ) ) {
		DBGC ( disklog, "DISKLOG has bad magic signature\n" );
		return -EINVAL;
	}

	/* Initialise buffer */
	disklog->offset = sizeof ( *hdr );
	disklog->unwritten = 0;
	memset ( ( disklog->buffer + sizeof ( *hdr ) ), 0,
		 ( disklog->blksize - sizeof ( *hdr ) ) );

	/* Enable console */
	disklog->console->disabled = 0;

	return 0;
}

/**
 * Write character to disk log console
 *
 * @v disklog		Disk log
 * @v character		Character
 */
void disklog_putchar ( struct disklog *disklog, int character ) {
	static int busy;
	int rc;

	/* Ignore if we are already mid-logging */
	if ( busy )
		return;
	busy = 1;

	/* Sanity checks */
	assert ( disklog->offset < disklog->blksize );

	/* Write character to buffer */
	disklog->buffer[disklog->offset++] = character;
	disklog->unwritten++;

	/* Write sector to disk, if applicable */
	if ( ( disklog->offset == disklog->blksize ) ||
	     ( disklog->unwritten == DISKLOG_MAX_UNWRITTEN ) ||
	     ( character == '\n' ) ) {

		/* Write sector to disk */
		if ( ( rc = disklog->op->write() ) != 0 ) {
			DBGC ( disklog, "DISKLOG could not write: %s\n",
			       strerror ( rc ) );
			/* Ignore and continue; there's nothing we can do */
		}

		/* Reset count of unwritten characters */
		disklog->unwritten = 0;
	}

	/* Move to next sector, if applicable */
	if ( disklog->offset == disklog->blksize ) {

		/* Disable console if we have run out of space */
		if ( disklog->lba >= disklog->max_lba )
			disklog->console->disabled = 1;

		/* Clear log buffer */
		memset ( disklog->buffer, 0, disklog->blksize );
		disklog->offset = 0;

		/* Move to next sector */
		disklog->lba++;
	}

	/* Clear busy flag */
	busy = 0;
}
