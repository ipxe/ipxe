#ifndef _GPXE_DHCPOPTS_H
#define _GPXE_DHCPOPTS_H

/** @file
 *
 * DHCP options
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

/** A DHCP options block */
struct dhcp_options {
	/** Option block raw data */
	void *data;
	/** Option block length */
	size_t len;
	/** Option block maximum length */
	size_t max_len;
};

extern int dhcpopt_store ( struct dhcp_options *options, unsigned int tag,
			   const void *data, size_t len );
extern int dhcpopt_extensible_store ( struct dhcp_options *options,
				      unsigned int tag,
				      const void *data, size_t len );
extern int dhcpopt_fetch ( struct dhcp_options *options, unsigned int tag,
			   void *data, size_t len );
extern void dhcpopt_init ( struct dhcp_options *options,
			   void *data, size_t max_len );

#endif /* _GPXE_DHCPOPTS_H */
