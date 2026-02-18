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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );
FILE_SECBOOT ( PERMITTED );

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/image.h>
#include <ipxe/settings.h>
#include <ipxe/crypto.h>
#include <ipxe/md5.h>
#include <ipxe/sha1.h>
#include <usr/imgmgmt.h>
#include <hci/digest_cmd.h>

/** @file
 *
 * Digest commands
 *
 */

/** "digest" options */
struct digest_options {
	/** Setting */
	struct named_setting setting;
};

/** "digest" option list */
static struct option_descriptor digest_opts[] = {
	OPTION_DESC ( "set", 's', required_argument, struct digest_options,
		      setting, parse_autovivified_setting ),
};

/** "digest" command descriptor */
static struct command_descriptor digest_cmd =
	COMMAND_DESC ( struct digest_options, digest_opts, 1, MAX_ARGUMENTS,
		       "<image> [<image>...]" );

/**
 * The "digest" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v digest		Digest algorithm
 * @ret rc		Return status code
 */
int digest_exec ( int argc, char **argv, struct digest_algorithm *digest ) {
	struct digest_options opts;
	struct image *image;
	uint8_t ctx[digest->ctxsize];
	uint8_t out[digest->digestsize];
	unsigned int j;
	int i;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &digest_cmd, &opts ) ) != 0 )
		return rc;

	/* Use default setting type, if not specified */
	if ( ! opts.setting.setting.type )
		opts.setting.setting.type = &setting_type_hexraw;

	/* Calculate digests for each image */
	for ( i = optind ; i < argc ; i++ ) {

		/* Acquire image */
		if ( ( rc = imgacquire ( argv[i], 0, &image ) ) != 0 )
			return rc;

		/* Calculate digest */
		digest_init ( digest, ctx );
		digest_update ( digest, ctx, image->data, image->len );
		digest_final ( digest, ctx, out );

		/* Display or store digest as directed */
		if ( opts.setting.settings ) {

			/* Store digest */
			if ( ( rc = store_setting ( opts.setting.settings,
						    &opts.setting.setting, out,
						    sizeof ( out ) ) ) != 0 ) {
				printf ( "Could not store \"%s\": %s\n",
					 opts.setting.setting.name,
					 strerror ( rc ) );
				return rc;
			}

		} else {

			/* Print digest */
			for ( j = 0 ; j < sizeof ( out ) ; j++ )
				printf ( "%02x", out[j] );
			printf ( "  %s\n", image->name );
		}
	}

	return 0;
}

/* Include "md5sum" and "sha1sum" commands unconditionally */

static int md5sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &md5_algorithm );
}

static int sha1sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &sha1_algorithm );
}

COMMAND ( md5sum, md5sum_exec );
COMMAND ( sha1sum, sha1sum_exec );

/* Drag in commands for any other enabled algorithms */
REQUIRING_SYMBOL ( digest_exec );
REQUIRE_OBJECT ( config_digest_cmd );
