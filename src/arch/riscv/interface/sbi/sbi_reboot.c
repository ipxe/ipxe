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
 * Supervisor Binary Interface (SBI) reboot mechanism
 *
 */

#include <errno.h>
#include <string.h>
#include <ipxe/sbi.h>
#include <ipxe/reboot.h>

/**
 * Reboot system
 *
 * @v flags		Reboot flags
 */
static void sbi_reboot ( int flags ) {
	struct sbi_return ret;
	int warm;
	int rc;

	/* Reboot system */
	warm = ( flags & REBOOT_WARM );
	ret = sbi_ecall_2 ( SBI_SRST, SBI_SRST_SYSTEM_RESET,
			    ( warm ? SBI_RESET_WARM : SBI_RESET_COLD ), 0 );

	/* Any return is an error */
	rc = -ESBI ( ret.error );
	DBGC ( SBI_SRST, "SBI %s reset failed: %s\n",
	       ( warm ? "warm" : "cold" ), strerror ( rc ) );

	/* Try a legacy shutdown */
	sbi_legacy_ecall_0 ( SBI_LEGACY_SHUTDOWN );
	DBGC ( SBI_SRST, "SBI legacy shutdown failed\n" );
}

/**
 * Power off system
 *
 * @ret rc		Return status code
 */
static int sbi_poweroff ( void ) {
	struct sbi_return ret;
	int rc;

	/* Shut down system */
	ret = sbi_ecall_2 ( SBI_SRST, SBI_SRST_SYSTEM_RESET,
			    SBI_RESET_SHUTDOWN, 0 );

	/* Any return is an error */
	rc = -ESBI ( ret.error );
	DBGC ( SBI_SRST, "SBI shutdown failed: %s\n", strerror ( rc ) );

	/* Try a legacy shutdown */
	sbi_legacy_ecall_0 ( SBI_LEGACY_SHUTDOWN );
	DBGC ( SBI_SRST, "SBI legacy shutdown failed\n" );

	return rc;
}

PROVIDE_REBOOT ( sbi, reboot, sbi_reboot );
PROVIDE_REBOOT ( sbi, poweroff, sbi_poweroff );
