/*
 * Copyright (C) 2014 Marin Hannache <ipxe@mareo.fr>.
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

FILE_SECBOOT ( FORBIDDEN );

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <ipxe/nfs_uri.h>

/** @file
 *
 * Network File System protocol URI handling functions
 *
 */

int nfs_uri_init ( struct nfs_uri *nfs_uri, const struct uri *uri ) {
	if ( ! ( nfs_uri->mountpoint = strdup ( uri->path ) ) )
		return -ENOMEM;

	nfs_uri->filename = basename ( nfs_uri->mountpoint );
	if ( strchr ( uri->path, '/' ) != NULL )
		nfs_uri->mountpoint = dirname ( nfs_uri->mountpoint );

	if ( nfs_uri->filename[0] == '\0' ) {
		free ( nfs_uri->mountpoint );
		return -EINVAL;
	}

	if ( ! ( nfs_uri->path = strdup ( nfs_uri->filename ) ) ) {
		free ( nfs_uri->mountpoint );
		return -ENOMEM;
	}
	nfs_uri->lookup_pos = nfs_uri->path;

	return 0;
}

char *nfs_uri_mountpoint ( const struct nfs_uri *uri ) {
	if ( uri->mountpoint + 1 == uri->filename ||
	     uri->mountpoint     == uri->filename )
		return "/";

	return uri->mountpoint;
}

int nfs_uri_next_mountpoint ( struct nfs_uri *uri ) {
	char *sep;

	if ( uri->mountpoint + 1 == uri->filename ||
	     uri->mountpoint     == uri->filename )
		return -ENOENT;

	sep = strrchr ( uri->mountpoint, '/' );
	uri->filename[-1] = '/';
	uri->filename     = sep + 1;
	*sep = '\0';

	free ( uri->path );
	if ( ! ( uri->path = strdup ( uri->filename ) ) ) {
		uri->path = NULL;
		return -ENOMEM;
	}
	uri->lookup_pos = uri->path;

	return 0;
}

int nfs_uri_symlink ( struct nfs_uri *uri, const char *symlink ) {
	size_t len;
	size_t symlink_len;
	size_t lookup_pos_len;
	size_t mountpoint_len;
	char *new_path;

	if ( ! uri->path )
		return -EINVAL;

	if ( *symlink == '/' )
	{
		if ( strncmp ( symlink, uri->mountpoint,
			       strlen ( uri->mountpoint ) ) != 0 )
			return -EINVAL;

		mountpoint_len = strlen ( uri->mountpoint );
		symlink_len = strlen ( symlink );
		lookup_pos_len = strlen ( uri->lookup_pos );

		/* Security: Calculate exact length needed and verify no overflow */
		if ( symlink_len < mountpoint_len )
			return -EINVAL;

		len = lookup_pos_len + ( symlink_len - mountpoint_len );
		if ( len >= symlink_len || len >= lookup_pos_len )
			return -EINVAL;

		new_path = malloc ( ( len + 1 ) * sizeof ( char ) );
		if ( ! new_path )
			return -ENOMEM;

		/* Security: Use memcpy with explicit lengths instead of strcpy */
		memcpy ( new_path, symlink + mountpoint_len, symlink_len - mountpoint_len );
		new_path[symlink_len - mountpoint_len] = '\0';
		memcpy ( new_path + ( symlink_len - mountpoint_len ), uri->lookup_pos, lookup_pos_len );
		new_path[len] = '\0';

	} else {
		symlink_len = strlen ( symlink );
		lookup_pos_len = strlen ( uri->lookup_pos );

		/* Security: Calculate exact length needed and verify no overflow */
		len = lookup_pos_len + symlink_len;
		if ( len < symlink_len || len < lookup_pos_len )
			return -EINVAL;

		new_path = malloc ( ( len + 1 ) * sizeof ( char ) );
		if ( ! new_path )
			return -ENOMEM;

		/* Security: Use memcpy with explicit lengths instead of strcpy */
		memcpy ( new_path, symlink, symlink_len );
		new_path[symlink_len] = '\0';
		memcpy ( new_path + symlink_len, uri->lookup_pos, lookup_pos_len );
		new_path[len] = '\0';
	}

	free ( uri->path );
	uri->lookup_pos = uri->path = new_path;

	return 0;
}

char *nfs_uri_next_path_component ( struct nfs_uri *uri ) {
	char *sep;
	char *start;

	if ( ! uri->path )
		return NULL;

	for ( sep = uri->lookup_pos ; *sep != '\0' && *sep != '/'; sep++ )
		;

	start = uri->lookup_pos;
	uri->lookup_pos = sep;
	if ( *sep != '\0' ) {
		uri->lookup_pos++;
		*sep = '\0';
		if ( *start == '\0' )
			return nfs_uri_next_path_component ( uri );
	}

	return start;
}

void nfs_uri_free ( struct nfs_uri *uri ) {
	free ( uri->mountpoint );
	free ( uri->path );
	uri->mountpoint = NULL;
	uri->path       = NULL;
}
