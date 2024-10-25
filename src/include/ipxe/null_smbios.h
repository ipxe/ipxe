#ifndef _IPXE_NULL_SMBIOS_H
#define _IPXE_NULL_SMBIOS_H

/** @file
 *
 * Null SMBIOS API
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef SMBIOS_NULL
#define SMBIOS_PREFIX_null
#else
#define SMBIOS_PREFIX_null __null_
#endif

#endif /* _IPXE_NULL_SMBIOS_H */
