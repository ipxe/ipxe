#ifndef _GPXE_NVS_H
#define _GPXE_NVS_H

/** @file
 *
 * Non-volatile storage
 *
 */

#include <stdint.h>

struct nvs_operations;

struct nvs_device {
	struct dhcp_option_block *options;
	size_t len;
	struct nvs_operations *op;
};

struct nvs_operations {
	int ( * read ) ( struct nvs_device *nvs, unsigned int offset,
			 void *data, size_t len );
	int ( * write ) ( struct nvs_device *nvs, unsigned int offset,
			  const void *data, size_t len );
};

extern int nvs_register ( struct nvs_device *nvs );
extern void nvs_unregister ( struct nvs_device *nvs );

#endif /* _GPXE_NVS_H */
