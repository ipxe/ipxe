#ifndef _GPXE_LINUX_COMPAT_H
#define _GPXE_LINUX_COMPAT_H

/** @file
 *
 * Linux code compatibility
 *
 * This file exists to ease the building of Linux source code within
 * gPXE.  This is intended to facilitate quick testing; it is not
 * intended to be a substitute for proper porting.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <byteswap.h>
#include <gpxe/bitops.h>

#define __init
#define __exit
#define __initdata
#define __exitdata
#define printk printf

#endif /* _GPXE_LINUX_COMPAT_H */
