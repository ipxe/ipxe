#ifndef _IP_H
#define _IP_H

/** @file
 *
 * IP protocol
 *
 * This file defines the gPXE IP API.
 *
 */

#include <gpxe/in.h>

extern void set_ipaddr ( struct in_addr address );
extern void set_netmask ( struct in_addr address );
extern void set_gateway ( struct in_addr address );
extern void init_tcpip ( void );
extern void run_tcpip ( void );

#endif /* _IP_H */
