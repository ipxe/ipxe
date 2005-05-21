#ifndef PXE_TYPES_H
#define PXE_TYPES_H

/** @addtogroup pxe Preboot eXecution Environment (PXE) API
 * @{
 */

/** @file
 *
 * PXE data types
 *
 */

#include "stdint.h"
#include "pxe_addr.h" /* Architecture-specific PXE definitions */
#include "errno.h" /* PXE status codes */

/** @defgroup pxe_types PXE data types
 *
 * These definitions are based on Table 1-1 ("Data Type Definitions")
 * in the Intel PXE specification version 2.1.  They have been
 * generalised to non-x86 architectures where possible.
 *
 * @{
 */

/** An 8-bit unsigned integer */
typedef uint8_t UINT8;

/** A 16-bit unsigned integer */
typedef uint16_t UINT16;

/** A 32-bit unsigned integer */
typedef uint32_t UINT32;

/** A PXE exit code.
 *
 * Permitted values are PXENV_EXIT_SUCCESS and PXENV_EXIT_FAILURE.
 *
 */
typedef uint16_t PXENV_EXIT;

/** A PXE status code.
 *
 * Status codes are defined in errno.h.
 *
 */
typedef uint16_t PXENV_STATUS;

/** An IP address.
 *
 * This is an IPv4 address in host byte order.
 *
 */
typedef uint32_t IP4;

/** A UDP port.
 *
 * Note that this is in network (big-endian) byte order.
 *
 */
typedef uint16_t UDP_PORT;

/** Maximum length of a MAC address */
#define MAC_ADDR_LEN 16

/** A MAC address */
typedef uint8_t MAC_ADDR[MAC_ADDR_LEN];

/** A physical address.
 *
 * For x86, this is a 32-bit physical address, and is therefore
 * limited to the low 4GB.
 *
 */
typedef physaddr_t ADDR32;

#ifndef HAVE_ARCH_SEGSEL
/** A segment selector.
 *
 * For x86, this is a real mode segment (0x0000-0xffff), or a
 * protected-mode segment selector, such as could be loaded into a
 * segment register.
 *
 */
typedef uint16_t SEGSEL;
#endif

#ifndef HAVE_ARCH_OFF16
/** An offset within a segment identified by #SEGSEL */
typedef uint16_t OFF16;
#endif

/** A segment:offset address */
typedef struct s_SEGOFF16 {
	OFF16	offset;			/**< Offset within the segment */
	SEGSEL	segment;		/**< Segment selector */
} SEGOFF16 __attribute__ (( packed ));

/** A segment descriptor */
typedef struct s_SEGDESC {
	SEGSEL	segment_address;	/**< Segment selector */
	ADDR32	physical_address;	/**< Base address of the segment */
	OFF16	seg_size;		/**< Size of the segment */
} SEGDESC __attribute__ (( packed ));

/** @} */ /* defgroup */


/** @} */ /* addtogroup */

#endif /* PXE_TYPES_H */
