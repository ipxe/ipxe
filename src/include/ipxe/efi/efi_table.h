#ifndef _IPXE_EFI_TABLE_H
#define _IPXE_EFI_TABLE_H

/** @file
 *
 * EFI configuration tables
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/efi/efi.h>

/** An installable EFI configuration table type */
struct efi_table {
	/** Table GUID */
	EFI_GUID *guid;
	/**
	 * Determine length of table
	 *
	 * @v data		Configuration table data (presumed valid)
	 * @ret len		Length of table
	 *
	 * EFI does not record the length of installed configuration
	 * tables.  Consumers must understand the specific type of
	 * table in order to be able to determine its length from the
	 * contents.
	 */
	size_t ( * len ) ( const void *data );
};

extern void * efi_find_table ( EFI_GUID *guid );
extern int efi_install_table ( struct efi_table *table, const void *data,
			       void **backup );
extern int efi_uninstall_table ( struct efi_table *table, void **backup );

#endif /* _IPXE_EFI_TABLE_H */
