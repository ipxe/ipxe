/*
 * Copyright (C) 2022 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ipxe/image.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_shim.h>
#include <ipxe/efi/Protocol/PxeBaseCode.h>
#include <ipxe/efi/Protocol/ShimLock.h>

/** @file
 *
 * UEFI shim special handling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * Require use of a third party loader binary
 *
 * The UEFI shim is gradually becoming less capable of directly
 * executing a Linux kernel image, due to an ever increasing list of
 * assumptions that it will only ever be used in conjunction with a
 * second stage loader binary such as GRUB.
 *
 * For example: shim will erroneously complain if the image that it
 * loads and executes does not in turn call in to the "shim lock
 * protocol" to verify a separate newly loaded binary before calling
 * ExitBootServices(), even if no such separate binary is used or
 * required.
 *
 * Experience shows that there is unfortunately no point in trying to
 * get a fix for this upstreamed into shim.  We therefore default to
 * reducing the Secure Boot attack surface by removing, where
 * possible, this spurious requirement for the use of an additional
 * second stage loader.
 *
 * This option may be used to require the use of an additional second
 * stage loader binary, in case this behaviour is ever desirable.
 */
int efi_shim_require_loader = 0;

/**
 * Allow use of PXE base code protocol
 *
 * We provide shim with access to all of the relevant downloaded files
 * via our EFI_SIMPLE_FILE_SYSTEM_PROTOCOL interface.  However, shim
 * will instead try to redownload the files via TFTP since it prefers
 * to use the EFI_PXE_BASE_CODE_PROTOCOL installed on the same handle.
 *
 * Experience shows that there is unfortunately no point in trying to
 * get a fix for this upstreamed into shim.  We therefore default to
 * working around this undesirable behaviour by stopping the PXE base
 * code protocol before invoking shim.
 *
 * This option may be used to allow shim to use the PXE base code
 * protocol, in case this behaviour is ever desirable.
 */
int efi_shim_allow_pxe = 0;

/**
 * Allow SBAT variable access
 *
 * The UEFI shim implements a fairly nicely designed revocation
 * mechanism designed around the concept of security generations.
 * Unfortunately nobody in the shim community has thus far added the
 * relevant metadata to the Linux kernel, with the result that current
 * versions of shim are incapable of booting current versions of the
 * Linux kernel.
 *
 * Experience shows that there is unfortunately no point in trying to
 * get a fix for this upstreamed into shim.  We therefore default to
 * working around this undesirable behaviour by patching data read
 * from the "SbatLevel" variable used to hold SBAT configuration.
 *
 * This option may be used to allow shim unpatched access to the
 * "SbatLevel" variable, in case this behaviour is ever desirable.
 */
int efi_shim_allow_sbat = 0;

/** UEFI shim image */
struct image_tag efi_shim __image_tag = {
	.name = "SHIM",
};

/** Original GetMemoryMap() function */
static EFI_GET_MEMORY_MAP efi_shim_orig_get_memory_map;

/** Original SetVariable() function */
static EFI_SET_VARIABLE efi_shim_orig_set_variable;

/** Original GetVariable() function */
static EFI_GET_VARIABLE efi_shim_orig_get_variable;

/** Verify read from SbatLevel variable */
static int efi_shim_sbatlevel_verify;

/**
 * Check if variable is SbatLevel
 *
 * @v name		Variable name
 * @v guid		Variable namespace GUID
 * @ret is_sbatlevel	Variable is SbatLevel
 */
static int efi_shim_is_sbatlevel ( const CHAR16 *name, const EFI_GUID *guid ) {
	static CHAR16 sbatlevel[] = L"SbatLevel";
	EFI_GUID *shimlock = &efi_shim_lock_protocol_guid;

	return ( ( memcmp ( name, sbatlevel, sizeof ( sbatlevel ) ) == 0 ) &&
		 ( memcmp ( guid, shimlock, sizeof ( *shimlock ) ) == 0 ) );
}

