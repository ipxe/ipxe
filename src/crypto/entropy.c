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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Entropy source
 *
 */

#include <string.h>
#include <ipxe/entropy.h>

/**
 * Obtain entropy input
 *
 * @v entropy_bits	Minimum amount of entropy, in bits
 * @v data		Data buffer
 * @v min_len		Minimum length of entropy input, in bytes
 * @v max_len		Maximum length of entropy input, in bytes
 * @ret len		Length of entropy input, in bytes
 */
int get_entropy_input ( unsigned int entropy_bits, void *data, size_t min_len,
			size_t max_len ) {

	/* Placeholder to allow remainder of RBG code to be tested */
	( void ) entropy_bits;
	( void ) min_len;
	memset ( data, 0x01, max_len );

	return max_len;
}
