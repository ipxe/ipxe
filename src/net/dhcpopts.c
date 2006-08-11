/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <byteswap.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <vsprintf.h>
#include <gpxe/list.h>
#include <gpxe/in.h>
#include <gpxe/dhcp.h>

/** @file
 *
 * DHCP options
 *
 */

/** List of registered DHCP option blocks */
static LIST_HEAD ( option_blocks );

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
 * Obtain value of a numerical DHCP option
 *
 * @v option		DHCP option, or NULL
 * @ret value		Numerical value of the option, or 0
 *
 * Parses the numerical value from a DHCP option, if present.  It is
 * permitted to call dhcp_num_option() with @c option set to NULL; in
 * this case 0 will be returned.
 *
 * The caller does not specify the size of the DHCP option data; this
 * is implied by the length field stored within the DHCP option
 * itself.
 */
unsigned long dhcp_num_option ( struct dhcp_option *option ) {
	unsigned long value = 0;
	uint8_t *data;

	if ( option ) {
		/* This is actually smaller code than using htons()
		 * etc., and will also cope well with malformed
		 * options (such as zero-length options).
		 */
		for ( data = option->data.bytes ;
		      data < ( option->data.bytes + option->len ) ; data++ )
			value = ( ( value << 8 ) | *data );
	}
	return value;
}

/**
 * Obtain value of an IPv4-address DHCP option
 *
 * @v option		DHCP option, or NULL
 * @v inp		IPv4 address to fill in
 *
 * Parses the IPv4 address value from a DHCP option, if present.  It
 * is permitted to call dhcp_ipv4_option() with @c option set to NULL;
 * in this case the address will be set to 0.0.0.0.
 */
void dhcp_ipv4_option ( struct dhcp_option *option, struct in_addr *inp ) {
	if ( option )
		*inp = option->data.in;
}

/**
 * Print DHCP string option value into buffer
 *
 * @v buf		Buffer into which to write the string
 * @v size		Size of buffer
 * @v option		DHCP option, or NULL
 * @ret len		Length of formatted string
 *
 * DHCP option strings are stored without a NUL terminator.  This
 * function provides a convenient way to extract these DHCP strings
 * into standard C strings.  It is permitted to call dhcp_snprintf()
 * with @c option set to NULL; in this case the buffer will be filled
 * with an empty string.
 *
 * The usual snprintf() semantics apply with regard to buffer size,
 * return value when the buffer is too small, etc.
 */
int dhcp_snprintf ( char *buf, size_t size, struct dhcp_option *option ) {
	size_t len;
	char *content = "";

	if ( option ) {
		/* Shrink buffer size so that it is only just large
		 * enough to contain the option data.  snprintf() will
		 * take care of everything else (inserting the NUL etc.)
		 */
		len = ( option->len + 1 );
		if ( len < size )
			size = len;
		content = option->data.string;
	}
	return snprintf ( buf, size, "%s", content );
}

/**
 * Calculate length of a normal DHCP option
 *
 * @v option		DHCP option
 * @ret len		Length (including tag and length field)
 *
 * @c option may not be a @c DHCP_PAD or @c DHCP_END option.
 */
static inline unsigned int dhcp_option_len ( struct dhcp_option *option ) {
	assert ( option->tag != DHCP_PAD );
	assert ( option->tag != DHCP_END );
	return ( option->len + DHCP_OPTION_HEADER_LEN );
}

/**
 * Calculate length of any DHCP option
 *
 * @v option		DHCP option
 * @ret len		Length (including tag and length field)
 */
static inline unsigned int dhcp_any_option_len ( struct dhcp_option *option ) {
	if ( ( option->tag == DHCP_END ) || ( option->tag == DHCP_PAD ) ) {
		return 1;
	} else {
		return dhcp_option_len ( option );
	}
}

/**
 * Find DHCP option within DHCP options block, and its encapsulator (if any)
 *
 * @v options		DHCP options block
 * @v tag		DHCP option tag to search for
 * @ret encapsulator	Encapsulating DHCP option
 * @ret option		DHCP option, or NULL if not found
 *
 * Searches for the DHCP option matching the specified tag within the
 * DHCP option block.  Encapsulated options may be searched for by
 * using DHCP_ENCAP_OPT() to construct the tag value.
 *
 * If the option is encapsulated, and @c encapsulator is non-NULL, it
 * will be filled in with a pointer to the encapsulating option.
 *
 * This routine is designed to be paranoid.  It does not assume that
 * the option data is well-formatted, and so must guard against flaws
 * such as options missing a @c DHCP_END terminator, or options whose
 * length would take them beyond the end of the data block.
 */
