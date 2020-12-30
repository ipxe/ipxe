/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * @file
 *
 * EFI image wrapping
 *
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/LoadedImage.h>
#include <ipxe/efi/efi_wrap.h>

/** Colour for debug messages */
#define colour &efi_systab

/**
 * Convert EFI status code to text
 *
 * @v efirc		EFI status code
 * @ret text		EFI status code text
 */
static const char * efi_status ( EFI_STATUS efirc ) {
	static char buf[ 19 /* "0xXXXXXXXXXXXXXXXX" + NUL */ ];

	switch ( efirc ) {
	case EFI_SUCCESS :			return "0";
	case EFI_LOAD_ERROR :			return "LOAD_ERROR";
	case EFI_INVALID_PARAMETER :		return "INVALID_PARAMETER";
	case EFI_UNSUPPORTED :			return "UNSUPPORTED";
	case EFI_BAD_BUFFER_SIZE :		return "BAD_BUFFER_SIZE";
	case EFI_BUFFER_TOO_SMALL :		return "BUFFER_TOO_SMALL";
	case EFI_NOT_READY :			return "NOT_READY";
	case EFI_DEVICE_ERROR :			return "DEVICE_ERROR";
	case EFI_WRITE_PROTECTED :		return "WRITE_PROTECTED";
	case EFI_OUT_OF_RESOURCES :		return "OUT_OF_RESOURCES";
	case EFI_VOLUME_CORRUPTED :		return "VOLUME_CORRUPTED";
	case EFI_VOLUME_FULL :			return "VOLUME_FULL";
	case EFI_NO_MEDIA :			return "NO_MEDIA";
	case EFI_MEDIA_CHANGED :		return "MEDIA_CHANGED";
	case EFI_NOT_FOUND :			return "NOT_FOUND";
	case EFI_ACCESS_DENIED :		return "ACCESS_DENIED";
	case EFI_NO_RESPONSE :			return "NO_RESPONSE";
	case EFI_NO_MAPPING :			return "NO_MAPPING";
	case EFI_TIMEOUT :			return "TIMEOUT";
	case EFI_NOT_STARTED :			return "NOT_STARTED";
	case EFI_ALREADY_STARTED :		return "ALREADY_STARTED";
	case EFI_ABORTED :			return "ABORTED";
	case EFI_ICMP_ERROR :			return "ICMP_ERROR";
	case EFI_TFTP_ERROR :			return "TFTP_ERROR";
	case EFI_PROTOCOL_ERROR :		return "PROTOCOL_ERROR";
	case EFI_INCOMPATIBLE_VERSION :		return "INCOMPATIBLE_VERSION";
	case EFI_SECURITY_VIOLATION :		return "SECURITY_VIOLATION";
	case EFI_CRC_ERROR :			return "CRC_ERROR";
	case EFI_END_OF_MEDIA :			return "END_OF_MEDIA";
	case EFI_END_OF_FILE :			return "END_OF_FILE";
	case EFI_INVALID_LANGUAGE :		return "INVALID_LANGUAGE";
	case EFI_COMPROMISED_DATA :		return "COMPROMISED_DATA";
	case EFI_WARN_UNKNOWN_GLYPH :		return "WARN_UNKNOWN_GLYPH";
	case EFI_WARN_DELETE_FAILURE :		return "WARN_DELETE_FAILURE";
	case EFI_WARN_WRITE_FAILURE :		return "WARN_WRITE_FAILURE";
	case EFI_WARN_BUFFER_TOO_SMALL :	return "WARN_BUFFER_TOO_SMALL";
	case EFI_WARN_STALE_DATA :		return "WARN_STALE_DATA";
	default:
		snprintf ( buf, sizeof ( buf ), "%#lx",
			   ( unsigned long ) efirc );
		return buf;
	}
}

/**
 * Convert EFI boolean to text
 *
 * @v boolean		Boolean value
 * @ret text		Boolean value text
 */
static const char * efi_boolean ( BOOLEAN boolean ) {

	return ( boolean ? "TRUE" : "FALSE" );
}

/**
 * Convert EFI TPL to text
 *
 * @v tpl		Task priority level
 * @ret text		Task priority level as text
 */
static const char * efi_tpl ( EFI_TPL tpl ) {
	static char buf[ 19 /* "0xXXXXXXXXXXXXXXXX" + NUL */ ];

	switch ( tpl ) {
	case TPL_APPLICATION:		return "Application";
	case TPL_CALLBACK:		return "Callback";
	case TPL_NOTIFY:		return "Notify";
	case TPL_HIGH_LEVEL:		return "HighLevel";
	default:
		snprintf ( buf, sizeof ( buf ), "%#lx",
			   ( unsigned long ) tpl );
		return buf;
	}
}

/**
 * Convert EFI allocation type to text
 *
 * @v type		Allocation type
 * @ret text		Allocation type as text
 */
static const char * efi_allocate_type ( EFI_ALLOCATE_TYPE type ) {
	static char buf[ 11 /* "0xXXXXXXXX" + NUL */ ];

	switch ( type ) {
	case AllocateAnyPages:		return "AnyPages";
	case AllocateMaxAddress:	return "MaxAddress";
	case AllocateAddress:		return "Address";
	default:
		snprintf ( buf, sizeof ( buf ), "%#x", type );
		return buf;
	}
}

/**
 * Convert EFI memory type to text
 *
 * @v type		Memory type
 * @ret text		Memory type as text
 */
