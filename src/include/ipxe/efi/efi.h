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

/* EFI headers redefine ARRAY_SIZE */
#undef ARRAY_SIZE

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

/* EFI headers define EFI_HANDLE as a void pointer, which renders type
 * checking somewhat useless.  Work around this bizarre sabotage
 * attempt by redefining EFI_HANDLE as a pointer to an anonymous
 * structure.
 */
#define EFI_HANDLE STUPID_EFI_HANDLE
#include <ipxe/efi/Uefi/UefiBaseType.h>
#undef EFI_HANDLE
typedef struct {} *EFI_HANDLE;

/* Include the top-level EFI header files */
#include <ipxe/efi/Uefi.h>
#include <ipxe/efi/PiDxe.h>
#include <ipxe/efi/Protocol/LoadedImage.h>

/* Reset any trailing #pragma pack directives */
#pragma pack(1)
#pragma pack()

#include <ipxe/tables.h>
#include <ipxe/uuid.h>
#include <ipxe/version.h>
#include <ipxe/profile.h>

/** An EFI saved task priority level */
struct efi_saved_tpl {
	/** Current external TPL */
	EFI_TPL current;
	/** Previous external TPL */
	EFI_TPL previous;
};

/** An EFI protocol used by iPXE */
struct efi_protocol {
	/** GUID */
	EFI_GUID guid;
	/** Variable containing pointer to protocol structure */
	void **protocol;
	/** Protocol is required */
	int required;
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
		.required = 1,						     \
	}

/** Declare an EFI protocol to be requested by iPXE
 *
 * @v _protocol		EFI protocol name
 * @v _ptr		Pointer to protocol instance
 */
#define EFI_REQUEST_PROTOCOL( _protocol, _ptr )				     \
	struct efi_protocol __ ## _protocol __efi_protocol = {		     \
		.guid = _protocol ## _GUID,				     \
		.protocol = ( ( void ** ) ( void * )			     \
			      ( ( (_ptr) == ( ( _protocol ** ) (_ptr) ) ) ?  \
				(_ptr) : (_ptr) ) ),			     \
		.required = 0,						     \
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

