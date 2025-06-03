#ifndef CONFIG_IOAPI_H
#define CONFIG_IOAPI_H

/** @file
 *
 * I/O API configuration
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <config/defaults.h>

//#undef	PCIAPI_PCBIOS		/* Access via PCI BIOS */
//#define	PCIAPI_DIRECT		/* Direct access via Type 1 accesses */

#include <config/named.h>
#include NAMED_CONFIG(ioapi.h)
#include <config/local/ioapi.h>
#include LOCAL_NAMED_CONFIG(ioapi.h)

#endif /* CONFIG_IOAPI_H */
