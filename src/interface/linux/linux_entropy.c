/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Linux entropy source
 *
 */

#include <stdint.h>
#include <errno.h>
#include <ipxe/linux_api.h>
#include <ipxe/entropy.h>

struct entropy_source linux_entropy __entropy_source ( ENTROPY_NORMAL );

/** Entropy source filename */
static const char entropy_filename[] = "/dev/random";

/** Entropy source file handle */
static int entropy_fd;

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
static int linux_entropy_enable ( void ) {

	/* Open entropy source */
	entropy_fd = linux_open ( entropy_filename, O_RDONLY );
	if ( entropy_fd < 0 ) {
		DBGC ( &entropy_fd, "ENTROPY could not open %s: %s\n",
		       entropy_filename, linux_strerror ( linux_errno ) );
		return entropy_fd;
	}

	/* linux_get_noise() reads a single byte from /dev/random,
	 * which is supposed to block until a sufficient amount of
	 * entropy is available.  We therefore assume that each sample
	 * contains exactly 8 bits of entropy.
	 */
	entropy_init ( &linux_entropy, MIN_ENTROPY ( 8.0 ) );

	return 0;
}

/**
 * Disable entropy gathering
 *
 */
static void linux_entropy_disable ( void ) {

	/* Close entropy source */
	linux_close ( entropy_fd );
}

/**
 * Get noise sample
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 */
static int linux_get_noise ( noise_sample_t *noise ) {
	uint8_t byte;
	ssize_t len;

	/* Read a single byte from entropy source */
	len = linux_read ( entropy_fd, &byte, sizeof ( byte ) );
	if ( len < 0 ) {
		DBGC ( &entropy_fd, "ENTROPY could not read from %s: %s\n",
		       entropy_filename, linux_strerror ( linux_errno ) );
		return len;
	}
	if ( len == 0 ) {
		DBGC ( &entropy_fd, "ENTROPY EOF on reading from %s: %s\n",
		       entropy_filename, linux_strerror ( linux_errno ) );
		return -EPIPE;
	}
	*noise = byte;

	return 0;
}

/** Linux entropy source */
struct entropy_source linux_entropy __entropy_source ( ENTROPY_NORMAL ) = {
	.name = "linux",
	.enable = linux_entropy_enable,
	.disable = linux_entropy_disable,
	.get_noise = linux_get_noise,
};
