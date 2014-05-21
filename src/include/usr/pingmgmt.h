#ifndef _USR_PINGMGMT_H
#define _USR_PINGMGMT_H

/** @file
 *
 * ICMP ping management
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

extern int ping ( const char *hostname, unsigned long timeout, size_t len );

#endif /* _USR_PINGMGMT_H */
