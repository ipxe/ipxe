#ifndef NMB_H
#define NMB_H

#include <gpxe/dns.h>

/*
 * NetBIOS name query packets are basically the same as DNS packets,
 * though the resource record format is different.
 *
 */

#define DNS_TYPE_NB		0x20
#define DNS_FLAG_BROADCAST	( 0x01 << 4 )
#define NBNS_UDP_PORT		137

struct dns_rr_info_nb {
	struct dns_rr_info info;
	uint16_t	nb_flags;
	struct in_addr	nb_address;
} __attribute__ (( packed ));

#endif /* NMB_H */