static struct dhcp_option *
find_dhcp_option_with_encap ( struct dhcp_option_block *options,
			      unsigned int tag,
			      struct dhcp_option **encapsulator ) {
	unsigned int original_tag __attribute__ (( unused )) = tag;
	struct dhcp_option *option = options->data;
	ssize_t remaining = options->len;
	unsigned int option_len;

	while ( remaining ) {
		/* Calculate length of this option.  Abort processing
		 * if the length is malformed (i.e. takes us beyond
		 * the end of the data block).
		 */
		option_len = dhcp_any_option_len ( option );
		remaining -= option_len;
		if ( remaining < 0 )
			break;
		/* Check for matching tag */
		if ( option->tag == tag ) {
			DBG ( "Found DHCP option %s (length %d) in block %p\n",
			      dhcp_tag_name ( original_tag ), option->len,
			      options );
			return option;
		}
		/* Check for explicit end marker */
		if ( option->tag == DHCP_END )
			break;
		/* Check for start of matching encapsulation block */
		if ( DHCP_IS_ENCAP_OPT ( tag ) &&
		     ( option->tag == DHCP_ENCAPSULATOR ( tag ) ) ) {
			if ( encapsulator )
				*encapsulator = option;
			/* Continue search within encapsulated option block */
			tag = DHCP_ENCAPSULATED ( tag );
			remaining = option->len;
			option = ( void * ) &option->data;
			continue;
		}
		option = ( ( ( void * ) option ) + option_len );
	}
	return NULL;
}

/**
 * Find DHCP option within DHCP options block
 *
 * @v options		DHCP options block, or NULL
 * @v tag		DHCP option tag to search for
 * @ret option		DHCP option, or NULL if not found
 *
 * Searches for the DHCP option matching the specified tag within the
 * DHCP option block.  Encapsulated options may be searched for by
 * using DHCP_ENCAP_OPT() to construct the tag value.
 *
 * If @c options is NULL, all registered option blocks will be
 * searched.  Where multiple option blocks contain the same DHCP
 * option, the option from the highest-priority block will be
 * returned.  (Priority of an options block is determined by the value
 * of the @c DHCP_EB_PRIORITY option within the block, if present; the
 * default priority is zero).
 */
struct dhcp_option * find_dhcp_option ( struct dhcp_option_block *options,
					unsigned int tag ) {
	struct dhcp_option *option;

	if ( options ) {
		return find_dhcp_option_with_encap ( options, tag, NULL );
	} else {
		list_for_each_entry ( options, &option_blocks, list ) {
			if ( ( option = find_dhcp_option ( options, tag ) ) )
				return option;
		}
		return NULL;
	}
}

/**
 * Register DHCP option block
 *
 * @v options		DHCP option block
 *
 * Register a block of DHCP options.
 */
void register_dhcp_options ( struct dhcp_option_block *options ) {
	struct dhcp_option_block *existing;

	/* Determine priority of new block */
	options->priority = find_dhcp_num_option ( options, DHCP_EB_PRIORITY );
	DBG ( "Registering DHCP options block %p with priority %d\n",
	      options, options->priority );

	/* Insert after any existing blocks which have a higher priority */
	list_for_each_entry ( existing, &option_blocks, list ) {
		if ( options->priority > existing->priority )
			break;
	}
	list_add_tail ( &options->list, &existing->list );
}

/**
 * Unregister DHCP option block
 *
 * @v options		DHCP option block
 */
void unregister_dhcp_options ( struct dhcp_option_block *options ) {
	list_del ( &options->list );
}

/**
 * Initialise empty block of DHCP options
 *
 * @v options		Uninitialised DHCP option block
 * @v data		Memory for DHCP option data
 * @v max_len		Length of memory for DHCP option data
 *
 * Populates the DHCP option data with a single @c DHCP_END option and
 * fills in the fields of the @c dhcp_option_block structure.
 */
void init_dhcp_options ( struct dhcp_option_block *options,
			 void *data, size_t max_len ) {
	struct dhcp_option *option;

	options->data = data;
	options->max_len = max_len;
	option = options->data;
	option->tag = DHCP_END;
	options->len = 1;

	DBG ( "DHCP options block %p initialised (data %p max_len %#zx)\n",
	      options, options->data, options->max_len );
}

