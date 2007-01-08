#ifndef _UNDINET_H
#define _UNDINET_H

/** @file
 *
 * UNDI network device driver
 *
 */

struct undi_device;

extern int undinet_probe ( struct undi_device *undi );
extern void undinet_remove ( struct undi_device *undi );

#endif /* _UNDINET_H */
