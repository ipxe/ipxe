#ifndef _GPXE_NVO_H
#define _GPXE_NVO_H

/** @file
 *
 * Non-volatile stored options
 *
 */

#include <stdint.h>

struct nvs_device;
struct dhcp_option_block;

/**
 * A fragment of a non-volatile storage device used for stored options
 */
struct nvo_fragment {
	/** Starting address of fragment within NVS device */
	unsigned int address;
	/** Length of fragment */
	size_t len;
};

/**
 * A block of non-volatile stored options
 */
struct nvo_block {
	/** Underlying non-volatile storage device */
	struct nvs_device *nvs;
	/** List of option-containing fragments
	 *
	 * The list is terminated by a fragment with a length of zero.
	 */
	struct nvo_fragment *fragments;
	/** Total length of all fragments
	 *
	 * This field is filled in by nvo_register().
	 */
	size_t total_len;
	/** DHCP options block */
	struct dhcp_option_block *options;
};

extern int nvo_register ( struct nvo_block *nvo );
extern int nvo_save ( struct nvo_block *nvo );
extern void nvo_unregister ( struct nvo_block *nvo );

#endif /* _GPXE_NVO_H */