static const char * efi_memory_type ( EFI_MEMORY_TYPE type ) {
	static char buf[ 11 /* "0xXXXXXXXX" + NUL */ ];

	switch ( type ) {
	case EfiReservedMemoryType:	return "Reserved";
	case EfiLoaderCode:		return "LoaderCode";
	case EfiLoaderData:		return "LoaderData";
	case EfiBootServicesCode:	return "BootCode";
	case EfiBootServicesData:	return "BootData";
	case EfiRuntimeServicesCode:	return "RuntimeCode";
	case EfiRuntimeServicesData:	return "RuntimeData";
	case EfiConventionalMemory:	return "Conventional";
	case EfiUnusableMemory:		return "Unusable";
	case EfiACPIReclaimMemory:	return "ACPIReclaim";
	case EfiACPIMemoryNVS:		return "ACPINVS";
	case EfiMemoryMappedIO:		return "MMIO";
	case EfiMemoryMappedIOPortSpace:return "PIO";
	case EfiPalCode:		return "PalCode";
	case EfiPersistentMemory:	return "Persistent";
	default:
		snprintf ( buf, sizeof ( buf ), "%#x", type );
		return buf;
	}
}

/**
 * Convert EFI timer delay type to text
 *
 * @v type		Timer delay type
 * @ret text		Timer delay type as text
 */
static const char * efi_timer_delay ( EFI_TIMER_DELAY type ) {
	static char buf[ 11 /* "0xXXXXXXXX" + NUL */ ];

	switch ( type ) {
	case TimerCancel:		return "Cancel";
	case TimerPeriodic:		return "Periodic";
	case TimerRelative:		return "Relative";
	default:
		snprintf ( buf, sizeof ( buf ), "%#x", type );
		return buf;
	}
}

/**
 * Wrap RaiseTPL()
 *
 */
static EFI_TPL EFIAPI
efi_raise_tpl_wrapper ( EFI_TPL new_tpl ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_TPL old_tpl;

	DBGCP ( colour, "RaiseTPL ( %s ) ", efi_tpl ( new_tpl ) );
	old_tpl = bs->RaiseTPL ( new_tpl );
	DBGCP ( colour, "= %s -> %p\n", efi_tpl ( old_tpl ), retaddr );
	return old_tpl;
}

/**
 * Wrap RestoreTPL()
 *
 */
static VOID EFIAPI
efi_restore_tpl_wrapper ( EFI_TPL old_tpl ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );

	DBGCP ( colour, "RestoreTPL ( %s ) ", efi_tpl ( old_tpl ) );
	bs->RestoreTPL ( old_tpl );
	DBGCP ( colour, "-> %p\n", retaddr );
}

/**
 * Wrap AllocatePages()
 *
 */
static EFI_STATUS EFIAPI
efi_allocate_pages_wrapper ( EFI_ALLOCATE_TYPE type,
			     EFI_MEMORY_TYPE memory_type, UINTN pages,
			     EFI_PHYSICAL_ADDRESS *memory ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC2 ( colour, "AllocatePages ( %s, %s, %#llx, %#llx ) ",
		efi_allocate_type ( type ), efi_memory_type ( memory_type ),
		( ( unsigned long long ) pages ),
		( ( unsigned long long ) *memory ) );
	efirc = bs->AllocatePages ( type, memory_type, pages, memory );
	DBGC2 ( colour, "= %s ( %#llx ) -> %p\n", efi_status ( efirc ),
		( ( unsigned long long ) *memory ), retaddr );
	return efirc;
}

/**
 * Wrap FreePages()
 *
 */
