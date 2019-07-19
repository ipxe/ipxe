/*
 * Copyright (C) 2019 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/netdevice.h>
#include <ipxe/fdt.h>

/** @file
 *
 * Flattened Device Tree
 *
 */

/** The system flattened device tree (if present) */
static struct fdt fdt;

/** A position within a device tree */
struct fdt_cursor {
	/** Offset within structure block */
	unsigned int offset;
	/** Tree depth */
	int depth;
};

/** A lexical descriptor */
struct fdt_descriptor {
	/** Node or property name (if applicable) */
	const char *name;
	/** Property data (if applicable) */
	const void *data;
	/** Length of property data (if applicable) */
	size_t len;
};

/**
 * Check if device tree exists
 *
 * @v has_fdt		Device tree exists
 */
static inline __attribute__ (( always_inline )) int fdt_exists ( void ) {

	return ( fdt.hdr != NULL );
}

/**
 * Traverse device tree
 *
 * @v pos		Position within device tree
 * @v desc		Lexical descriptor to fill in
 * @ret rc		Return status code
 */
static int fdt_traverse ( struct fdt_cursor *pos,
			  struct fdt_descriptor *desc ) {
	const fdt_token_t *token;
	const void *data;
	const struct fdt_prop *prop;
	unsigned int name_off;
	size_t remaining;
	size_t len;

	/* Sanity checks */
	assert ( pos->offset < fdt.len );
	assert ( ( pos->offset & ( FDT_STRUCTURE_ALIGN - 1 ) ) == 0 );

	/* Clear descriptor */
	memset ( desc, 0, sizeof ( *desc ) );

	/* Locate token and calculate remaining space */
	token = ( fdt.raw + fdt.structure + pos->offset );
	remaining = ( fdt.len - pos->offset );
	if ( remaining < sizeof ( *token ) ) {
		DBGC ( &fdt, "FDT truncated tree at +%#04x\n", pos->offset );
		return -EINVAL;
	}
	remaining -= sizeof ( *token );
	data = ( ( ( const void * ) token ) + sizeof ( *token ) );
	len = 0;

	/* Handle token */
	switch ( *token ) {

	case cpu_to_be32 ( FDT_BEGIN_NODE ):

		/* Start of node */
		desc->name = data;
		len = ( strnlen ( desc->name, remaining ) + 1 /* NUL */ );
		if ( remaining < len ) {
			DBGC ( &fdt, "FDT unterminated node name at +%#04x\n",
			       pos->offset );
			return -EINVAL;
		}
		pos->depth++;
		break;

	case cpu_to_be32 ( FDT_END_NODE ):

		/* End of node */
		if ( pos->depth < 0 ) {
			DBGC ( &fdt, "FDT spurious node end at +%#04x\n",
			       pos->offset );
			return -EINVAL;
		}
		pos->depth--;
		if ( pos->depth < 0 ) {
			/* End of (sub)tree */
			return -ENOENT;
		}
		break;

	case cpu_to_be32 ( FDT_PROP ):

		/* Property */
		prop = data;
		if ( remaining < sizeof ( *prop ) ) {
			DBGC ( &fdt, "FDT truncated property at +%#04x\n",
			       pos->offset );
			return -EINVAL;
		}
		desc->data = ( ( ( const void * ) prop ) + sizeof ( *prop ) );
		desc->len = be32_to_cpu ( prop->len );
		len = ( sizeof ( *prop ) + desc->len );
		if ( remaining < len ) {
			DBGC ( &fdt, "FDT overlength property at +%#04x\n",
			       pos->offset );
			return -EINVAL;
		}
		name_off = be32_to_cpu ( prop->name_off );
		if ( name_off > fdt.strings_len ) {
			DBGC ( &fdt, "FDT property name outside strings "
			       "block at +%#04x\n", pos->offset );
			return -EINVAL;
		}
		desc->name = ( fdt.raw + fdt.strings + name_off );
		break;

	case cpu_to_be32 ( FDT_NOP ):

		/* Do nothing */
		break;

	default:

		/* Unrecognised or unexpected token */
		DBGC ( &fdt, "FDT unexpected token %#08x at +%#04x\n",
		       be32_to_cpu ( *token ), pos->offset );
		return -EINVAL;
	}

	/* Update cursor */
	len = ( ( len + FDT_STRUCTURE_ALIGN - 1 ) &
		~( FDT_STRUCTURE_ALIGN - 1 ) );
	pos->offset += ( sizeof ( *token ) + len );

	/* Sanity checks */
	assert ( pos->offset <= fdt.len );

	return 0;
}

/**
 * Find child node
 *
 * @v offset		Starting node offset
 * @v name		Node name
 * @v child		Child node offset to fill in
 * @ret rc		Return status code
 */
