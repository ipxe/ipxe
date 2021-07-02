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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/image.h>
#include <ipxe/crypto.h>
#include <ipxe/md5.h>
#include <ipxe/sha1.h>
#include <ipxe/sha256.h>
#include <ipxe/sha512.h>
#include <ipxe/settings.h>
#include <usr/imgmgmt.h>

/** @file
 *
 * Digest commands
 *
 */

/** "digest" options */
struct digest_options { 
	/** String to digest */
	char *str;
	/** Rounds to rehash */
	unsigned int rounds;
};

/** "digest" option list */
static struct option_descriptor digest_opts[] = {
	OPTION_DESC ( "rounds", 'r', required_argument,
		struct digest_options, rounds, parse_integer),
	OPTION_DESC ( "str", 's', required_argument,
		struct digest_options, str, parse_string ),
};

/** "digest" command descriptor */
static struct command_descriptor digest_cmd =
	COMMAND_DESC ( struct digest_options, digest_opts, 0, MAX_ARGUMENTS,
		       "[<image>] [<image>...]" );

/**
 * The "digest" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v digest		Digest algorithm
 * @ret rc		Return status code
 */
static int digest_exec ( int argc, char **argv,
			 struct digest_algorithm *digest ) {
	struct digest_options opts;
	struct image *image;
	struct named_setting setting;
	uint8_t digest_ctx[digest->ctxsize];
	uint8_t digest_out[digest->digestsize];
	uint8_t buf[128];
	size_t offset;
	size_t len;
	size_t frag_len;
	unsigned long origlen;
	int i;
	unsigned j, r;
	int rc;
	char hashstr[130];

	if ( argc < 2 ) {
		print_usage ( &digest_cmd, argv );
		return 0;
	}

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &digest_cmd, &opts ) ) != 0 )
		return rc;

	for ( i = optind ; i < argc || opts.str ; i++ ) {

		hashstr[0] = '\0';

		/* Acquire image */
		if ( ( ! opts.str ) && 
			( ( rc = imgacquire ( argv[i], 0, &image ) ) != 0 ) )
			continue;

		/* calculate digest */
		digest_init ( digest, digest_ctx );
		if ( opts.str ) {
			origlen = strlen( opts.str );
			digest_update ( digest, digest_ctx, opts.str,
				origlen );
		} else {
			offset = 0;
			len = image->len;
			origlen = image->len;
			while ( len ) {
				frag_len = len;
				if ( frag_len > sizeof ( buf ) )
					frag_len = sizeof ( buf );
				copy_from_user ( buf, image->data, offset, frag_len );
				digest_update ( digest, digest_ctx, buf, frag_len );
				len -= frag_len;
				offset += frag_len;
			}
		}
		digest_final ( digest, digest_ctx, digest_out );

		for ( r = 1 ; r < opts.rounds ; r++ ) {
			digest_init ( digest, digest_ctx );
			digest_update ( digest, digest_ctx, digest_out,
				sizeof ( digest_out ) );
			digest_final ( digest, digest_ctx, digest_out );
		}

		if ( sizeof( hashstr ) >= sizeof ( digest_out ) )
			for ( j = 0 ; j < sizeof ( digest_out ) ; j++ )
				sprintf ( hashstr + j*2, "%02x", digest_out[j] );

		if ( parse_autovivified_setting ( "hash", &setting ) == 0 ) {
			setting.setting.type = &setting_type_string;
			storef_setting ( setting.settings, &setting.setting,
				hashstr );
		}

		if ( parse_autovivified_setting ( "hashlen", &setting ) == 0 ) {
			setting.setting.type = &setting_type_int32;
			storen_setting ( setting.settings, &setting.setting,
				origlen );
		}

		if ( opts.str ) {
			printf( "%s\n", hashstr );
			break;
		}

		printf ( "%s  %s\n", hashstr, image->name );
	}

	return 0;
}

static int md5sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &md5_algorithm );
}

static int sha1sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &sha1_algorithm );
}

static int sha224sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &sha224_algorithm );
}

static int sha256sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &sha256_algorithm );
}

static int sha384sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &sha384_algorithm );
}

static int sha512sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &sha512_algorithm );
}

struct command md5sum_command __command = {
	.name = "md5sum",
	.exec = md5sum_exec,
};

struct command sha1sum_command __command = {
	.name = "sha1sum",
	.exec = sha1sum_exec,
};

struct command sha224sum_command __command = {
	.name = "sha224sum",
	.exec = sha224sum_exec,
};

struct command sha256sum_command __command = {
	.name = "sha256sum",
	.exec = sha256sum_exec,
};

struct command sha384sum_command __command = {
	.name = "sha384sum",
	.exec = sha384sum_exec,
};

struct command sha512sum_command __command = {
	.name = "sha512sum",
	.exec = sha512sum_exec,
};

