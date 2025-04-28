/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
 * EFI reboot mechanism
 *
 */

#include <errno.h>
#include <string.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Guid/GlobalVariable.h>
#include <ipxe/reboot.h>

/**
 * Reboot system
 *
 * @v flags		Reboot flags
 */
static void efi_reboot ( int flags ) {
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;
	static CHAR16 wname[] = EFI_OS_INDICATIONS_VARIABLE_NAME;
	UINT64 osind;
	UINT32 attrs;
	EFI_RESET_TYPE type;
	EFI_STATUS efirc;
	int rc;

	/* Request boot to firmware setup, if applicable */
	if ( flags & REBOOT_SETUP ) {
		osind = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
		attrs = ( EFI_VARIABLE_BOOTSERVICE_ACCESS |
			  EFI_VARIABLE_RUNTIME_ACCESS |
			  EFI_VARIABLE_NON_VOLATILE );
		if ( ( efirc = rs->SetVariable ( wname, &efi_global_variable,
						 attrs, sizeof ( osind ),
						 &osind ) ) != 0 ) {
			rc = -EEFI ( efirc );
			DBGC ( efi_systab, "EFI could not set %ls: %s\n",
			       wname, strerror ( rc ) );
			/* Continue to reboot anyway */
		}
	}

	/* Use runtime services to reset system */
	type = ( ( flags & REBOOT_WARM ) ? EfiResetWarm : EfiResetCold );
	rs->ResetSystem ( type, 0, 0, NULL );
}

/**
 * Power off system
 *
 * @ret rc		Return status code
 */
static int efi_poweroff ( void ) {
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;

	/* Use runtime services to power off system */
	rs->ResetSystem ( EfiResetShutdown, 0, 0, NULL );

	/* Should never happen */
	return -ECANCELED;
}

PROVIDE_REBOOT ( efi, reboot, efi_reboot );
PROVIDE_REBOOT ( efi, poweroff, efi_poweroff );