/**
 * Allocate space for a block of DHCP options
 *
 * @v max_len		Maximum length of option block
 * @ret options		DHCP option block, or NULL
 *
 * Creates a new DHCP option block and populates it with an empty
 * options list.  This call does not register the options block.
 */
struct dhcp_option_block * alloc_dhcp_options ( size_t max_len ) {
	struct dhcp_option_block *options;

	options = malloc ( sizeof ( *options ) + max_len );
	if ( options ) {
		init_dhcp_options ( options, 
				    ( (void *) options + sizeof ( *options ) ),
				    max_len );
	}
	return options;
}

/**
 * Free DHCP options block
 *
 * @v options		DHCP option block
 */
void free_dhcp_options ( struct dhcp_option_block *options ) {
	free ( options );
}

/**
 * Resize a DHCP option
 *
 * @v options		DHCP option block
 * @v option		DHCP option to resize
 * @v encapsulator	Encapsulating option (or NULL)
 * @v old_len		Old length (including header)
 * @v new_len		New length (including header)
 * @ret rc		Return status code
 */
static int resize_dhcp_option ( struct dhcp_option_block *options,
				struct dhcp_option *option,
				struct dhcp_option *encapsulator,
				size_t old_len, size_t new_len ) {
	void *source = ( ( ( void * ) option ) + old_len );
	void *dest = ( ( ( void * ) option ) + new_len );
	void *end = ( options->data + options->max_len );
	ssize_t delta = ( new_len - old_len );
	size_t new_options_len;
	size_t new_encapsulator_len;

	/* Check for sufficient space, and update length fields */
	if ( new_len > DHCP_MAX_LEN )
		return -ENOMEM;
	new_options_len = ( options->len + delta );
	if ( new_options_len > options->max_len )
		return -ENOMEM;
	if ( encapsulator ) {
		new_encapsulator_len = ( encapsulator->len + delta );
		if ( new_encapsulator_len > DHCP_MAX_LEN )
			return -ENOMEM;
		encapsulator->len = new_encapsulator_len;
	}
	options->len = new_options_len;

	/* Move remainder of option data */
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
 * @ret option		DHCP option, or NULL
 *
 * Sets the value of a DHCP option within the options block.  The
 * option may or may not already exist.  Encapsulators will be created
 * (and deleted) as necessary.
 *
 * This call may fail due to insufficient space in the options block.
 * If it does fail, and the option existed previously, the option will
 * be left with its original value.
 */
struct dhcp_option * set_dhcp_option ( struct dhcp_option_block *options,
				       unsigned int tag,
				       const void *data, size_t len ) {
	static const uint8_t empty_encapsulator[] = { DHCP_END };
	struct dhcp_option *option;
	void *insertion_point;
	struct dhcp_option *encapsulator = NULL;
	unsigned int encap_tag = DHCP_ENCAPSULATOR ( tag );
	size_t old_len = 0;
	size_t new_len = ( len ? ( len + DHCP_OPTION_HEADER_LEN ) : 0 );

	/* Return NULL if no options block specified */
	if ( ! options )
		return NULL;

	/* Find old instance of this option, if any */
	option = find_dhcp_option_with_encap ( options, tag, &encapsulator );
	if ( option ) {
		old_len = dhcp_option_len ( option );
		DBG ( "Resizing DHCP option %s from length %d to %d in block "
		      "%p\n", dhcp_tag_name (tag), option->len, len, options );
	} else {
		old_len = 0;
		DBG ( "Creating DHCP option %s (length %d) in block %p\n",
		      dhcp_tag_name ( tag ), len, options );
	}
	
	/* Ensure that encapsulator exists, if required */
	insertion_point = options->data;
	if ( DHCP_IS_ENCAP_OPT ( tag ) ) {
		if ( ! encapsulator )
			encapsulator = set_dhcp_option ( options, encap_tag,
							 empty_encapsulator,
						sizeof ( empty_encapsulator) );
		if ( ! encapsulator )
			return NULL;
		insertion_point = &encapsulator->data;
	}

	/* Create new option if necessary */
	if ( ! option )
		option = insertion_point;
	
	/* Resize option to fit new data */
	if ( resize_dhcp_option ( options, option, encapsulator,
				  old_len, new_len ) != 0 )
		return NULL;

	/* Copy new data into option, if applicable */
	if ( len ) {
		option->tag = tag;
		option->len = len;
		memcpy ( &option->data, data, len );
	}

	/* Delete encapsulator if there's nothing else left in it */
	if ( encapsulator && ( encapsulator->len <= 1 ) )
		set_dhcp_option ( options, encap_tag, NULL, 0 );

	return option;
}

/**
 * Find DHCP option within all registered DHCP options blocks
 *
 * @v tag		DHCP option tag to search for
 * @ret option		DHCP option, or NULL if not found
 *
 * This function exists merely as a notational shorthand for
 * find_dhcp_option() with @c options set to NULL.
 */
struct dhcp_option * find_global_dhcp_option ( unsigned int tag ) {
	return find_dhcp_option ( NULL, tag );
}

/**
 * Find DHCP numerical option, and return its value
 *
 * @v options		DHCP options block
 * @v tag		DHCP option tag to search for
 * @ret value		Numerical value of the option, or 0 if not found
 *
 * This function exists merely as a notational shorthand for a call to
 * find_dhcp_option() followed by a call to dhcp_num_option().  It is
 * not possible to distinguish between the cases "option not found"
 * and "option has a value of zero" using this function; if this
 * matters to you then issue the two constituent calls directly and
 * check that find_dhcp_option() returns a non-NULL value.
 */
unsigned long find_dhcp_num_option ( struct dhcp_option_block *options,
				     unsigned int tag ) {
	return dhcp_num_option ( find_dhcp_option ( options, tag ) );
}

/**
 * Find DHCP numerical option, and return its value
 *
 * @v tag		DHCP option tag to search for
 * @ret value		Numerical value of the option, or 0 if not found
 *
 * This function exists merely as a notational shorthand for a call to
 * find_global_dhcp_option() followed by a call to dhcp_num_option().
 * It is not possible to distinguish between the cases "option not
 * found" and "option has a value of zero" using this function; if
 * this matters to you then issue the two constituent calls directly
 * and check that find_global_dhcp_option() returns a non-NULL value.
 */
unsigned long find_global_dhcp_num_option ( unsigned int tag ) {
	return dhcp_num_option ( find_global_dhcp_option ( tag ) );
}

/**
 * Find DHCP IPv4-address option, and return its value
 *
 * @v options		DHCP options block
 * @v tag		DHCP option tag to search for
 * @v inp		IPv4 address to fill in
 * @ret value		Numerical value of the option, or 0 if not found
 *
 * This function exists merely as a notational shorthand for a call to
 * find_dhcp_option() followed by a call to dhcp_ipv4_option().  It is
 * not possible to distinguish between the cases "option not found"
 * and "option has a value of 0.0.0.0" using this function; if this
 * matters to you then issue the two constituent calls directly and
 * check that find_dhcp_option() returns a non-NULL value.
 */
void find_dhcp_ipv4_option ( struct dhcp_option_block *options,
			     unsigned int tag, struct in_addr *inp ) {
	dhcp_ipv4_option ( find_dhcp_option ( options, tag ), inp );
}

/**
 * Find DHCP IPv4-address option, and return its value
 *
 * @v options		DHCP options block
 * @v tag		DHCP option tag to search for
 * @v inp		IPv4 address to fill in
 * @ret value		Numerical value of the option, or 0 if not found
 *
 * This function exists merely as a notational shorthand for a call to
 * find_dhcp_option() followed by a call to dhcp_ipv4_option().  It is
 * not possible to distinguish between the cases "option not found"
 * and "option has a value of 0.0.0.0" using this function; if this
 * matters to you then issue the two constituent calls directly and
 * check that find_dhcp_option() returns a non-NULL value.
 */
void find_global_dhcp_ipv4_option ( unsigned int tag, struct in_addr *inp ) {
	dhcp_ipv4_option ( find_global_dhcp_option ( tag ), inp );
}

/**
 * Delete DHCP option
 *
 * @v options		DHCP options block
 * @v tag		DHCP option tag
 *
 * This function exists merely as a notational shorthand for a call to
 * set_dhcp_option() with @c len set to zero.
 */
void delete_dhcp_option ( struct dhcp_option_block *options,
			  unsigned int tag ) {
	set_dhcp_option ( options, tag, NULL, 0 );
}
