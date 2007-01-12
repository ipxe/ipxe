#ifndef _USR_DHCPMGMT_H
#define _USR_DHCPMGMT_H

/** @file
 *
 * DHCP management
 *
 */

struct net_device;

int dhcp ( struct net_device *netdev );

#endif /* _USR_DHCPMGMT_H */