/**
 * Unlock UEFI shim
 *
 */
static void efi_shim_unlock ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	uint8_t empty[0];
	union {
		EFI_SHIM_LOCK_PROTOCOL *lock;
		void *interface;
	} u;
	EFI_STATUS efirc;

	/* Locate shim lock protocol */
	if ( ( efirc = bs->LocateProtocol ( &efi_shim_lock_protocol_guid,
					    NULL, &u.interface ) ) == 0 ) {
		u.lock->Verify ( empty, sizeof ( empty ) );
		DBGC ( &efi_shim, "SHIM unlocked via %p\n", u.lock );
	}
}

/**
 * Wrap SetVariable()
 *
 * @v name		Variable name
 * @v guid		Variable namespace GUID
 * @v attrs		Attributes
 * @v len		Buffer size
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_shim_set_variable ( CHAR16 *name, EFI_GUID *guid, UINT32 attrs,
			UINTN len, VOID *data ) {
	EFI_STATUS efirc;

	/* Call original SetVariable() */
	efirc = efi_shim_orig_set_variable ( name, guid, attrs, len, data );

	/* Allow verification of SbatLevel variable content */
	if ( efi_shim_is_sbatlevel ( name, guid ) && ( efirc == 0 ) ) {
		DBGC ( &efi_shim, "SHIM detected write to %ls:\n", name );
		DBGC_HDA ( &efi_shim, 0, data, len );
		efi_shim_sbatlevel_verify = 1;
	}

	return efirc;
}

/**
 * Wrap GetVariable()
 *
 * @v name		Variable name
 * @v guid		Variable namespace GUID
 * @v attrs		Attributes to fill in
 * @v len		Buffer size
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_shim_get_variable ( CHAR16 *name, EFI_GUID *guid, UINT32 *attrs,
			UINTN *len, VOID *data ) {
	char *value = data;
	EFI_STATUS efirc;

	/* Call original GetVariable() */
	efirc = efi_shim_orig_get_variable ( name, guid, attrs, len, data );

	/* Patch SbatLevel variable if applicable */
	if ( efi_shim_is_sbatlevel ( name, guid ) && data && ( efirc == 0 ) ) {
		if ( efi_shim_allow_sbat ) {
			DBGC ( &efi_shim, "SHIM allowing read from %ls:\n",
			       name );
		} else if ( efi_shim_sbatlevel_verify ) {
			DBGC ( &efi_shim, "SHIM allowing one read from %ls:\n",
			       name );
			efi_shim_sbatlevel_verify = 0;
		} else {
			DBGC ( &efi_shim, "SHIM patching read from %ls:\n",
			       name );
			value[0] = '\0';
		}
		DBGC_HDA ( &efi_shim, 0, data, *len );
	}

	return efirc;
}

/**
 * Wrap GetMemoryMap()
 *
 * @v len		Memory map size
 * @v map		Memory map
 * @v key		Memory map key
 * @v desclen		Descriptor size
 * @v descver		Descriptor version
 * @ret efirc		EFI status code
 */
static EFIAPI EFI_STATUS efi_shim_get_memory_map ( UINTN *len,
						   EFI_MEMORY_DESCRIPTOR *map,
						   UINTN *key, UINTN *desclen,
						   UINT32 *descver ) {
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;

	/* Unlock shim */
	if ( ! efi_shim_require_loader )
		efi_shim_unlock();

	/* Uninstall runtime services wrappers, if still installed */
	if ( rs->SetVariable == efi_shim_set_variable ) {
		rs->SetVariable = efi_shim_orig_set_variable;
		DBGC ( &efi_shim, "SHIM uninstalled SetVariable() wrapper\n" );
	} else if ( rs->SetVariable != efi_shim_orig_set_variable ) {
		DBGC ( &efi_shim, "SHIM could not uninstall SetVariable() "
		       "wrapper!\n" );
	}
	if ( rs->GetVariable == efi_shim_get_variable ) {
		rs->GetVariable = efi_shim_orig_get_variable;
		DBGC ( &efi_shim, "SHIM uninstalled GetVariable() wrapper\n" );
	} else if ( rs->GetVariable != efi_shim_orig_get_variable ) {
		DBGC ( &efi_shim, "SHIM could not uninstall GetVariable() "
		       "wrapper!\n" );
	}

	/* Hand off to original GetMemoryMap() */
	return efi_shim_orig_get_memory_map ( len, map, key, desclen,
					      descver );
}

