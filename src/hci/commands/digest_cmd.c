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
#include <usr/imgmgmt.h>

/** @file
 *
 * Digest commands
 *
 */

/** "digest" options */
struct digest_options {};

/** "digest" option list */
static struct option_descriptor digest_opts[] = {};

/** "digest" command descriptor */
static struct command_descriptor digest_cmd =
	COMMAND_DESC ( struct digest_options, digest_opts, 1, MAX_ARGUMENTS,
		       "<image> [<image>...]" );

struct digest_verify_options {};

/** "digest_verify" command descriptor */
static struct command_descriptor digest_verify_cmd =
	COMMAND_DESC ( struct digest_verify_options, digest_opts, 1, MAX_ARGUMENTS,
		       "<image> <digesthex>" );

static int digest_one(char *imageuri, char *digest_out_hex, struct image **image, struct digest_algorithm *digest) {
	uint8_t digest_ctx[digest->ctxsize];
	size_t frag_len;
	uint8_t buf[128];
	uint8_t digest_out[digest->digestsize];
	size_t offset;
	size_t len;
	unsigned j;
	int rc;

	/* Acquire image */
	if ( ( rc = imgacquire ( imageuri, 0, image ) ) != 0 )
		return rc;
	offset = 0;
	len = (*image)->len;

	/* calculate digest */
	digest_init ( digest, digest_ctx );
	while ( len ) {
		frag_len = len;
		if ( frag_len > sizeof ( buf ) )
			frag_len = sizeof ( buf );
		copy_from_user ( buf, (*image)->data, offset, frag_len );
		digest_update ( digest, digest_ctx, buf, frag_len );
		len -= frag_len;
		offset += frag_len;
	}
	digest_final ( digest, digest_ctx, digest_out );
	for ( j = 0 ; j < sizeof ( digest_out ) ; j++ ) {
		snprintf ( digest_out_hex + 2 * j, 3, "%02x", digest_out[j] );
	}

	return 0;
}

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
	char digest_out_hex[1 + 2 * digest->digestsize];
	int i;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &digest_cmd, &opts ) ) != 0 )
		return rc;

	for ( i = optind ; i < argc ; i++ ) {

		if ( ( rc = digest_one ( argv[i], digest_out_hex, &image, digest) ) != 0 )
			continue;

		printf ( "%s  %s\n", digest_out_hex, image->name );
	}

	return 0;
}

/**
 * The "digest_verify" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v digest		Digest algorithm
 * @ret rc		Return status code
 */
static int digest_verify_exec ( int argc, char **argv,
			 struct digest_algorithm *digest ) {
	struct digest_options opts;
	struct image *image;
	char digest_out_hex[1 + 2 * digest->digestsize];
	int rc;
	char *intended_digest;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &digest_verify_cmd, &opts ) ) != 0 )
		return rc;

	if ( argc != 3 ) {
		printf( "argc should be 3, not %i\n", argc );
		sleep( 2 );
		return -1;
	}

	if ( ( rc = digest_one ( argv[optind], digest_out_hex, &image, digest) ) != 0 )
		return rc;
	intended_digest = argv[optind+1];
	if ( strlen( intended_digest ) != 2 * digest->digestsize ) {
		printf ( "reference digest has wrong length\n" );
		sleep ( 2 );
		return 1;
	}

#ifndef NDEBUG
	printf ( "comparing %s to %s\n", digest_out_hex, intended_digest );
#endif
	if ( strncmp( digest_out_hex, intended_digest, 2 * digest->digestsize ) != 0 ) {
		printf ( "digest verification failed for %s %s\n", argv[optind], argv[optind+1] );
		sleep ( 2 );
		return 1;
	}

	printf ( "  verified %s\n", image->name );
	return 0;
}

static int md5sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &md5_algorithm );
}

static int sha1sum_exec ( int argc, char **argv ) {
	return digest_exec ( argc, argv, &sha1_algorithm );
}

static int sha1verify_exec ( int argc, char **argv ) {
	return digest_verify_exec ( argc, argv, &sha1_algorithm );
}

struct command md5sum_command __command = {
	.name = "md5sum",
	.exec = md5sum_exec,
};

struct command sha1sum_command __command = {
	.name = "sha1sum",
	.exec = sha1sum_exec,
};

struct command sha1verify_command __command = {
	.name = "sha1verify",
	.exec = sha1verify_exec,
};
