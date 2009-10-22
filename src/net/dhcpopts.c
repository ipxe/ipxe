/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <gpxe/dhcp.h>
#include <gpxe/dhcpopts.h>

/** @file
 *
 * DHCP options
 *
 */

/**
 * Obtain printable version of a DHCP option tag
 *
 * @v tag		DHCP option tag
 * @ret name		String representation of the tag
 *
 */
static inline char * dhcp_tag_name ( unsigned int tag ) {
	static char name[8];

	if ( DHCP_IS_ENCAP_OPT ( tag ) ) {
		snprintf ( name, sizeof ( name ), "%d.%d",
			   DHCP_ENCAPSULATOR ( tag ),
			   DHCP_ENCAPSULATED ( tag ) );
	} else {
		snprintf ( name, sizeof ( name ), "%d", tag );
	}
	return name;
}

/**
 * Get pointer to DHCP option
 *
 * @v options		DHCP options block
 * @v offset		Offset within options block
 * @ret option		DHCP option
 */
static inline __attribute__ (( always_inline )) struct dhcp_option *
dhcp_option ( struct dhcp_options *options, unsigned int offset ) {
	return ( ( struct dhcp_option * ) ( options->data + offset ) );
}

/**
 * Get offset of a DHCP option
 *
 * @v options		DHCP options block
 * @v option		DHCP option
 * @ret offset		Offset within options block
 */
static inline __attribute__ (( always_inline )) int
dhcp_option_offset ( struct dhcp_options *options,
		     struct dhcp_option *option ) {
	return ( ( ( void * ) option ) - options->data );
}

/**
 * Calculate length of any DHCP option
 *
 * @v option		DHCP option
 * @ret len		Length (including tag and length field)
 */
static unsigned int dhcp_option_len ( struct dhcp_option *option ) {
	if ( ( option->tag == DHCP_END ) || ( option->tag == DHCP_PAD ) ) {
		return 1;
	} else {
		return ( option->len + DHCP_OPTION_HEADER_LEN );
	}
}

/**
 * Find DHCP option within DHCP options block, and its encapsulator (if any)
 *
 * @v options		DHCP options block
 * @v tag		DHCP option tag to search for
 * @ret encap_offset	Offset of encapsulating DHCP option
 * @ret offset		Offset of DHCP option, or negative error
 *
 * Searches for the DHCP option matching the specified tag within the
 * DHCP option block.  Encapsulated options may be searched for by
 * using DHCP_ENCAP_OPT() to construct the tag value.
 *
 * If the option is encapsulated, and @c encap_offset is non-NULL, it
 * will be filled in with the offset of the encapsulating option.
 *
 * This routine is designed to be paranoid.  It does not assume that
 * the option data is well-formatted, and so must guard against flaws
 * such as options missing a @c DHCP_END terminator, or options whose
 * length would take them beyond the end of the data block.
 */
static int find_dhcp_option_with_encap ( struct dhcp_options *options,
					 unsigned int tag,
					 int *encap_offset ) {
	unsigned int original_tag __attribute__ (( unused )) = tag;
	struct dhcp_option *option;
	int offset = 0;
	ssize_t remaining = options->len;
	unsigned int option_len;

	/* Sanity check */
	if ( tag == DHCP_PAD )
		return -ENOENT;

	/* Search for option */
	while ( remaining ) {
		/* Calculate length of this option.  Abort processing
		 * if the length is malformed (i.e. takes us beyond
		 * the end of the data block).
		 */
		option = dhcp_option ( options, offset );
		option_len = dhcp_option_len ( option );
		remaining -= option_len;
		if ( remaining < 0 )
			break;
		/* Check for explicit end marker */
		if ( option->tag == DHCP_END ) {
			if ( tag == DHCP_END )
				/* Special case where the caller is interested
				 * in whether we have this marker or not.
				 */
				return offset;
			else
				break;
		}
		/* Check for matching tag */
		if ( option->tag == tag ) {
			DBGC ( options, "DHCPOPT %p found %s (length %d)\n",
			       options, dhcp_tag_name ( original_tag ),
			       option_len );
			return offset;
		}
		/* Check for start of matching encapsulation block */
		if ( DHCP_IS_ENCAP_OPT ( tag ) &&
		     ( option->tag == DHCP_ENCAPSULATOR ( tag ) ) ) {
			if ( encap_offset )
				*encap_offset = offset;
			/* Continue search within encapsulated option block */
			tag = DHCP_ENCAPSULATED ( tag );
			remaining = option_len;
			offset += DHCP_OPTION_HEADER_LEN;
			continue;
		}
		offset += option_len;
	}

	return -ENOENT;
}

