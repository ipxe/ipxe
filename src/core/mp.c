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
 * Multiprocessor functions
 *
 */

#include <ipxe/timer.h>
#include <ipxe/mp.h>

/** Time to wait for application processors */
#define MP_MAX_CPUID_WAIT_MS 10

/**
 * Get boot CPU identifier
 *
 * @ret id		Boot CPU identifier
 */
unsigned int mp_boot_cpuid ( void ) {
	unsigned int max = 0;

	/* Update maximum to accommodate boot processor */
	mp_exec_boot ( mp_update_max_cpuid, &max );
	DBGC ( &mp_call, "MP boot processor ID is %#x\n", max );

	return max;
}

/**
 * Get maximum CPU identifier
 *
 * @ret max		Maximum CPU identifier
 */
unsigned int mp_max_cpuid ( void ) {
	unsigned int max = mp_boot_cpuid();

	/* Update maximum to accommodate application processors */
	mp_start_all ( mp_update_max_cpuid, &max );
	mdelay ( MP_MAX_CPUID_WAIT_MS );
	DBGC ( &mp_call, "MP observed maximum CPU ID is %#x\n", max );

	return max;
}