static EFI_STATUS EFIAPI
efi_free_pages_wrapper ( EFI_PHYSICAL_ADDRESS memory, UINTN pages ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC2 ( colour, "FreePages ( %#llx, %#llx ) ",
		( ( unsigned long long ) memory ),
		( ( unsigned long long ) pages ) );
	efirc = bs->FreePages ( memory, pages );
	DBGC2 ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap GetMemoryMap()
 *
 */
static EFI_STATUS EFIAPI
efi_get_memory_map_wrapper ( UINTN *memory_map_size,
			     EFI_MEMORY_DESCRIPTOR *memory_map, UINTN *map_key,
			     UINTN *descriptor_size,
			     UINT32 *descriptor_version ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_MEMORY_DESCRIPTOR *desc;
	size_t remaining;
	EFI_STATUS efirc;

	DBGC ( colour, "GetMemoryMap ( %#llx, %p ) ",
	       ( ( unsigned long long ) *memory_map_size ), memory_map );
	efirc = bs->GetMemoryMap ( memory_map_size, memory_map, map_key,
				   descriptor_size, descriptor_version );
	DBGC ( colour, "= %s ( %#llx, %#llx, %#llx, v%d",
	       efi_status ( efirc ),
	       ( ( unsigned long long ) *memory_map_size ),
	       ( ( unsigned long long ) *map_key ),
	       ( ( unsigned long long ) *descriptor_size ),
	       *descriptor_version );
	if ( DBG_EXTRA && ( efirc == 0 ) ) {
		DBGC2 ( colour, ",\n" );
		for ( desc = memory_map, remaining = *memory_map_size ;
		      remaining >= *descriptor_size ;
		      desc = ( ( ( void * ) desc ) + *descriptor_size ),
		      remaining -= *descriptor_size ) {
			DBGC2 ( colour, "%#016llx+%#08llx %#016llx "
				"%s\n", desc->PhysicalStart,
				( desc->NumberOfPages * EFI_PAGE_SIZE ),
				desc->Attribute,
				efi_memory_type ( desc->Type ) );
		}
	} else {
		DBGC ( colour, " " );
	}
	DBGC ( colour, ") -> %p\n", retaddr );
	return efirc;
}

/**
 * Wrap AllocatePool()
 *
 */
static EFI_STATUS EFIAPI
efi_allocate_pool_wrapper ( EFI_MEMORY_TYPE pool_type, UINTN size,
			    VOID **buffer ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC2 ( colour, "AllocatePool ( %s, %#llx ) ",
		efi_memory_type ( pool_type ),
		( ( unsigned long long ) size ) );
	efirc = bs->AllocatePool ( pool_type, size, buffer );
	DBGC2 ( colour, "= %s ( %p ) -> %p\n",
		efi_status ( efirc ), *buffer, retaddr );
	return efirc;
}

/**
 * Wrap FreePool()
 *
 */
static EFI_STATUS EFIAPI
efi_free_pool_wrapper ( VOID *buffer ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC2 ( colour, "FreePool ( %p ) ", buffer );
	efirc = bs->FreePool ( buffer );
	DBGC2 ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap CreateEvent()
 *
 */
static EFI_STATUS EFIAPI
efi_create_event_wrapper ( UINT32 type, EFI_TPL notify_tpl,
			   EFI_EVENT_NOTIFY notify_function,
			   VOID *notify_context, EFI_EVENT *event ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "CreateEvent ( %#x, %s, %p, %p ) ",
	       type, efi_tpl ( notify_tpl ), notify_function, notify_context );
	efirc = bs->CreateEvent ( type, notify_tpl, notify_function,
				  notify_context, event );
	DBGC ( colour, "= %s ( %p ) -> %p\n",
	       efi_status ( efirc ), *event, retaddr );
	return efirc;
}

/**
 * Wrap SetTimer()
 *
 */
static EFI_STATUS EFIAPI
efi_set_timer_wrapper ( EFI_EVENT event, EFI_TIMER_DELAY type,
			UINT64 trigger_time ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "SetTimer ( %p, %s, %ld.%07ld00s ) ",
	       event, efi_timer_delay ( type ),
	       ( ( unsigned long ) ( trigger_time / 10000000 ) ),
	       ( ( unsigned long ) ( trigger_time % 10000000 ) ) );
	efirc = bs->SetTimer ( event, type, trigger_time );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap WaitForEvent()
 *
 */
static EFI_STATUS EFIAPI
efi_wait_for_event_wrapper ( UINTN number_of_events, EFI_EVENT *event,
			     UINTN *index ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	unsigned int i;
	EFI_STATUS efirc;

	DBGC ( colour, "WaitForEvent (" );
	for ( i = 0 ; i < number_of_events ; i++ )
		DBGC ( colour, " %p", event[i] );
	DBGC ( colour, " ) " );
	efirc = bs->WaitForEvent ( number_of_events, event, index );
	DBGC ( colour, "= %s", efi_status ( efirc ) );
	if ( efirc == 0 )
		DBGC ( colour, " ( %p )", event[*index] );
	DBGC ( colour, " -> %p\n", retaddr );
	return efirc;
}

/**
 * Wrap SignalEvent()
 *
 */
static EFI_STATUS EFIAPI
efi_signal_event_wrapper ( EFI_EVENT event ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC2 ( colour, "SignalEvent ( %p ) ", event );
	efirc = bs->SignalEvent ( event );
	DBGC2 ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap CloseEvent()
 *
 */
static EFI_STATUS EFIAPI
efi_close_event_wrapper ( EFI_EVENT event ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "CloseEvent ( %p ) ", event );
	efirc = bs->SignalEvent ( event );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}
/**
 * Wrap CheckEvent()
 *
 */
static EFI_STATUS EFIAPI
efi_check_event_wrapper ( EFI_EVENT event ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGCP ( colour, "CheckEvent ( %p ) ", event );
	efirc = bs->SignalEvent ( event );
	DBGCP ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap InstallProtocolInterface()
 *
 */
static EFI_STATUS EFIAPI
efi_install_protocol_interface_wrapper ( EFI_HANDLE *handle, EFI_GUID *protocol,
					 EFI_INTERFACE_TYPE interface_type,
					 VOID *interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "InstallProtocolInterface ( %s, %s, %d, %p ) ",
	       efi_handle_name ( *handle ), efi_guid_ntoa ( protocol ),
	       interface_type, interface );
	efirc = bs->InstallProtocolInterface ( handle, protocol, interface_type,
					       interface );
	DBGC ( colour, "= %s ( %s ) -> %p\n",
	       efi_status ( efirc ), efi_handle_name ( *handle ), retaddr );
	return efirc;
}

/**
 * Wrap ReinstallProtocolInterface()
 *
 */
static EFI_STATUS EFIAPI
efi_reinstall_protocol_interface_wrapper ( EFI_HANDLE handle,
					   EFI_GUID *protocol,
					   VOID *old_interface,
					   VOID *new_interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "ReinstallProtocolInterface ( %s, %s, %p, %p ) ",
	       efi_handle_name ( handle ), efi_guid_ntoa ( protocol ),
	       old_interface, new_interface );
	efirc = bs->ReinstallProtocolInterface ( handle, protocol,
						 old_interface, new_interface );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap UninstallProtocolInterface()
 *
 */
static EFI_STATUS EFIAPI
efi_uninstall_protocol_interface_wrapper ( EFI_HANDLE handle,
					   EFI_GUID *protocol,
					   VOID *interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "UninstallProtocolInterface ( %s, %s, %p ) ",
	       efi_handle_name ( handle ), efi_guid_ntoa ( protocol ),
	       interface );
	efirc = bs->UninstallProtocolInterface ( handle, protocol, interface );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap HandleProtocol()
 *
 */
static EFI_STATUS EFIAPI
efi_handle_protocol_wrapper ( EFI_HANDLE handle, EFI_GUID *protocol,
			      VOID **interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "HandleProtocol ( %s, %s ) ",
	       efi_handle_name ( handle ), efi_guid_ntoa ( protocol ) );
	efirc = bs->HandleProtocol ( handle, protocol, interface );
	DBGC ( colour, "= %s ( %p ) -> %p\n",
	       efi_status ( efirc ), *interface, retaddr );
	return efirc;
}

