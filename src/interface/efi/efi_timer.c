/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ipxe/timer.h>
#include <ipxe/init.h>
#include <ipxe/efi/efi.h>

/** @file
 *
 * iPXE timer API for EFI
 *
 */

/**
 * Number of jiffies per second
 *
 * This is a policy decision.
 */
#define EFI_JIFFIES_PER_SEC 32

/** Current tick count */
static unsigned long efi_jiffies;

/** Timer tick event */
static EFI_EVENT efi_tick_event;

/** Colour for debug messages */
#define colour &efi_jiffies

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
static void efi_udelay ( unsigned long usecs ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	if ( ( efirc = bs->Stall ( usecs ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( colour, "EFI could not delay for %ldus: %s\n",
		       usecs, strerror ( rc ) );
		/* Probably screwed */
	}
}

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 */
static unsigned long efi_currticks ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;

	/* UEFI manages to ingeniously combine the worst aspects of
	 * both polling and interrupt-driven designs.  There is no way
	 * to support proper interrupt-driven operation, since there
	 * is no way to hook in an interrupt service routine.  A
	 * mockery of interrupts is provided by UEFI timers, which
	 * trigger at a preset rate and can fire at any time.
	 *
	 * We therefore have all of the downsides of a polling design
	 * (inefficiency and inability to sleep until something
	 * interesting happens) combined with all of the downsides of
	 * an interrupt-driven design (the complexity of code that
	 * could be preempted at any time).
	 *
	 * The UEFI specification expects us to litter the entire
	 * codebase with calls to RaiseTPL() as needed for sections of
	 * code that are not reentrant.  Since this doesn't actually
	 * gain us any substantive benefits (since even with such
	 * calls we would still be suffering from the limitations of a
	 * polling design), we instead choose to run at TPL_CALLBACK
	 * almost all of the time, dropping to TPL_APPLICATION to
	 * allow timer ticks to occur.
	 *
	 *
	 * For added excitement, UEFI provides no clean way for device
	 * drivers to shut down in preparation for handover to a
	 * booted operating system.  The platform firmware simply
	 * doesn't bother to call the drivers' Stop() methods.
	 * Instead, all non-trivial drivers must register an
	 * EVT_SIGNAL_EXIT_BOOT_SERVICES event to be signalled when
	 * ExitBootServices() is called, and clean up without any
	 * reference to the EFI driver model.
	 *
	 * Unfortunately, all timers silently stop working when
	 * ExitBootServices() is called.  Even more unfortunately, and
	 * for no discernible reason, this happens before any
	 * EVT_SIGNAL_EXIT_BOOT_SERVICES events are signalled.  The
	 * net effect of this entertaining design choice is that any
	 * timeout loops on the shutdown path (e.g. for gracefully
	 * closing outstanding TCP connections) may wait indefinitely.
	 *
	 * There is no way to report failure from currticks(), since
	 * the API lazily assumes that the host system continues to
	 * travel through time in the usual direction.  Work around
	 * EFI's violation of this assumption by falling back to a
	 * simple free-running monotonic counter during shutdown.
	 */
	if ( efi_shutdown_in_progress ) {
		efi_jiffies++;
	} else {
		bs->RestoreTPL ( TPL_APPLICATION );
		bs->RaiseTPL ( TPL_CALLBACK );
	}

	return ( efi_jiffies * ( TICKS_PER_SEC / EFI_JIFFIES_PER_SEC ) );
}

/**
 * Timer tick
 *
 * @v event		Timer tick event
 * @v context		Event context
 */
static EFIAPI void efi_tick ( EFI_EVENT event __unused,
			      void *context __unused ) {

	/* Increment tick count */
	efi_jiffies++;
}

/**
 * Start timer tick
 *
 */
static void efi_tick_startup ( void ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Create timer tick event */
	if ( ( efirc = bs->CreateEvent ( ( EVT_TIMER | EVT_NOTIFY_SIGNAL ),
					 TPL_CALLBACK, efi_tick, NULL,
					 &efi_tick_event ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( colour, "EFI could not create timer tick: %s\n",
		       strerror ( rc ) );
		/* Nothing we can do about it */
		return;
	}

	/* Start timer tick */
	if ( ( efirc = bs->SetTimer ( efi_tick_event, TimerPeriodic,
				      ( 10000000 / EFI_JIFFIES_PER_SEC ) ))!=0){
		rc = -EEFI ( efirc );
		DBGC ( colour, "EFI could not start timer tick: %s\n",
		       strerror ( rc ) );
		/* Nothing we can do about it */
		return;
	}
	DBGC ( colour, "EFI timer started at %d ticks per second\n",
	       EFI_JIFFIES_PER_SEC );
}

/**
 * Stop timer tick
 *
 * @v booting		System is shutting down in order to boot
 */
static void efi_tick_shutdown ( int booting __unused ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Stop timer tick */
	if ( ( efirc = bs->SetTimer ( efi_tick_event, TimerCancel, 0 ) ) != 0 ){
		rc = -EEFI ( efirc );
		DBGC ( colour, "EFI could not stop timer tick: %s\n",
		       strerror ( rc ) );
		/* Self-destruct initiated */
		return;
	}
	DBGC ( colour, "EFI timer stopped\n" );

	/* Destroy timer tick event */
	if ( ( efirc = bs->CloseEvent ( efi_tick_event ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( colour, "EFI could not destroy timer tick: %s\n",
		       strerror ( rc ) );
		/* Probably non-fatal */
		return;
	}
}

/** Timer tick startup function */
struct startup_fn efi_tick_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.startup = efi_tick_startup,
	.shutdown = efi_tick_shutdown,
};

/** EFI timer */
struct timer efi_timer __timer ( TIMER_NORMAL ) = {
	.name = "efi",
	.currticks = efi_currticks,
	.udelay = efi_udelay,
};
