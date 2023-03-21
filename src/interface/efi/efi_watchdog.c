/*
 * Copyright (C) 2015 Michael Brown <mbrown@fensystems.co.uk>.
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
 * EFI watchdog holdoff timer
 *
 */

#include <errno.h>
#include <string.h>
#include <ipxe/retry.h>
#include <ipxe/timer.h>
#include <ipxe/init.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/efi_watchdog.h>

/** Watchdog holdoff interval (in seconds) */
#define WATCHDOG_HOLDOFF_SECS 10

/** Watchdog timeout (in seconds) */
#define WATCHDOG_TIMEOUT_SECS ( 5 * 60 )

/** Watchdog code (to be logged on watchdog timeout) */
#define WATCHDOG_CODE 0x6950584544454144ULL

/** Watchdog data (to be logged on watchdog timeout) */
#define WATCHDOG_DATA L"iPXE";

/**
 * Hold off watchdog timer
 *
 * @v retry		Retry timer
 * @v over		Failure indicator
 */
static void efi_watchdog_expired ( struct retry_timer *timer,
				   int over __unused ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	static CHAR16 data[] = WATCHDOG_DATA;
	EFI_STATUS efirc;
	int rc;

	DBGC2 ( timer, "EFI holding off watchdog timer\n" );

	/* Restart this holdoff timer */
	start_timer_fixed ( timer, ( WATCHDOG_HOLDOFF_SECS * TICKS_PER_SEC ) );

	/* Reset watchdog timer */
	if ( ( efirc = bs->SetWatchdogTimer ( WATCHDOG_TIMEOUT_SECS,
					      WATCHDOG_CODE, sizeof ( data ),
					      data ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( timer, "EFI could not set watchdog timer: %s\n",
		       strerror ( rc ) );
		return;
	}
}

/** Watchdog holdoff timer */
struct retry_timer efi_watchdog = TIMER_INIT ( efi_watchdog_expired );

/**
 * Disable watching when shutting down to boot an operating system
 *
 * @v booting		System is shutting down for OS boot
 */
static void efi_watchdog_shutdown ( int booting ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* If we are shutting down to boot an operating system, then
	 * disable the boot services watchdog timer.  The UEFI
	 * specification mandates that the platform firmware does this
	 * as part of the ExitBootServices() call, but some platforms
	 * (e.g. Hyper-V) are observed to occasionally forget to do
	 * so, resulting in a reboot approximately five minutes after
	 * starting the operating system.
	 */
	if ( booting &&
	     ( ( efirc = bs->SetWatchdogTimer ( 0, 0, 0, NULL ) ) != 0 ) ) {
		rc = -EEFI ( efirc );
		DBGC ( &efi_watchdog, "EFI could not disable watchdog timer: "
		       "%s\n", strerror ( rc ) );
		/* Nothing we can do */
	}
}

/** Watchdog startup/shutdown function */
struct startup_fn efi_watchdog_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "efi_watchdog",
	.shutdown = efi_watchdog_shutdown,
};
