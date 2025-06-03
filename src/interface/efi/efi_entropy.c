/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <errno.h>
#include <ipxe/entropy.h>
#include <ipxe/profile.h>
#include <ipxe/efi/efi.h>

/** @file
 *
 * EFI entropy source
 *
 */

struct entropy_source efitick_entropy __entropy_source ( ENTROPY_FALLBACK );

/** Time (in 100ns units) to delay waiting for timer tick
 *
 * In theory, UEFI allows us to specify a trigger time of zero to
 * simply wait for the next timer tick.  In practice, specifying zero
 * seems to often return immediately, which produces almost no
 * entropy.  Specify a delay of 1000ns to try to force an existent
 * delay.
 */
#define EFI_ENTROPY_TRIGGER_TIME 10

/** Event used to wait for timer tick */
static EFI_EVENT tick;

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
static int efi_entropy_enable ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Drop to external TPL to allow timer tick event to take place */
	bs->RestoreTPL ( efi_external_tpl );

	/* Create timer tick event */
	if ( ( efirc = bs->CreateEvent ( EVT_TIMER, TPL_NOTIFY, NULL, NULL,
					 &tick ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &tick, "ENTROPY could not create event: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* We use essentially the same mechanism as for the BIOS
	 * RTC-based entropy source, and so assume the same
	 * min-entropy per sample.
	 */
	entropy_init ( &efitick_entropy, MIN_ENTROPY ( 1.3 ) );

	return 0;
}

/**
 * Disable entropy gathering
 *
 */
static void efi_entropy_disable ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* Close timer tick event */
	bs->CloseEvent ( tick );

	/* Return to internal TPL */
	bs->RaiseTPL ( efi_internal_tpl );
}

/**
 * Wait for a timer tick
 *
 * @ret low		CPU profiling low-order bits, or negative error
 */
static int efi_entropy_tick ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	UINTN index;
	uint16_t low;
	EFI_STATUS efirc;
	int rc;

	/* Wait for next timer tick */
	if ( ( efirc = bs->SetTimer ( tick, TimerRelative,
				      EFI_ENTROPY_TRIGGER_TIME ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &tick, "ENTROPY could not set timer: %s\n",
		       strerror ( rc ) );
		return rc;
	}
	if ( ( efirc = bs->WaitForEvent ( 1, &tick, &index ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( &tick, "ENTROPY could not wait for timer tick: %s\n",
		       strerror ( rc ) );
		return rc;
	}

	/* Get current CPU profiling timestamp low-order bits */
	low = profile_timestamp();

	return low;
}

/**
 * Get noise sample from timer ticks
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 */
static int efi_get_noise ( noise_sample_t *noise ) {
	int before;
	int after;
	int rc;

	/* Wait for a timer tick */
	before = efi_entropy_tick();
	if ( before < 0 ) {
		rc = before;
		return rc;
	}

	/* Wait for another timer tick */
	after = efi_entropy_tick();
	if ( after < 0 ) {
		rc = after;
		return rc;
	}

	/* Use TSC delta as noise sample */
	*noise = ( after - before );

	return 0;
}

/** EFI entropy source */
struct entropy_source efitick_entropy __entropy_source ( ENTROPY_FALLBACK ) = {
	.name = "efitick",
	.enable = efi_entropy_enable,
	.disable = efi_entropy_disable,
	.get_noise = efi_get_noise,
};
