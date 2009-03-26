/*
 * Copyright (C) 2009 Daniel Verkamp <daniel@drv.nu>.
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gpxe/command.h>
#include <gpxe/image.h>
#include <gpxe/crypto.h>

#include <gpxe/md5.h>
#include <gpxe/sha1.h>

/**
 * "digest" command syntax message
 *
 * @v argv		Argument list
 */
static void digest_syntax ( char **argv ) {
	printf ( "Usage:\n"
		 "  %s <image name>\n"
		 "\n"
		 "Calculate the %s of an image\n",
		 argv[0], argv[0] );
}

/**
 * The "digest" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v digest		Digest algorithm
 * @ret rc		Exit code
 */
static int digest_exec ( int argc, char **argv,
			 struct digest_algorithm *digest ) {
	const char *image_name;
	struct image *image;
	uint8_t digest_ctx[digest->ctxsize];
	uint8_t digest_out[digest->digestsize];
	uint8_t buf[128];
	size_t offset;
	size_t len;
	size_t frag_len;
	int i;
	unsigned j;

	if ( argc < 2 ||
	     !strcmp ( argv[1], "--help" ) ||
	     !strcmp ( argv[1], "-h" ) ) {
		digest_syntax ( argv );
		return 1;
	}

	for ( i = 1 ; i < argc ; i++ ) {
		image_name = argv[i];

		/* find image */
		image = find_image ( image_name );
		if ( ! image ) {
			printf ( "No such image: %s\n", image_name );
			continue;
		}
		offset = 0;
		len = image->len;

		/* calculate digest */
		digest_init ( digest, digest_ctx );
		while ( len ) {
			frag_len = len;
			if ( frag_len > sizeof ( buf ) )
				frag_len = sizeof ( buf );
			copy_from_user ( buf, image->data, offset, frag_len );
			digest_update ( digest, digest_ctx, buf, frag_len );
			len -= frag_len;
			offset += frag_len;
		}
		digest_final ( digest, digest_ctx, digest_out );

		for ( j = 0 ; j < sizeof ( digest_out ) ; j++ )
			printf ( "%02x", digest_out[j] );

		printf ( "  %s\n", image->name );
	}

	return 0;
}

static int md5sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &md5_algorithm );
}

static int sha1sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &sha1_algorithm );
}

struct command md5sum_command __command = {
	.name = "md5sum",
	.exec = md5sum_exec,
};

struct command sha1sum_command __command = {
	.name = "sha1sum",
	.exec = sha1sum_exec,
};