/**
 * Resize a DHCP option
 *
 * @v options		DHCP option block
 * @v offset		Offset of option to resize
 * @v encap_offset	Offset of encapsulating offset (or -ve for none)
 * @v old_len		Old length (including header)
 * @v new_len		New length (including header)
 * @v can_realloc	Can reallocate options data if necessary
 * @ret rc		Return status code
 */
static int resize_dhcp_option ( struct dhcp_options *options,
				int offset, int encap_offset,
				size_t old_len, size_t new_len,
				int can_realloc ) {
	struct dhcp_option *encapsulator;
	struct dhcp_option *option;
	ssize_t delta = ( new_len - old_len );
	size_t new_options_len;
	size_t new_encapsulator_len;
	void *new_data;
	void *source;
	void *dest;
	void *end;

	/* Check for sufficient space, and update length fields */
	if ( new_len > DHCP_MAX_LEN ) {
		DBGC ( options, "DHCPOPT %p overlength option\n", options );
		return -ENOSPC;
	}
	new_options_len = ( options->len + delta );
	if ( new_options_len > options->max_len ) {
		/* Reallocate options block if allowed to do so. */
		if ( can_realloc ) {
			new_data = realloc ( options->data, new_options_len );
			if ( ! new_data ) {
				DBGC ( options, "DHCPOPT %p could not "
				       "reallocate to %zd bytes\n", options,
				       new_options_len );
				return -ENOMEM;
			}
			options->data = new_data;
			options->max_len = new_options_len;
		} else {
			DBGC ( options, "DHCPOPT %p out of space\n", options );
			return -ENOMEM;
		}
	}
	if ( encap_offset >= 0 ) {
		encapsulator = dhcp_option ( options, encap_offset );
		new_encapsulator_len = ( encapsulator->len + delta );
		if ( new_encapsulator_len > DHCP_MAX_LEN ) {
			DBGC ( options, "DHCPOPT %p overlength encapsulator\n",
			       options );
			return -ENOSPC;
		}
		encapsulator->len = new_encapsulator_len;
	}
	options->len = new_options_len;

	/* Move remainder of option data */
	option = dhcp_option ( options, offset );
	source = ( ( ( void * ) option ) + old_len );
	dest = ( ( ( void * ) option ) + new_len );
	end = ( options->data + options->max_len );
	memmove ( dest, source, ( end - dest ) );

	return 0;
}

/**
 * Set value of DHCP option
 *
 * @v options		DHCP option block
 * @v tag		DHCP option tag
 * @v data		New value for DHCP option
 * @v len		Length of value, in bytes
 * @v can_realloc	Can reallocate options data if necessary
 * @ret offset		Offset of DHCP option, or negative error
 *
 * Sets the value of a DHCP option within the options block.  The
 * option may or may not already exist.  Encapsulators will be created
 * (and deleted) as necessary.
 *
 * This call may fail due to insufficient space in the options block.
 * If it does fail, and the option existed previously, the option will
 * be left with its original value.
 */
