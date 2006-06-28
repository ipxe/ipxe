#ifndef _GPXE_DHCP_H
#define _GPXE_DHCP_H

/** @file
 *
 * Dynamic Host Configuration Protocol
 *
 */

#include <stdint.h>
#include <gpxe/list.h>

/** Construct a tag value for an encapsulated option
 *
 * This tag value can be passed to Etherboot functions when searching
 * for DHCP options in order to search for a tag within an
 * encapsulated options block.
 */
#define DHCP_ENCAP_OPT( encapsulator, encapsulated ) \
	( ( (encapsulator) << 8 ) | (encapsulated) )
/** Extract encapsulating option block tag from encapsulated tag value */
#define DHCP_ENCAPSULATOR( encap_opt ) ( (encap_opt) >> 8 )
/** Extract encapsulated option tag from encapsulated tag value */
#define DHCP_ENCAPSULATED( encap_opt ) ( (encap_opt) & 0xff )

/**
 * @defgroup dhcpopts DHCP option tags
 * @{
 */

#define DHCP_PAD 0
#define DHCP_END 255

#define DHCP_EB_ENCAP 175

#define DHCP_EB_PRIORITY DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 1 )

/** @} */

/**
 * A DHCP option
 *
 * DHCP options consist of a mandatory tag, a length field that is
 * mandatory for all options except @c DHCP_PAD and @c DHCP_END, and a
 * payload.  
 */
struct dhcp_option {
	/** Tag
	 *
	 * Must be a @c DHCP_XXX value.
	 */
	uint8_t tag;
	/** Length
	 *
	 * This is the length of the data field (i.e. excluding the
	 * tag and length fields).  For the two tags @c DHCP_PAD and
	 * @c DHCP_END, the length field is implicitly zero and is
	 * also missing, i.e. these DHCP options are only a single
	 * byte in length.
	 */
	uint8_t len;
	/** Option data
	 *
	 * Interpretation of the content is entirely dependent upon
	 * the tag.  For fields containing a multi-byte integer, the
	 * field is defined to be in network-endian order (unless you
	 * are Intel and feel like violating the spec for fun).
	 */
	union {
		uint8_t byte;
		uint16_t word;
		uint32_t dword;
		uint8_t bytes[0];
	} data;
} __attribute__ (( packed ));

/** A DHCP options block */
struct dhcp_option_block {
	/** List of option blocks */
	struct list_head list;
	/** Option block raw data */
	void *data;
	/** Option block length */
	size_t len;
};

extern unsigned long dhcp_num_option ( struct dhcp_option *option );
extern struct dhcp_option * find_dhcp_option ( unsigned int tag,
					   struct dhcp_option_block *options );

/**
 * Find DHCP numerical option, and return its value
 *
 * @v tag		DHCP option tag to search for
 * @v options		DHCP options block
 * @ret value		Numerical value of the option, or 0 if not found
 *
 * This function exists merely as a notational shorthand for a call to
 * find_dhcp_option() followed by a call to dhcp_num_option().  It is
 * not possible to distinguish between the cases "option not found"
 * and "option has a value of zero" using this function; if this
 * matters to you then issue the two constituent calls directly and
 * check that find_dhcp_option() returns a non-NULL value.
 */
static inline unsigned long
find_dhcp_num_option ( unsigned int tag, struct dhcp_option_block *options ) {
	return dhcp_num_option ( find_dhcp_option ( tag, options ) );
}

#endif /* _GPXE_DHCP_H */
