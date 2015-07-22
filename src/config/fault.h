#ifndef CONFIG_FAULT_H
#define CONFIG_FAULT_H

/** @file
 *
 * Fault injection
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <config/defaults.h>

/* Drop every N transmitted or received network packets */
#define	NETDEV_DISCARD_RATE 0

#include <config/local/fault.h>

#endif /* CONFIG_FAULT_H */