extern EFI_GUID efi_absolute_pointer_protocol_guid;
extern EFI_GUID efi_acpi_table_protocol_guid;
extern EFI_GUID efi_apple_net_boot_protocol_guid;
extern EFI_GUID efi_arp_protocol_guid;
extern EFI_GUID efi_arp_service_binding_protocol_guid;
extern EFI_GUID efi_block_io_protocol_guid;
extern EFI_GUID efi_block_io2_protocol_guid;
extern EFI_GUID efi_bus_specific_driver_override_protocol_guid;
extern EFI_GUID efi_component_name_protocol_guid;
extern EFI_GUID efi_component_name2_protocol_guid;
extern EFI_GUID efi_console_control_protocol_guid;
extern EFI_GUID efi_device_path_protocol_guid;
extern EFI_GUID efi_dhcp4_protocol_guid;
extern EFI_GUID efi_dhcp4_service_binding_protocol_guid;
extern EFI_GUID efi_disk_io_protocol_guid;
extern EFI_GUID efi_driver_binding_protocol_guid;
extern EFI_GUID efi_graphics_output_protocol_guid;
extern EFI_GUID efi_hii_config_access_protocol_guid;
extern EFI_GUID efi_hii_font_protocol_guid;
extern EFI_GUID efi_ip4_protocol_guid;
extern EFI_GUID efi_ip4_config_protocol_guid;
extern EFI_GUID efi_ip4_service_binding_protocol_guid;
extern EFI_GUID efi_load_file_protocol_guid;
extern EFI_GUID efi_load_file2_protocol_guid;
extern EFI_GUID efi_loaded_image_protocol_guid;
extern EFI_GUID efi_loaded_image_device_path_protocol_guid;
extern EFI_GUID efi_managed_network_protocol_guid;
extern EFI_GUID efi_managed_network_service_binding_protocol_guid;
extern EFI_GUID efi_mtftp4_protocol_guid;
extern EFI_GUID efi_mtftp4_service_binding_protocol_guid;
extern EFI_GUID efi_nii_protocol_guid;
extern EFI_GUID efi_nii31_protocol_guid;
extern EFI_GUID efi_pci_io_protocol_guid;
extern EFI_GUID efi_pci_root_bridge_io_protocol_guid;
extern EFI_GUID efi_pxe_base_code_protocol_guid;
extern EFI_GUID efi_serial_io_protocol_guid;
extern EFI_GUID efi_simple_file_system_protocol_guid;
extern EFI_GUID efi_simple_network_protocol_guid;
extern EFI_GUID efi_simple_pointer_protocol_guid;
extern EFI_GUID efi_simple_text_input_protocol_guid;
extern EFI_GUID efi_simple_text_input_ex_protocol_guid;
extern EFI_GUID efi_simple_text_output_protocol_guid;
extern EFI_GUID efi_tcg_protocol_guid;
extern EFI_GUID efi_tcp4_protocol_guid;
extern EFI_GUID efi_tcp4_service_binding_protocol_guid;
extern EFI_GUID efi_tree_protocol_guid;
extern EFI_GUID efi_udp4_protocol_guid;
extern EFI_GUID efi_udp4_service_binding_protocol_guid;
extern EFI_GUID efi_uga_draw_protocol_guid;
extern EFI_GUID efi_unicode_collation_protocol_guid;
extern EFI_GUID efi_usb_hc_protocol_guid;
extern EFI_GUID efi_usb2_hc_protocol_guid;
extern EFI_GUID efi_usb_io_protocol_guid;
extern EFI_GUID efi_vlan_config_protocol_guid;

extern EFI_GUID efi_file_info_id;
extern EFI_GUID efi_file_system_info_id;

extern EFI_HANDLE efi_image_handle;
extern EFI_LOADED_IMAGE_PROTOCOL *efi_loaded_image;
extern EFI_DEVICE_PATH_PROTOCOL *efi_loaded_image_path;
extern EFI_SYSTEM_TABLE *efi_systab;
extern EFI_TPL efi_external_tpl;
extern int efi_shutdown_in_progress;

extern const __attribute__ (( pure )) char *
efi_guid_ntoa ( CONST EFI_GUID *guid );
extern const __attribute__ (( pure )) char *
efi_locate_search_type_name ( EFI_LOCATE_SEARCH_TYPE search_type );
extern const __attribute__ (( pure )) char *
efi_open_attributes_name ( unsigned int attributes );
extern const __attribute__ (( pure )) char *
efi_devpath_text ( EFI_DEVICE_PATH_PROTOCOL *path );
extern const __attribute__ (( pure )) char *
efi_handle_name ( EFI_HANDLE handle );

extern void dbg_efi_opener ( EFI_HANDLE handle, EFI_GUID *protocol,
			     EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opener );
extern void dbg_efi_openers ( EFI_HANDLE handle, EFI_GUID *protocol );
extern void dbg_efi_protocols ( EFI_HANDLE handle );

#define DBG_EFI_OPENER_IF( level, handle, protocol,		\
			   opener ) do {			\
		if ( DBG_ ## level ) {				\
			dbg_efi_opener ( handle, protocol,	\
					 opener );		\
		}						\
	} while ( 0 )

#define DBG_EFI_OPENERS_IF( level, handle, protocol ) do {	\
		if ( DBG_ ## level ) {				\
			dbg_efi_openers ( handle, protocol );	\
		}						\
	} while ( 0 )

#define DBG_EFI_PROTOCOLS_IF( level, handle ) do {		\
		if ( DBG_ ## level ) {				\
			dbg_efi_protocols ( handle );		\
		}						\
	} while ( 0 )

