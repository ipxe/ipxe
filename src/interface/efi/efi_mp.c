/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * EFI multiprocessor API implementation
 *
 */

#include <string.h>
#include <errno.h>
#include <ipxe/mp.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/MpService.h>

/** EFI multiprocessor function call data */
struct efi_mp_func_data {
	/** Multiprocessor function */
	mp_addr_t func;
	/** Opaque data pointer */
	mp_addr_t opaque;
};

/** Multiprocessor services protocol */
static EFI_MP_SERVICES_PROTOCOL *efimp;
EFI_REQUEST_PROTOCOL ( EFI_MP_SERVICES_PROTOCOL, &efimp );

/**
 * Call multiprocessor function on current CPU
 *
 * @v buffer		Multiprocessor function call data
 */
static EFIAPI VOID efi_mp_call ( VOID *buffer ) {
	struct efi_mp_func_data *data = buffer;

	/* Call multiprocessor function */
	mp_call ( data->func, data->opaque );
}

/**
 * Execute a multiprocessor function on the boot processor
 *
 * @v func		Multiprocessor function
 * @v opaque		Opaque data pointer
 */
static void efi_mp_exec_boot ( mp_func_t func, void *opaque ) {
	struct efi_mp_func_data data;

	/* Construct call data */
	data.func = mp_address ( func );
	data.opaque = mp_address ( opaque );

	/* Call multiprocesor function */
	efi_mp_call ( &data );
}

/**
 * Start a multiprocessor function on all application processors
 *
 * @v func		Multiprocessor function
 * @v opaque		Opaque data pointer
 */
static void efi_mp_start_all ( mp_func_t func, void *opaque ) {
	struct efi_mp_func_data data;
	EFI_STATUS efirc;
	int rc;

	/* Do nothing if MP services is not present */
	if ( ! efimp ) {
		DBGC ( func, "EFIMP has no multiprocessor services\n" );
		return;
	}

	/* Construct call data */
	data.func = mp_address ( func );
	data.opaque = mp_address ( opaque );

	/* Start up all application processors */
	if ( ( efirc = efimp->StartupAllAPs ( efimp, efi_mp_call, FALSE, NULL,
					      0, &data, NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( func, "EFIMP could not start APs: %s\n",
		       strerror ( rc ) );
		return;
	}
}

PROVIDE_MPAPI_INLINE ( efi, mp_address );
PROVIDE_MPAPI ( efi, mp_exec_boot, efi_mp_exec_boot );
PROVIDE_MPAPI ( efi, mp_start_all, efi_mp_start_all );