/**
 * Inhibit use of PXE base code
 *
 * @v handle		EFI handle
 * @ret rc		Return status code
 */
static int efi_shim_inhibit_pxe ( EFI_HANDLE handle ) {
	EFI_PXE_BASE_CODE_PROTOCOL *pxe;
	EFI_STATUS efirc;
	int rc;

	/* Locate PXE base code */
	if ( ( rc = efi_open ( handle, &efi_pxe_base_code_protocol_guid,
			       &pxe ) ) != 0 ) {
		DBGC ( &efi_shim, "SHIM could not open PXE base code: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* Stop PXE base code */
	if ( ( efirc = pxe->Stop ( pxe ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_shim, "SHIM could not stop PXE base code: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	DBGC ( &efi_shim, "SHIM stopped PXE base code\n" );
	return 0;
}

/**
 * Update command line
 *
 * @v shim		Shim image
 * @v cmdline		Command line to update
 * @ret rc		Return status code
 */
static int efi_shim_cmdline ( struct image *shim, wchar_t **cmdline ) {
	wchar_t *shimcmdline;
	int len;
	int rc;

	/* Construct new command line */
	len = ( shim->cmdline ?
		efi_asprintf ( &shimcmdline, "%s %s", shim->name,
			       shim->cmdline ) :
		efi_asprintf ( &shimcmdline, "%s %ls", shim->name,
			       *cmdline ) );
	if ( len < 0 ) {
		rc = len;
		DBGC ( &efi_shim, "SHIM could not construct command line: "
		       "%s\n", strerror ( rc ) );
		return rc;
	}

	/* Replace command line */
	free ( *cmdline );
	*cmdline = shimcmdline;

	return 0;
}

/**
 * Install UEFI shim special handling
 *
 * @v shim		Shim image
 * @v handle		EFI device handle
 * @v cmdline		Command line to update
 * @ret rc		Return status code
 */
int efi_shim_install ( struct image *shim, EFI_HANDLE handle,
		       wchar_t **cmdline ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;
	int rc;

	/* Stop PXE base code */
	if ( ( ! efi_shim_allow_pxe ) &&
	     ( ( rc = efi_shim_inhibit_pxe ( handle ) ) != 0 ) ) {
		return rc;
	}

	/* Update command line */
	if ( ( rc = efi_shim_cmdline ( shim, cmdline ) ) != 0 )
		return rc;

	/* Record original boot and runtime services functions */
	efi_shim_orig_get_memory_map = bs->GetMemoryMap;
	efi_shim_orig_set_variable = rs->SetVariable;
	efi_shim_orig_get_variable = rs->GetVariable;

	/* Wrap relevant boot and runtime services functions */
	bs->GetMemoryMap = efi_shim_get_memory_map;
	rs->SetVariable = efi_shim_set_variable;
	rs->GetVariable = efi_shim_get_variable;
	DBGC ( &efi_shim, "SHIM installed wrappers\n" );

	return 0;
}

/**
 * Uninstall UEFI shim special handling
 *
 */
void efi_shim_uninstall ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;

	/* Restore original boot and runtime services functions */
	bs->GetMemoryMap = efi_shim_orig_get_memory_map;
	rs->SetVariable = efi_shim_orig_set_variable;
	rs->GetVariable = efi_shim_orig_get_variable;
	DBGC ( &efi_shim, "SHIM uninstalled wrappers\n" );
}