/**
 * Wrap RegisterProtocolNotify()
 *
 */
static EFI_STATUS EFIAPI
efi_register_protocol_notify_wrapper ( EFI_GUID *protocol, EFI_EVENT event,
				       VOID **registration ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "RegisterProtocolNotify ( %s, %p ) ",
	       efi_guid_ntoa ( protocol ), event );
	efirc = bs->RegisterProtocolNotify ( protocol, event, registration );
	DBGC ( colour, "= %s ( %p ) -> %p\n",
	       efi_status ( efirc ), *registration, retaddr );
	return efirc;
}

/**
 * Wrap LocateHandle()
 *
 */
static EFI_STATUS EFIAPI
efi_locate_handle_wrapper ( EFI_LOCATE_SEARCH_TYPE search_type,
			    EFI_GUID *protocol, VOID *search_key,
			    UINTN *buffer_size, EFI_HANDLE *buffer ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	unsigned int i;
	EFI_STATUS efirc;

	DBGC ( colour, "LocateHandle ( %s, %s, %p, %zd ) ",
	       efi_locate_search_type_name ( search_type ),
	       efi_guid_ntoa ( protocol ), search_key,
	       ( ( size_t ) *buffer_size ) );
	efirc = bs->LocateHandle ( search_type, protocol, search_key,
				   buffer_size, buffer );
	DBGC ( colour, "= %s ( %zd", efi_status ( efirc ),
	       ( ( size_t ) *buffer_size ) );
	if ( efirc == 0 ) {
		DBGC ( colour, ", {" );
		for ( i = 0; i < ( *buffer_size / sizeof ( buffer[0] ) ); i++ ){
			DBGC ( colour, "%s%s", ( i ? ", " : " " ),
			       efi_handle_name ( buffer[i] ) );
		}
		DBGC ( colour, " }" );
	}
	DBGC ( colour, " ) -> %p\n", retaddr );
	return efirc;
}

/**
 * Wrap LocateDevicePath()
 *
 */
static EFI_STATUS EFIAPI
efi_locate_device_path_wrapper ( EFI_GUID *protocol,
				 EFI_DEVICE_PATH_PROTOCOL **device_path,
				 EFI_HANDLE *device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "LocateDevicePath ( %s, %s ) ",
	       efi_guid_ntoa ( protocol ), efi_devpath_text ( *device_path ) );
	efirc = bs->LocateDevicePath ( protocol, device_path, device );
	DBGC ( colour, "= %s ( %s, ",
	       efi_status ( efirc ), efi_devpath_text ( *device_path ) );
	DBGC ( colour, "%s ) -> %p\n", efi_handle_name ( *device ), retaddr );
	return efirc;
}

/**
 * Wrap InstallConfigurationTable()
 *
 */
static EFI_STATUS EFIAPI
efi_install_configuration_table_wrapper ( EFI_GUID *guid, VOID *table ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "InstallConfigurationTable ( %s, %p ) ",
	       efi_guid_ntoa ( guid ), table );
	efirc = bs->InstallConfigurationTable ( guid, table );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap LoadImage()
 *
 */
static EFI_STATUS EFIAPI
efi_load_image_wrapper ( BOOLEAN boot_policy, EFI_HANDLE parent_image_handle,
			 EFI_DEVICE_PATH_PROTOCOL *device_path,
			 VOID *source_buffer, UINTN source_size,
			 EFI_HANDLE *image_handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "LoadImage ( %s, %s, ", efi_boolean ( boot_policy ),
	       efi_handle_name ( parent_image_handle ) );
	DBGC ( colour, "%s, %p, %#llx ) ",
	       efi_devpath_text ( device_path ), source_buffer,
	       ( ( unsigned long long ) source_size ) );
	efirc = bs->LoadImage ( boot_policy, parent_image_handle, device_path,
				source_buffer, source_size, image_handle );
	DBGC ( colour, "= %s ( ", efi_status ( efirc ) );
	if ( efirc == 0 )
		DBGC ( colour, "%s ", efi_handle_name ( *image_handle ) );
	DBGC ( colour, ") -> %p\n", retaddr );

	/* Wrap the new image */
	if ( efirc == 0 )
		efi_wrap ( *image_handle );

	return efirc;
}

/**
 * Wrap StartImage()
 *
 */
static EFI_STATUS EFIAPI
efi_start_image_wrapper ( EFI_HANDLE image_handle, UINTN *exit_data_size,
			  CHAR16 **exit_data ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "StartImage ( %s ) ", efi_handle_name ( image_handle ) );
	efirc = bs->StartImage ( image_handle, exit_data_size, exit_data );
	DBGC ( colour, "= %s", efi_status ( efirc ) );
	if ( ( efirc != 0 ) && exit_data && *exit_data_size )
		DBGC ( colour, " ( \"%ls\" )", *exit_data );
	DBGC ( colour, " -> %p\n", retaddr );
	if ( ( efirc != 0 ) && exit_data && *exit_data_size )
		DBGC_HD ( colour, *exit_data, *exit_data_size );
	return efirc;
}

