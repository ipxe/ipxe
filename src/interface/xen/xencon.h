#ifndef _XENCON_H
#define _XENCON_H

/** @file
 *
 * Xen Console driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

void xencon_late_init(struct xen_hypervisor *xen);
void xencon_uninit(void);
#endif
