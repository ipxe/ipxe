#ifndef _GPXE_DHCPPKT_H
#define _GPXE_DHCPPKT_H

/** @file
 *
 * DHCP packets
 *
 */

#include <gpxe/dhcp.h>
#include <gpxe/dhcpopts.h>

/**
 * A DHCP packet
 *
 */
struct dhcp_packet {
	/** The DHCP packet contents */
	struct dhcphdr *dhcphdr;
	/** Maximum length of the DHCP packet buffer */
	size_t max_len;
	/** Used length of the DHCP packet buffer */
	size_t len;
	/** DHCP option blocks */
	struct dhcp_options options;
};

extern int dhcppkt_store ( struct dhcp_packet *dhcppkt, unsigned int tag,
			   const void *data, size_t len );
extern int dhcppkt_fetch ( struct dhcp_packet *dhcppkt, unsigned int tag,
			   void *data, size_t len );
extern void dhcppkt_init ( struct dhcp_packet *dhcppkt, 
			   void *data, size_t len );

#endif /* _GPXE_DHCPPKT_H */
