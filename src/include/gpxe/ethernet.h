#ifndef _GPXE_ETHERNET_H
#define _GPXE_ETHERNET_H

/** @file
 *
 * Ethernet protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

extern void eth_init_addr ( const void *hw_addr, void *ll_addr );
extern const char * eth_ntoa ( const void *ll_addr );
extern int eth_mc_hash ( unsigned int af, const void *net_addr,
			 void *ll_addr );
extern int eth_eth_addr ( const void *ll_addr, void *eth_addr );
extern struct net_device * alloc_etherdev ( size_t priv_size );

#endif /* _GPXE_ETHERNET_H */
