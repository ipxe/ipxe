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
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Real mode transition self-tests
 *
 * This file allows for easy measurement of the time taken to perform
 * real mode transitions, which may have a substantial overhead when
 * running under a hypervisor.
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <ipxe/test.h>
#include <ipxe/profile.h>
#include <realmode.h>

/** Number of sample iterations for profiling */
#define PROFILE_COUNT 4096

/** Protected-to-real mode transition profiler */
static struct profiler p2r_profiler __profiler = { .name = "p2r" };

/** Real-to-protected mode transition profiler */
static struct profiler r2p_profiler __profiler = { .name = "r2p" };

/**
 * Perform real mode transition self-tests
 *
 */
static void librm_test_exec ( void ) {
	unsigned int i;
	unsigned long p2r_elapsed;

	/* Profile mode transitions.  We want to profile each
	 * direction of the transition separately, so perform an RDTSC
	 * while in real mode and tweak the profilers' start/stop
	 * times appropriately.
	 */
	for ( i = 0 ; i < PROFILE_COUNT ; i++ ) {
		profile_start ( &p2r_profiler );
		__asm__ __volatile__ ( REAL_CODE ( "rdtsc\n\t" )
				       : "=A" ( r2p_profiler.started ) : );
		profile_stop ( &r2p_profiler );
		p2r_elapsed = ( r2p_profiler.started - p2r_profiler.started );
		profile_update ( &p2r_profiler, p2r_elapsed );
	}
}

/** Real mode transition self-test */
struct self_test librm_test __self_test = {
	.name = "librm",
	.exec = librm_test_exec,
};

REQUIRE_OBJECT ( test );