static int fdt_child ( unsigned int offset, const char *name,
		       unsigned int *child ) {
	struct fdt_cursor pos;
	struct fdt_descriptor desc;
	unsigned int orig_offset;
	int rc;

	/* Record original offset (for debugging) */
	orig_offset = offset;

	/* Initialise cursor */
	pos.offset = offset;
	pos.depth = -1;

	/* Find child node */
	while ( 1 ) {

		/* Record current offset */
		*child = pos.offset;

		/* Traverse tree */
		if ( ( rc = fdt_traverse ( &pos, &desc ) ) != 0 ) {
			DBGC ( &fdt, "FDT +%#04x has no child node \"%s\": "
			       "%s\n", orig_offset, name, strerror ( rc ) );
			return rc;
		}

		/* Check for matching immediate child node */
		if ( ( pos.depth == 1 ) && desc.name && ( ! desc.data ) ) {
			DBGC2 ( &fdt, "FDT +%#04x has child node \"%s\"\n",
				orig_offset, desc.name );
			if ( strcmp ( name, desc.name ) == 0 ) {
				DBGC2 ( &fdt, "FDT +%#04x found child node "
					"\"%s\" at +%#04x\n", orig_offset,
					desc.name, *child );
				return 0;
			}
		}
	}
}

/**
 * Find node by path
 *
 * @v path		Node path
 * @v offset		Offset to fill in
 * @ret rc		Return status code
 */
int fdt_path ( const char *path, unsigned int *offset ) {
	char *tmp = ( ( char * ) path );
	char *del;
	int rc;

	/* Initialise offset */
	*offset = 0;

	/* Traverse tree one path segment at a time */
	while ( *tmp ) {

		/* Skip any leading '/' */
		while ( *tmp == '/' )
			tmp++;

		/* Find next '/' delimiter and convert to NUL */
		del = strchr ( tmp, '/' );
		if ( del )
			*del = '\0';

		/* Find child and restore delimiter */
		rc = fdt_child ( *offset, tmp, offset );
		if ( del )
			*del = '/';
		if ( rc != 0 )
			return rc;

		/* Move to next path component, if any */
		while ( *tmp && ( *tmp != '/' ) )
			tmp++;
	}

	DBGC2 ( &fdt, "FDT found path \"%s\" at +%#04x\n", path, *offset );
	return 0;
}

/**
 * Find node by alias
 *
 * @v name		Alias name
 * @v offset		Offset to fill in
 * @ret rc		Return status code
 */
