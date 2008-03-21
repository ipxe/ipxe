#ifndef _GPXE_DHCPPKT_H
#define _GPXE_DHCPPKT_H

/** @file
 *
 * DHCP packets
 *
 */

#include <gpxe/dhcp.h>
#include <gpxe/dhcpopts.h>
#include <gpxe/settings.h>

/**
 * A DHCP packet
 *
 */
struct dhcp_packet {
	/** Settings block */
	struct settings settings;
	/** The DHCP packet contents */
	struct dhcphdr *dhcphdr;
	/** Maximum length of the DHCP packet buffer */
	size_t max_len;
	/** Used length of the DHCP packet buffer */
	size_t len;
	/** DHCP option blocks */
	struct dhcp_options options;
};

extern void dhcppkt_init ( struct dhcp_packet *dhcppkt, struct refcnt *refcnt,
			   void *data, size_t len );

#endif /* _GPXE_DHCPPKT_H */
