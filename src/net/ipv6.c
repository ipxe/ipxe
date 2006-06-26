#include <errno.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>
#include <gpxe/in.h>

/**
 * Transmit IP6 packets
 *
 * Placeholder to allow linking. The function should be placed in net/ipv6.c
 */
int ipv6_tx ( struct pk_buff *pkb __unused, uint16_t trans_proto __unused,
	      struct in6_addr *dest __unused) {
	return -ENOSYS;
}

/**
 * Process incoming IP6 packets
 *
 * Placeholder function. Should rewrite in net/ipv6.c
 */
void ipv6_rx ( struct pk_buff *pkb __unused,
	       struct net_device *netdev __unused,
	       const void *ll_source __unused ) {
}