#define DBGC_EFI_OPENER_IF( level, id, ... ) do {		\
		DBG_AC_IF ( level, id );			\
		DBG_EFI_OPENER_IF ( level, __VA_ARGS__ );	\
		DBG_DC_IF ( level );				\
	} while ( 0 )

#define DBGC_EFI_OPENERS_IF( level, id, ... ) do {		\
		DBG_AC_IF ( level, id );			\
		DBG_EFI_OPENERS_IF ( level, __VA_ARGS__ );	\
		DBG_DC_IF ( level );				\
	} while ( 0 )

#define DBGC_EFI_PROTOCOLS_IF( level, id, ... ) do {		\
		DBG_AC_IF ( level, id );			\
		DBG_EFI_PROTOCOLS_IF ( level, __VA_ARGS__ );	\
		DBG_DC_IF ( level );				\
	} while ( 0 )

#define DBGC_EFI_OPENER( ... )					\
	DBGC_EFI_OPENER_IF ( LOG, ##__VA_ARGS__ )
#define DBGC_EFI_OPENERS( ... )					\
	DBGC_EFI_OPENERS_IF ( LOG, ##__VA_ARGS__ )
#define DBGC_EFI_PROTOCOLS( ... )				\
	DBGC_EFI_PROTOCOLS_IF ( LOG, ##__VA_ARGS__ )

#define DBGC2_EFI_OPENER( ... )					\
	DBGC_EFI_OPENER_IF ( EXTRA, ##__VA_ARGS__ )
#define DBGC2_EFI_OPENERS( ... )				\
	DBGC_EFI_OPENERS_IF ( EXTRA, ##__VA_ARGS__ )
#define DBGC2_EFI_PROTOCOLS( ... )				\
	DBGC_EFI_PROTOCOLS_IF ( EXTRA, ##__VA_ARGS__ )

#define DBGCP_EFI_OPENER( ... )					\
	DBGC_EFI_OPENER_IF ( PROFILE, ##__VA_ARGS__ )
#define DBGCP_EFI_OPENERS( ... )				\
	DBGC_EFI_OPENERS_IF ( PROFILE, ##__VA_ARGS__ )
#define DBGCP_EFI_PROTOCOLS( ... )				\
	DBGC_EFI_PROTOCOLS_IF ( PROFILE, ##__VA_ARGS__ )

/* Allow for EFI-only interface operations */
#ifdef PLATFORM_efi
#define EFI_INTF_OP INTF_OP
#else
#define EFI_INTF_OP UNUSED_INTF_OP
#endif

extern unsigned long __stack_chk_guard;
extern unsigned long efi_stack_cookie ( EFI_HANDLE handle );
extern void __stack_chk_fail ( void );

/**
 * Initialise stack cookie
 *
 * @v handle		Image handle
 */
static inline __attribute__ (( always_inline )) void
efi_init_stack_guard ( EFI_HANDLE handle ) {

	/* The calling function must not itself use stack protection,
	 * since the change in the stack guard value would trigger a
	 * false positive.
	 *
	 * There is unfortunately no way to annotate a function to
	 * exclude the use of stack protection.  We must therefore
	 * rely on correctly anticipating the compiler's decision on
	 * the use of stack protection.
	 *
	 * The calculation of the stack cookie value deliberately
	 * takes the address of a stack variable (to provide an
	 * additional source of entropy).  This operation would
	 * trigger the application of stack protection to the calling
	 * function, and so must be externalised.
	 */
	__stack_chk_guard = efi_stack_cookie ( handle );
}

extern EFI_STATUS efi_init ( EFI_HANDLE image_handle,
			     EFI_SYSTEM_TABLE *systab );
extern void efi_raise_tpl ( struct efi_saved_tpl *tpl );
extern void efi_restore_tpl ( struct efi_saved_tpl *tpl );

#endif /* _IPXE_EFI_H */