int fdt_alias ( const char *name, unsigned int *offset ) {
	const char *alias;
	int rc;

	/* Locate "/aliases" node */
	if ( ( rc = fdt_child ( 0, "aliases", offset ) ) != 0 )
		return rc;

	/* Locate alias property */
	if ( ( alias = fdt_string ( *offset, name ) ) == NULL )
		return -ENOENT;
	DBGC ( &fdt, "FDT alias \"%s\" is \"%s\"\n", name, alias );

	/* Locate aliased node */
	if ( ( rc = fdt_path ( alias, offset ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Find property
 *
 * @v offset		Starting node offset
 * @v name		Property name
 * @v desc		Lexical descriptor to fill in
 * @ret rc		Return status code
 */
static int fdt_property ( unsigned int offset, const char *name,
			  struct fdt_descriptor *desc ) {
	struct fdt_cursor pos;
	int rc;

	/* Initialise cursor */
	pos.offset = offset;
	pos.depth = -1;

	/* Find property */
	while ( 1 ) {

		/* Traverse tree */
		if ( ( rc = fdt_traverse ( &pos, desc ) ) != 0 ) {
			DBGC ( &fdt, "FDT +%#04x has no property \"%s\": %s\n",
			       offset, name, strerror ( rc ) );
			return rc;
		}

		/* Check for matching immediate child property */
		if ( ( pos.depth == 0 ) && desc->data ) {
			DBGC2 ( &fdt, "FDT +%#04x has property \"%s\" len "
				"%#zx\n", offset, desc->name, desc->len );
			if ( strcmp ( name, desc->name ) == 0 ) {
				DBGC2 ( &fdt, "FDT +%#04x found property "
					"\"%s\"\n", offset, desc->name );
				DBGC2_HDA ( &fdt, 0, desc->data, desc->len );
				return 0;
			}
		}
	}
}

/**
 * Find string property
 *
 * @v offset		Starting node offset
 * @v name		Property name
 * @ret string		String property, or NULL on error
 */
const char * fdt_string ( unsigned int offset, const char *name ) {
	struct fdt_descriptor desc;
	int rc;

	/* Find property */
	if ( ( rc = fdt_property ( offset, name, &desc ) ) != 0 )
		return NULL;

	/* Check NUL termination */
	if ( strnlen ( desc.data, desc.len ) == desc.len ) {
		DBGC ( &fdt, "FDT unterminated string property \"%s\"\n",
		       name );
		return NULL;
	}

	return desc.data;
}

/**
 * Get MAC address from property
 *
 * @v offset		Starting node offset
 * @v netdev		Network device
 * @ret rc		Return status code
 */
int fdt_mac ( unsigned int offset, struct net_device *netdev ) {
	struct fdt_descriptor desc;
	size_t len;
	int rc;

	/* Find applicable MAC address property */
	if ( ( ( rc = fdt_property ( offset, "mac-address", &desc ) ) != 0 ) &&
	     ( ( rc = fdt_property ( offset, "local-mac-address",
				     &desc ) ) != 0 ) ) {
		return rc;
	}

	/* Check length */
	len = netdev->ll_protocol->hw_addr_len;
	if ( len != desc.len ) {
		DBGC ( &fdt, "FDT malformed MAC address \"%s\":\n",
		       desc.name );
		DBGC_HDA ( &fdt, 0, desc.data, desc.len );
		return -ERANGE;
	}

	/* Fill in MAC address */
	memcpy ( netdev->hw_addr, desc.data, len );

	return 0;
}

/**
 * Register device tree
 *
 * @v fdt		Device tree header
 * @ret rc		Return status code
 */
int register_fdt ( const struct fdt_header *hdr ) {
	const uint8_t *end;

	/* Record device tree location */
	fdt.hdr = hdr;
	fdt.len = be32_to_cpu ( hdr->totalsize );
	DBGC ( &fdt, "FDT version %d at %p+%#04zx\n",
	       be32_to_cpu ( hdr->version ), fdt.hdr, fdt.len );

	/* Check signature */
	if ( hdr->magic != cpu_to_be32 ( FDT_MAGIC ) ) {
		DBGC ( &fdt, "FDT has invalid magic value %#08x\n",
		       be32_to_cpu ( hdr->magic ) );
		goto err;
	}

	/* Check version */
	if ( hdr->last_comp_version != cpu_to_be32 ( FDT_VERSION ) ) {
		DBGC ( &fdt, "FDT unsupported version %d\n",
		       be32_to_cpu ( hdr->last_comp_version ) );
		goto err;
	}

	/* Record structure block location */
	fdt.structure = be32_to_cpu ( hdr->off_dt_struct );
	fdt.structure_len = be32_to_cpu ( hdr->size_dt_struct );
	DBGC ( &fdt, "FDT structure block at +[%#04x,%#04zx)\n",
	       fdt.structure, ( fdt.structure + fdt.structure_len ) );
	if ( ( fdt.structure > fdt.len ) ||
	     ( fdt.structure_len > ( fdt.len - fdt.structure ) ) ) {
		DBGC ( &fdt, "FDT structure block exceeds table\n" );
		goto err;
	}
	if ( ( fdt.structure | fdt.structure_len ) &
	     ( FDT_STRUCTURE_ALIGN - 1 ) ) {
		DBGC ( &fdt, "FDT structure block is misaligned\n" );
		goto err;
	}

	/* Record strings block location */
	fdt.strings = be32_to_cpu ( hdr->off_dt_strings );
	fdt.strings_len = be32_to_cpu ( hdr->size_dt_strings );
	DBGC ( &fdt, "FDT strings block at +[%#04x,%#04zx)\n",
	       fdt.strings, ( fdt.strings + fdt.strings_len ) );
	if ( ( fdt.strings > fdt.len ) ||
	     ( fdt.strings_len > ( fdt.len - fdt.strings ) ) ) {
		DBGC ( &fdt, "FDT strings block exceeds table\n" );
		goto err;
	}

	/* Shrink strings block to ensure NUL termination safety */
	end = ( fdt.raw + fdt.strings + fdt.strings_len );
	for ( ; fdt.strings_len ; fdt.strings_len-- ) {
		if ( *(--end) == '\0' )
			break;
	}
	if ( fdt.strings_len != be32_to_cpu ( hdr->size_dt_strings ) ) {
		DBGC ( &fdt, "FDT strings block shrunk to +[%#04x,%#04zx)\n",
		       fdt.strings, ( fdt.strings + fdt.strings_len ) );
	}

	/* Print model name (for debugging) */
	DBGC ( &fdt, "FDT model is \"%s\"\n", fdt_string ( 0, "model" ) );

	return 0;

 err:
	DBGC_HDA ( &fdt, 0, hdr, sizeof ( *hdr ) );
	fdt.hdr = NULL;
	return -EINVAL;
}

/* Drag in objects via register_fdt */
REQUIRING_SYMBOL ( register_fdt );

/* Drag in device tree configuration */
REQUIRE_OBJECT ( config_fdt );
