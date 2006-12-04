#ifndef _GPXE_NVO_H
#define _GPXE_NVO_H

/** @file
 *
 * Non-volatile stored options
 *
 */

struct nvs_device;
struct dhcp_option_block;

struct nvs_options {
	struct nvs_device *nvs;
	struct dhcp_option_block *options;
};

extern int nvo_register ( struct nvs_options *nvo );
extern void nvo_unregister ( struct nvs_options *nvo );

#endif /* _GPXE_NVO_H */