/**
 * Wrap Exit()
 *
 */
static EFI_STATUS EFIAPI
efi_exit_wrapper ( EFI_HANDLE image_handle, EFI_STATUS exit_status,
		   UINTN exit_data_size, CHAR16 *exit_data ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	if ( ( exit_status != 0 ) && exit_data && exit_data_size )
		DBGC_HD ( colour, exit_data, exit_data_size );
	DBGC ( colour, "Exit ( %s, %s",
	       efi_handle_name ( image_handle ), efi_status ( exit_status ) );
	if ( ( exit_status != 0 ) && exit_data && exit_data_size )
		DBGC ( colour, ", \"%ls\"", exit_data );
	DBGC ( colour, " ) " );
	efirc = bs->Exit ( image_handle, exit_status, exit_data_size,
			   exit_data );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap UnloadImage()
 *
 */
static EFI_STATUS EFIAPI
efi_unload_image_wrapper ( EFI_HANDLE image_handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "UnloadImage ( %s ) ",
	       efi_handle_name ( image_handle ) );
	efirc = bs->UnloadImage ( image_handle );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap ExitBootServices()
 *
 */
static EFI_STATUS EFIAPI
efi_exit_boot_services_wrapper ( EFI_HANDLE image_handle, UINTN map_key ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "ExitBootServices ( %s, %#llx ) ",
	       efi_handle_name ( image_handle ),
	       ( ( unsigned long long ) map_key ) );
	efirc = bs->ExitBootServices ( image_handle, map_key );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap GetNextMonotonicCount()
 *
 */
static EFI_STATUS EFIAPI
efi_get_next_monotonic_count_wrapper ( UINT64 *count ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGCP ( colour, "GetNextMonotonicCount() " );
	efirc = bs->GetNextMonotonicCount ( count );
	DBGCP ( colour, "= %s ( %#llx ) -> %p\n",
		efi_status ( efirc ), *count, retaddr );
	return efirc;
}

/**
 * Wrap Stall()
 *
 */
static EFI_STATUS EFIAPI
efi_stall_wrapper ( UINTN microseconds ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC2 ( colour, "Stall ( %ld.%06lds ) ",
		( ( unsigned long ) ( microseconds / 1000000 ) ),
		( ( unsigned long ) ( microseconds % 1000000 ) ) );
	efirc = bs->Stall ( microseconds );
	DBGC2 ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap SetWatchdogTimer()
 *
 */
static EFI_STATUS EFIAPI
efi_set_watchdog_timer_wrapper ( UINTN timeout, UINT64 watchdog_code,
				 UINTN data_size, CHAR16 *watchdog_data ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "SetWatchdogTimer ( %lds, %#llx, %#llx, %p ) ",
	       ( ( unsigned long ) timeout ), watchdog_code,
	       ( ( unsigned long long ) data_size ), watchdog_data );
	efirc = bs->SetWatchdogTimer ( timeout, watchdog_code, data_size,
				       watchdog_data );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap ConnectController()
 *
 */
static EFI_STATUS EFIAPI
efi_connect_controller_wrapper ( EFI_HANDLE controller_handle,
				 EFI_HANDLE *driver_image_handle,
				 EFI_DEVICE_PATH_PROTOCOL *remaining_path,
				 BOOLEAN recursive ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_HANDLE *tmp;
	EFI_STATUS efirc;

	DBGC ( colour, "ConnectController ( %s, {",
	       efi_handle_name ( controller_handle ) );
	if ( driver_image_handle ) {
		for ( tmp = driver_image_handle ; *tmp ; tmp++ ) {
			DBGC ( colour, "%s%s",
			       ( ( tmp == driver_image_handle ) ? " " : ", " ),
			       efi_handle_name ( *tmp ) );
		}
	}
	DBGC ( colour, " }, %s, %s ) ", efi_devpath_text ( remaining_path ),
	       efi_boolean ( recursive ) );
	efirc = bs->ConnectController ( controller_handle, driver_image_handle,
					remaining_path, recursive );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap DisconnectController()
 *
 */
static EFI_STATUS EFIAPI
efi_disconnect_controller_wrapper ( EFI_HANDLE controller_handle,
				    EFI_HANDLE driver_image_handle,
				    EFI_HANDLE child_handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "DisconnectController ( %s",
	       efi_handle_name ( controller_handle ) );
	DBGC ( colour, ", %s", efi_handle_name ( driver_image_handle ) );
	DBGC ( colour, ", %s ) ", efi_handle_name ( child_handle ) );
	efirc = bs->DisconnectController ( controller_handle,
					   driver_image_handle,
					   child_handle );
	DBGC ( colour, "= %s -> %p\n", efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap OpenProtocol()
 *
 */
static EFI_STATUS EFIAPI
efi_open_protocol_wrapper ( EFI_HANDLE handle, EFI_GUID *protocol,
			    VOID **interface, EFI_HANDLE agent_handle,
			    EFI_HANDLE controller_handle, UINT32 attributes ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "OpenProtocol ( %s, %s, ",
	       efi_handle_name ( handle ), efi_guid_ntoa ( protocol ) );
	DBGC ( colour, "%s, ", efi_handle_name ( agent_handle ) );
	DBGC ( colour, "%s, %s ) ", efi_handle_name ( controller_handle ),
	       efi_open_attributes_name ( attributes ) );
	efirc = bs->OpenProtocol ( handle, protocol, interface, agent_handle,
				   controller_handle, attributes );
	DBGC ( colour, "= %s ( %p ) -> %p\n",
	       efi_status ( efirc ), *interface, retaddr );
	return efirc;
}

/**
 * Wrap CloseProtocol()
 *
 */
static EFI_STATUS EFIAPI
efi_close_protocol_wrapper ( EFI_HANDLE handle, EFI_GUID *protocol,
			     EFI_HANDLE agent_handle,
			     EFI_HANDLE controller_handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "CloseProtocol ( %s, %s, ",
	       efi_handle_name ( handle ), efi_guid_ntoa ( protocol ) );
	DBGC ( colour, "%s, ", efi_handle_name ( agent_handle ) );
	DBGC ( colour, "%s ) ", efi_handle_name ( controller_handle ) );
	efirc = bs->CloseProtocol ( handle, protocol, agent_handle,
				    controller_handle );
	DBGC ( colour, "= %s -> %p\n",
	       efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap OpenProtocolInformation()
 *
 */
static EFI_STATUS EFIAPI
efi_open_protocol_information_wrapper ( EFI_HANDLE handle, EFI_GUID *protocol,
					EFI_OPEN_PROTOCOL_INFORMATION_ENTRY
					**entry_buffer,
					UINTN *entry_count ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "OpenProtocolInformation ( %s, %s ) ",
	       efi_handle_name ( handle ), efi_guid_ntoa ( protocol ) );
	efirc = bs->OpenProtocolInformation ( handle, protocol, entry_buffer,
					      entry_count );
	DBGC ( colour, "= %s ( %p, %#llx ) -> %p\n",
	       efi_status ( efirc ), *entry_buffer,
	       ( ( unsigned long long ) *entry_count ), retaddr );
	return efirc;
}

/**
 * Wrap ProtocolsPerHandle()
 *
 */
static EFI_STATUS EFIAPI
efi_protocols_per_handle_wrapper ( EFI_HANDLE handle,
				   EFI_GUID ***protocol_buffer,
				   UINTN *protocol_buffer_count ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	unsigned int i;
	EFI_STATUS efirc;

	DBGC ( colour, "ProtocolsPerHandle ( %s ) ",
	       efi_handle_name ( handle ) );
	efirc = bs->ProtocolsPerHandle ( handle, protocol_buffer,
					 protocol_buffer_count );
	DBGC ( colour, "= %s", efi_status ( efirc ) );
	if ( efirc == 0 ) {
		DBGC ( colour, " ( {" );
		for ( i = 0 ; i < *protocol_buffer_count ; i++ ) {
			DBGC ( colour, "%s%s", ( i ? ", " : " " ),
			       efi_guid_ntoa ( (*protocol_buffer)[i] ) );
		}
		DBGC ( colour, " } )" );
	}
	DBGC ( colour, " -> %p\n", retaddr );
	return efirc;
}

/**
 * Wrap LocateHandleBuffer()
 *
 */
static EFI_STATUS EFIAPI
efi_locate_handle_buffer_wrapper ( EFI_LOCATE_SEARCH_TYPE search_type,
				   EFI_GUID *protocol, VOID *search_key,
				   UINTN *no_handles, EFI_HANDLE **buffer ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	unsigned int i;
	EFI_STATUS efirc;

	DBGC ( colour, "LocateHandleBuffer ( %s, %s, %p ) ",
	       efi_locate_search_type_name ( search_type ),
	       efi_guid_ntoa ( protocol ), search_key );
	efirc = bs->LocateHandleBuffer ( search_type, protocol, search_key,
					 no_handles, buffer );
	DBGC ( colour, "= %s", efi_status ( efirc ) );
	if ( efirc == 0 ) {
		DBGC ( colour, " ( %d, {", ( ( unsigned int ) *no_handles ) );
		for ( i = 0 ; i < *no_handles ; i++ ) {
			DBGC ( colour, "%s%s", ( i ? ", " : " " ),
			       efi_handle_name ( (*buffer)[i] ) );
		}
		DBGC ( colour, " } )" );
	}
	DBGC ( colour, " -> %p\n", retaddr );
	return efirc;
}

/**
 * Wrap LocateProtocol()
 *
 */
static EFI_STATUS EFIAPI
efi_locate_protocol_wrapper ( EFI_GUID *protocol, VOID *registration,
			      VOID **interface ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "LocateProtocol ( %s, %p ) ",
	       efi_guid_ntoa ( protocol ), registration );
	efirc = bs->LocateProtocol ( protocol, registration, interface );
	DBGC ( colour, "= %s ( %p ) -> %p\n",
	       efi_status ( efirc ), *interface, retaddr );
	return efirc;
}

/** Maximum number of interfaces for wrapped ...MultipleProtocolInterfaces() */
#define MAX_WRAP_MULTI 20

/**
 * Wrap InstallMultipleProtocolInterfaces()
 *
 */
static EFI_STATUS EFIAPI
efi_install_multiple_protocol_interfaces_wrapper ( EFI_HANDLE *handle, ... ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_GUID *protocol[ MAX_WRAP_MULTI + 1 ];
	VOID *interface[MAX_WRAP_MULTI];
	VA_LIST ap;
	unsigned int i;
	EFI_STATUS efirc;

	DBGC ( colour, "InstallMultipleProtocolInterfaces ( %s",
	       efi_handle_name ( *handle ) );
	memset ( protocol, 0, sizeof ( protocol ) );
	memset ( interface, 0, sizeof ( interface ) );
	VA_START ( ap, handle );
	for ( i = 0 ; ( protocol[i] = VA_ARG ( ap, EFI_GUID * ) ) ; i++ ) {
		if ( i == MAX_WRAP_MULTI ) {
			VA_END ( ap );
			efirc = EFI_OUT_OF_RESOURCES;
			DBGC ( colour, "<FATAL: too many arguments> ) = %s "
			       "-> %p\n", efi_status ( efirc ), retaddr );
			return efirc;
		}
		interface[i] = VA_ARG ( ap, VOID * );
		DBGC ( colour, ", %s, %p",
		       efi_guid_ntoa ( protocol[i] ), interface[i] );
	}
	VA_END ( ap );
	DBGC ( colour, " ) " );
	efirc = bs->InstallMultipleProtocolInterfaces ( handle,
		protocol[0], interface[0], protocol[1], interface[1],
		protocol[2], interface[2], protocol[3], interface[3],
		protocol[4], interface[4], protocol[5], interface[5],
		protocol[6], interface[6], protocol[7], interface[7],
		protocol[8], interface[8], protocol[9], interface[9],
		protocol[10], interface[10], protocol[11], interface[11],
		protocol[12], interface[12], protocol[13], interface[13],
		protocol[14], interface[14], protocol[15], interface[15],
		protocol[16], interface[16], protocol[17], interface[17],
		protocol[18], interface[18], protocol[19], interface[19],
		NULL );
	DBGC ( colour, "= %s ( %s ) -> %p\n",
	       efi_status ( efirc ), efi_handle_name ( *handle ), retaddr );
	return efirc;
}

/**
 * Wrap UninstallMultipleProtocolInterfaces()
 *
 */
static EFI_STATUS EFIAPI
efi_uninstall_multiple_protocol_interfaces_wrapper ( EFI_HANDLE handle, ... ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_GUID *protocol[ MAX_WRAP_MULTI  + 1 ];
	VOID *interface[MAX_WRAP_MULTI];
	VA_LIST ap;
	unsigned int i;
	EFI_STATUS efirc;

	DBGC ( colour, "UninstallMultipleProtocolInterfaces ( %s",
	       efi_handle_name ( handle ) );
	memset ( protocol, 0, sizeof ( protocol ) );
	memset ( interface, 0, sizeof ( interface ) );
	VA_START ( ap, handle );
	for ( i = 0 ; ( protocol[i] = VA_ARG ( ap, EFI_GUID * ) ) ; i++ ) {
		if ( i == MAX_WRAP_MULTI ) {
			VA_END ( ap );
			efirc = EFI_OUT_OF_RESOURCES;
			DBGC ( colour, "<FATAL: too many arguments> ) = %s "
			       "-> %p\n", efi_status ( efirc ), retaddr );
			return efirc;
		}
		interface[i] = VA_ARG ( ap, VOID * );
		DBGC ( colour, ", %s, %p",
		       efi_guid_ntoa ( protocol[i] ), interface[i] );
	}
	VA_END ( ap );
	DBGC ( colour, " ) " );
	efirc = bs->UninstallMultipleProtocolInterfaces ( handle,
		protocol[0], interface[0], protocol[1], interface[1],
		protocol[2], interface[2], protocol[3], interface[3],
		protocol[4], interface[4], protocol[5], interface[5],
		protocol[6], interface[6], protocol[7], interface[7],
		protocol[8], interface[8], protocol[9], interface[9],
		protocol[10], interface[10], protocol[11], interface[11],
		protocol[12], interface[12], protocol[13], interface[13],
		protocol[14], interface[14], protocol[15], interface[15],
		protocol[16], interface[16], protocol[17], interface[17],
		protocol[18], interface[18], protocol[19], interface[19],
		NULL );
	DBGC ( colour, "= %s -> %p\n",
	       efi_status ( efirc ), retaddr );
	return efirc;
}

/**
 * Wrap CreateEventEx()
 *
 */
static EFI_STATUS EFIAPI
efi_create_event_ex_wrapper ( UINT32 type, EFI_TPL notify_tpl,
			      EFI_EVENT_NOTIFY notify_function,
			      CONST VOID *notify_context,
			      CONST EFI_GUID *event_group, EFI_EVENT *event ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	void *retaddr = __builtin_return_address ( 0 );
	EFI_STATUS efirc;

	DBGC ( colour, "CreateEventEx ( %#x, %s, %p, %p, %s ) ",
	       type, efi_tpl ( notify_tpl ), notify_function, notify_context,
	       efi_guid_ntoa ( event_group ) );
	efirc = bs->CreateEventEx ( type, notify_tpl, notify_function,
				    notify_context, event_group, event );
	DBGC ( colour, "= %s ( %p ) -> %p\n",
	       efi_status ( efirc ), *event, retaddr );
	return efirc;
}

/**
 * Build table wrappers
 *
 * @ret systab		Wrapped system table
 */
EFI_SYSTEM_TABLE * efi_wrap_systab ( void ) {
	static EFI_SYSTEM_TABLE efi_systab_wrapper;
	static EFI_BOOT_SERVICES efi_bs_wrapper;
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* Build boot services table wrapper */
	memcpy ( &efi_bs_wrapper, bs, sizeof ( efi_bs_wrapper ) );
	efi_bs_wrapper.RaiseTPL		= efi_raise_tpl_wrapper;
	efi_bs_wrapper.RestoreTPL	= efi_restore_tpl_wrapper;
	efi_bs_wrapper.AllocatePages	= efi_allocate_pages_wrapper;
	efi_bs_wrapper.FreePages	= efi_free_pages_wrapper;
	efi_bs_wrapper.GetMemoryMap	= efi_get_memory_map_wrapper;
	efi_bs_wrapper.AllocatePool	= efi_allocate_pool_wrapper;
	efi_bs_wrapper.FreePool		= efi_free_pool_wrapper;
	efi_bs_wrapper.CreateEvent	= efi_create_event_wrapper;
	efi_bs_wrapper.SetTimer		= efi_set_timer_wrapper;
	efi_bs_wrapper.WaitForEvent	= efi_wait_for_event_wrapper;
	efi_bs_wrapper.SignalEvent	= efi_signal_event_wrapper;
	efi_bs_wrapper.CloseEvent	= efi_close_event_wrapper;
	efi_bs_wrapper.CheckEvent	= efi_check_event_wrapper;
	efi_bs_wrapper.InstallProtocolInterface
		= efi_install_protocol_interface_wrapper;
	efi_bs_wrapper.ReinstallProtocolInterface
		= efi_reinstall_protocol_interface_wrapper;
	efi_bs_wrapper.UninstallProtocolInterface
		= efi_uninstall_protocol_interface_wrapper;
	efi_bs_wrapper.HandleProtocol	= efi_handle_protocol_wrapper;
	efi_bs_wrapper.RegisterProtocolNotify
		= efi_register_protocol_notify_wrapper;
	efi_bs_wrapper.LocateHandle	= efi_locate_handle_wrapper;
	efi_bs_wrapper.LocateDevicePath	= efi_locate_device_path_wrapper;
	efi_bs_wrapper.InstallConfigurationTable
		= efi_install_configuration_table_wrapper;
	efi_bs_wrapper.LoadImage	= efi_load_image_wrapper;
	efi_bs_wrapper.StartImage	= efi_start_image_wrapper;
	efi_bs_wrapper.Exit		= efi_exit_wrapper;
	efi_bs_wrapper.UnloadImage	= efi_unload_image_wrapper;
	efi_bs_wrapper.ExitBootServices	= efi_exit_boot_services_wrapper;
	efi_bs_wrapper.GetNextMonotonicCount
		= efi_get_next_monotonic_count_wrapper;
	efi_bs_wrapper.Stall		= efi_stall_wrapper;
	efi_bs_wrapper.SetWatchdogTimer	= efi_set_watchdog_timer_wrapper;
	efi_bs_wrapper.ConnectController
		= efi_connect_controller_wrapper;
	efi_bs_wrapper.DisconnectController
		= efi_disconnect_controller_wrapper;
	efi_bs_wrapper.OpenProtocol	= efi_open_protocol_wrapper;
	efi_bs_wrapper.CloseProtocol	= efi_close_protocol_wrapper;
	efi_bs_wrapper.OpenProtocolInformation
		= efi_open_protocol_information_wrapper;
	efi_bs_wrapper.ProtocolsPerHandle
		= efi_protocols_per_handle_wrapper;
	efi_bs_wrapper.LocateHandleBuffer
		= efi_locate_handle_buffer_wrapper;
	efi_bs_wrapper.LocateProtocol	= efi_locate_protocol_wrapper;
	efi_bs_wrapper.InstallMultipleProtocolInterfaces
		= efi_install_multiple_protocol_interfaces_wrapper;
	efi_bs_wrapper.UninstallMultipleProtocolInterfaces
		= efi_uninstall_multiple_protocol_interfaces_wrapper;
	efi_bs_wrapper.CreateEventEx	= efi_create_event_ex_wrapper;

	/* Build system table wrapper */
	memcpy ( &efi_systab_wrapper, efi_systab,
		 sizeof ( efi_systab_wrapper ) );
	efi_systab_wrapper.BootServices	= &efi_bs_wrapper;

	return &efi_systab_wrapper;
}

/**
 * Wrap the calls made by a loaded image
 *
 * @v handle		Image handle
 */
void efi_wrap ( EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_LOADED_IMAGE_PROTOCOL *image;
		void *intf;
	} loaded;
	EFI_STATUS efirc;
	int rc;

	/* Do nothing unless debugging is enabled */
	if ( ! DBG_LOG )
		return;

	/* Open loaded image protocol */
	if ( ( efirc = bs->OpenProtocol ( handle,
					  &efi_loaded_image_protocol_guid,
					  &loaded.intf, efi_image_handle, NULL,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		rc = -EEFI ( efirc );
		DBGC ( colour, "WRAP %s could not get loaded image protocol: "
		       "%s\n", efi_handle_name ( handle ), strerror ( rc ) );
		return;
	}

	/* Provide system table wrapper to image */
	loaded.image->SystemTable = efi_wrap_systab();
	DBGC ( colour, "WRAP %s at base %p has protocols:\n",
	       efi_handle_name ( handle ), loaded.image->ImageBase );
	DBGC_EFI_PROTOCOLS ( colour, handle );
	DBGC ( colour, "WRAP %s parent", efi_handle_name ( handle ) );
	DBGC ( colour, " %s\n", efi_handle_name ( loaded.image->ParentHandle ));
	DBGC ( colour, "WRAP %s device", efi_handle_name ( handle ) );
	DBGC ( colour, " %s\n", efi_handle_name ( loaded.image->DeviceHandle ));
	DBGC ( colour, "WRAP %s file", efi_handle_name ( handle ) );
	DBGC ( colour, " %s\n", efi_devpath_text ( loaded.image->FilePath ) );

	/* Close loaded image protocol */
	bs->CloseProtocol ( handle, &efi_loaded_image_protocol_guid,
			    efi_image_handle, NULL );
}
