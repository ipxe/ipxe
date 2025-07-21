#ifndef _DWUSB_H
#define _DWUSB_H

/** @file
 *
 * Synopsys DesignWare USB3 host controller driver
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/xhci.h>

/** Global core control register */
#define DWUSB_GCTL 0xc110
#define DWUSB_GCTL_PRTDIR( x )	( (x) << 12 )	/**< Port direction */
#define DWUSB_GCTL_PRTDIR_HOST \
	DWUSB_GCTL_PRTDIR ( 1 )			/**< Operate as a host */
#define DWUSB_GCTL_PRTDIR_MASK \
	DWUSB_GCTL_PRTDIR ( 3 )			/**< Port direction mask */
#define DWUSB_GCTL_RESET	0x00000800	/**< Core soft reset */

#endif /* _DWUSB_H */
