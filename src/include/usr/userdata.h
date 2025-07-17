#ifndef _USR_USERDATA_H
#define _USR_USERDATA_H

/** @file
 *
 * Userdata
 *
 */

#include <ipxe/image.h>

extern int get_userdata ( int use_ipv6, struct image **image );
extern int execute_userdata ( struct image *image );

#endif /* _USR_USERDATA_H */
