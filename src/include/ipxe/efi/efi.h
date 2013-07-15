#ifndef _IPXE_EFI_H
#define _IPXE_EFI_H

/** @file
 *
 * EFI API
 *
 * The intention is to include near-verbatim copies of the EFI headers
 * required by iPXE.  This is achieved using the import.pl script in
 * this directory.  Run the import script to update the local copies
 * of the headers:
 *
 *     ./import.pl /path/to/edk2/edk2
 *
 * where /path/to/edk2/edk2 is the path to your local checkout of the
 * EFI Development Kit.
 *
 * Note that import.pl will modify any #include lines in each imported
 * header to reflect its new location within the iPXE tree.  It will
 * also tidy up the file by removing carriage return characters and
 * trailing whitespace.
 */

FILE_LICENCE ( GPL2_OR_LATER );

/* EFI headers rudely redefine NULL */
#undef NULL

/* EFI headers expect ICC to define __GNUC__ */
#if defined ( __ICC ) && ! defined ( __GNUC__ )
#define __GNUC__ 1
#endif

/* EFI headers think your compiler uses the MS ABI by default on X64 */
#if __x86_64__
#define EFIAPI __attribute__((ms_abi))
#endif

/* EFI headers assume regparm(0) on i386, but that is not the case for iPXE */
#if __i386__
#define EFIAPI __attribute__((cdecl,regparm(0)))
#endif

/* Include the top-level EFI header files */
#include <ipxe/efi/Uefi.h>
#include <ipxe/efi/PiDxe.h>
#include <ipxe/efi/Protocol/LoadedImage.h>

/* Reset any trailing #pragma pack directives */
#pragma pack(1)
#pragma pack()

#include <ipxe/tables.h>
#include <ipxe/uuid.h>

/** An EFI protocol used by iPXE */
struct efi_protocol {
	/** GUID */
	EFI_GUID guid;
	/** Variable containing pointer to protocol structure */
	void **protocol;
};

/** EFI protocol table */
#define EFI_PROTOCOLS __table ( struct efi_protocol, "efi_protocols" )

/** Declare an EFI protocol used by iPXE */
#define __efi_protocol __table_entry ( EFI_PROTOCOLS, 01 )

/** Declare an EFI protocol to be required by iPXE
 *
 * @v _protocol		EFI protocol name
 * @v _ptr		Pointer to protocol instance
 */
#define EFI_REQUIRE_PROTOCOL( _protocol, _ptr )				     \
	struct efi_protocol __ ## _protocol __efi_protocol = {		     \
		.guid = _protocol ## _GUID,				     \
		.protocol = ( ( void ** ) ( void * )			     \
			      ( ( (_ptr) == ( ( _protocol ** ) (_ptr) ) ) ?  \
				(_ptr) : (_ptr) ) ),			     \
	}

/** An EFI configuration table used by iPXE */
struct efi_config_table {
	/** GUID */
	EFI_GUID guid;
	/** Variable containing pointer to configuration table */
	void **table;
	/** Table is required for operation */
	int required;
};

/** EFI configuration table table */
#define EFI_CONFIG_TABLES \
	__table ( struct efi_config_table, "efi_config_tables" )

/** Declare an EFI configuration table used by iPXE */
#define __efi_config_table __table_entry ( EFI_CONFIG_TABLES, 01 )

/** Declare an EFI configuration table to be used by iPXE
 *
 * @v _table		EFI configuration table name
 * @v _ptr		Pointer to configuration table
 * @v _required		Table is required for operation
 */
#define EFI_USE_TABLE( _table, _ptr, _required )			     \
	struct efi_config_table __ ## _table __efi_config_table = {	     \
		.guid = _table ## _GUID,				     \
		.table = ( ( void ** ) ( void * ) (_ptr) ),		     \
		.required = (_required),				     \
	}

/**
 * Convert an iPXE status code to an EFI status code
 *
 * @v rc		iPXE status code
 * @ret efirc		EFI status code
 */
#define EFIRC( rc ) ERRNO_TO_PLATFORM ( -(rc) )

/**
 * Convert an EFI status code to an iPXE status code
 *
 * @v efirc		EFI status code
 * @ret rc		iPXE status code (before negation)
 */
#define EEFI( efirc ) EPLATFORM ( EINFO_EPLATFORM, efirc )

extern EFI_HANDLE efi_image_handle;
extern EFI_LOADED_IMAGE_PROTOCOL *efi_loaded_image;
extern EFI_DEVICE_PATH_PROTOCOL *efi_loaded_image_path;
extern EFI_SYSTEM_TABLE *efi_systab;

extern const char * efi_guid_ntoa ( EFI_GUID *guid );

extern void dbg_efi_protocols ( EFI_HANDLE handle );
extern void dbg_efi_devpath ( EFI_DEVICE_PATH_PROTOCOL *path );

#define DBG_EFI_PROTOCOLS_IF( level, handle ) do {		\
		if ( DBG_ ## level ) {				\
			dbg_efi_protocols ( handle );		\
		}						\
	} while ( 0 )

#define DBG_EFI_DEVPATH_IF( level, path ) do {			\
		if ( DBG_ ## level ) {				\
			dbg_efi_devpath ( path );		\
		}						\
	} while ( 0 )

#define DBGC_EFI_PROTOCOLS_IF( level, id, ... ) do {		\
		DBG_AC_IF ( level, id );			\
		DBG_EFI_PROTOCOLS_IF ( level, __VA_ARGS__ );	\
		DBG_DC_IF ( level );				\
	} while ( 0 )

#define DBGC_EFI_DEVPATH_IF( level, id, ... ) do {		\
		DBG_AC_IF ( level, id );			\
		DBG_EFI_DEVPATH_IF ( level, __VA_ARGS__ );	\
		DBG_DC_IF ( level );				\
	} while ( 0 )

#define DBGC_EFI_PROTOCOLS( ... )				\
	DBGC_EFI_PROTOCOLS_IF( LOG, ##__VA_ARGS__ )

#define DBGC_EFI_DEVPATH( ... )					\
	DBGC_EFI_DEVPATH_IF( LOG, ##__VA_ARGS__ )

extern EFI_STATUS efi_init ( EFI_HANDLE image_handle,
			     EFI_SYSTEM_TABLE *systab );

#endif /* _IPXE_EFI_H */