static int set_dhcp_option ( struct dhcp_options *options, unsigned int tag,
			     const void *data, size_t len,
			     int can_realloc ) {
	static const uint8_t empty_encapsulator[] = { DHCP_END };
	int offset;
	int encap_offset = -1;
	int creation_offset;
	struct dhcp_option *option;
	unsigned int encap_tag = DHCP_ENCAPSULATOR ( tag );
	size_t old_len = 0;
	size_t new_len = ( len ? ( len + DHCP_OPTION_HEADER_LEN ) : 0 );
	int rc;

	/* Sanity check */
	if ( tag == DHCP_PAD )
		return -ENOTTY;

	creation_offset = find_dhcp_option_with_encap ( options, DHCP_END,
							NULL );
	if ( creation_offset < 0 )
		creation_offset = options->len;
	/* Find old instance of this option, if any */
	offset = find_dhcp_option_with_encap ( options, tag, &encap_offset );
	if ( offset >= 0 ) {
		old_len = dhcp_option_len ( dhcp_option ( options, offset ) );
		DBGC ( options, "DHCPOPT %p resizing %s from %zd to %zd\n",
		       options, dhcp_tag_name ( tag ), old_len, new_len );
	} else {
		DBGC ( options, "DHCPOPT %p creating %s (length %zd)\n",
		       options, dhcp_tag_name ( tag ), new_len );
	}

	/* Ensure that encapsulator exists, if required */
	if ( encap_tag ) {
		if ( encap_offset < 0 )
			encap_offset = set_dhcp_option ( options, encap_tag,
							 empty_encapsulator, 1,
							 can_realloc );
		if ( encap_offset < 0 )
			return encap_offset;
		creation_offset = ( encap_offset + DHCP_OPTION_HEADER_LEN );
	}

	/* Create new option if necessary */
	if ( offset < 0 )
		offset = creation_offset;

	/* Resize option to fit new data */
	if ( ( rc = resize_dhcp_option ( options, offset, encap_offset,
					 old_len, new_len,
					 can_realloc ) ) != 0 )
		return rc;

	/* Copy new data into option, if applicable */
	if ( len ) {
		option = dhcp_option ( options, offset );
		option->tag = tag;
		option->len = len;
		memcpy ( &option->data, data, len );
	}

	/* Delete encapsulator if there's nothing else left in it */
	if ( encap_offset >= 0 ) {
		option = dhcp_option ( options, encap_offset );
		if ( option->len <= 1 )
			set_dhcp_option ( options, encap_tag, NULL, 0, 0 );
	}

	return offset;
}

/**
 * Store value of DHCP option setting
 *
 * @v options		DHCP option block
 * @v tag		Setting tag number
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
int dhcpopt_store ( struct dhcp_options *options, unsigned int tag,
		    const void *data, size_t len ) {
	int offset;

	offset = set_dhcp_option ( options, tag, data, len, 0 );
	if ( offset < 0 )
		return offset;
	return 0;
}

/**
 * Store value of DHCP option setting, extending options block if necessary
 *
 * @v options		DHCP option block
 * @v tag		Setting tag number
 * @v data		Setting data, or NULL to clear setting
 * @v len		Length of setting data
 * @ret rc		Return status code
 */
int dhcpopt_extensible_store ( struct dhcp_options *options, unsigned int tag,
			       const void *data, size_t len ) {
	int offset;

	offset = set_dhcp_option ( options, tag, data, len, 1 );
	if ( offset < 0 )
		return offset;
	return 0;
}

/**
 * Fetch value of DHCP option setting
 *
 * @v options		DHCP option block
 * @v tag		Setting tag number
 * @v data		Buffer to fill with setting data
 * @v len		Length of buffer
 * @ret len		Length of setting data, or negative error
 */
int dhcpopt_fetch ( struct dhcp_options *options, unsigned int tag,
		    void *data, size_t len ) {
	int offset;
	struct dhcp_option *option;
	size_t option_len;

	offset = find_dhcp_option_with_encap ( options, tag, NULL );
	if ( offset < 0 )
		return offset;

	option = dhcp_option ( options, offset );
	option_len = option->len;
	if ( len > option_len )
		len = option_len;
	memcpy ( data, option->data, len );

	return option_len;
}

/**
 * Recalculate length of DHCP options block
 *
 * @v options		Uninitialised DHCP option block
 *
 * The "used length" field will be updated based on scanning through
 * the block to find the end of the options.
 */
static void dhcpopt_update_len ( struct dhcp_options *options ) {
	struct dhcp_option *option;
	int offset = 0;
	ssize_t remaining = options->max_len;
	unsigned int option_len;

	/* Find last non-pad option */
	options->len = 0;
	while ( remaining ) {
		option = dhcp_option ( options, offset );
		option_len = dhcp_option_len ( option );
		remaining -= option_len;
		if ( remaining < 0 )
			break;
		offset += option_len;
		if ( option->tag != DHCP_PAD )
			options->len = offset;
	}
}

/**
 * Initialise prepopulated block of DHCP options
 *
 * @v options		Uninitialised DHCP option block
 * @v data		Memory for DHCP option data
 * @v max_len		Length of memory for DHCP option data
 *
 * The memory content must already be filled with valid DHCP options.
 * A zeroed block counts as a block of valid DHCP options.
 */
void dhcpopt_init ( struct dhcp_options *options, void *data,
		    size_t max_len ) {

	/* Fill in fields */
	options->data = data;
	options->max_len = max_len;

	/* Update length */
	dhcpopt_update_len ( options );

	DBGC ( options, "DHCPOPT %p created (data %p len %#zx max_len %#zx)\n",
	       options, options->data, options->len, options->max_len );
}
