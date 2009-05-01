#ifndef _GPXE_NVO_H
#define _GPXE_NVO_H

/** @file
 *
 * Non-volatile stored options
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <gpxe/dhcpopts.h>
#include <gpxe/settings.h>

struct nvs_device;
struct refcnt;

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
	/** Settings block */
	struct settings settings;
	/** Underlying non-volatile storage device */
	struct nvs_device *nvs;
	/** List of option-containing fragments
	 *
	 * The list is terminated by a fragment with a length of zero.
	 */
	struct nvo_fragment *fragments;
	/** Total length of option-containing fragments */
	size_t total_len;
	/** Option-containing data */
	void *data;
	/** DHCP options block */
	struct dhcp_options dhcpopts;
};

extern void nvo_init ( struct nvo_block *nvo, struct nvs_device *nvs,
		       struct nvo_fragment *fragments, struct refcnt *refcnt );
extern int register_nvo ( struct nvo_block *nvo, struct settings *parent );
extern void unregister_nvo ( struct nvo_block *nvo );

#endif /* _GPXE_NVO_H */
