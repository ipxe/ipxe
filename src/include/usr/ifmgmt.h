#ifndef _USR_IFMGMT_H
#define _USR_IFMGMT_H

/** @file
 *
 * Network interface management
 *
 */

struct net_device;

extern int ifopen ( struct net_device *netdev );
extern void ifclose ( struct net_device *netdev );
extern void ifstat ( struct net_device *netdev );

#endif /* _USR_IFMGMT_H */
