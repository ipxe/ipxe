#ifndef _GPXE_DHCPPKT_H
#define _GPXE_DHCPPKT_H

/** @file
 *
 * DHCP packets
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/dhcp.h>
#include <gpxe/dhcpopts.h>
#include <gpxe/refcnt.h>

/**
 * A DHCP packet
 *
 */
struct dhcp_packet {
	/** Reference counter */
	struct refcnt refcnt;
	/** The DHCP packet contents */
	struct dhcphdr *dhcphdr;
	/** Maximum length of the DHCP packet buffer */
	size_t max_len;
	/** Used length of the DHCP packet buffer */
	size_t len;
	/** DHCP options */
	struct dhcp_options options;
	/** Settings interface */
	struct settings settings;
};

/**
 * Increment reference count on DHCP packet
 *
 * @v dhcppkt		DHCP packet
 * @ret dhcppkt		DHCP packet
 */
static inline __attribute__ (( always_inline )) struct dhcp_packet *
dhcppkt_get ( struct dhcp_packet *dhcppkt ) {
	ref_get ( &dhcppkt->refcnt );
	return dhcppkt;
}

/**
 * Decrement reference count on DHCP packet
 *
 * @v dhcppkt		DHCP packet
 */
static inline __attribute__ (( always_inline )) void
dhcppkt_put ( struct dhcp_packet *dhcppkt ) {
	ref_put ( &dhcppkt->refcnt );
}

extern int dhcppkt_store ( struct dhcp_packet *dhcppkt, unsigned int tag,
			   const void *data, size_t len );
extern int dhcppkt_fetch ( struct dhcp_packet *dhcppkt, unsigned int tag,
			   void *data, size_t len );
extern void dhcppkt_init ( struct dhcp_packet *dhcppkt, 
			   struct dhcphdr *data, size_t len );

#endif /* _GPXE_DHCPPKT_H */
