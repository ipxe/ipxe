/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <ipxe/cpuid.h>

/** @file
 *
 * x86 CPU feature detection
 *
 */

/** Colour for debug messages */
#define colour 0x861d

/**
 * Check whether or not CPUID instruction is supported
 *
 * @ret rc		Return status code
 */
static int cpuid_instruction_supported ( void ) {
	unsigned long original;
	unsigned long inverted;

	/* Check for instruction existence via flag modifiability */
	__asm__ ( "pushf\n\t"
		  "pushf\n\t"
		  "pop %0\n\t"
		  "mov %0,%1\n\t"
		  "xor %2,%1\n\t"
		  "push %1\n\t"
		  "popf\n\t"
		  "pushf\n\t"
		  "pop %1\n\t"
		  "popf\n\t"
		  : "=&r" ( original ), "=&r" ( inverted )
		  : "ir" ( CPUID_FLAG ) );
	if ( ! ( ( original ^ inverted ) & CPUID_FLAG ) ) {
		DBGC ( colour, "CPUID instruction is not supported\n" );
		return -ENOTSUP;
	}

	return 0;
}

/**
 * Check whether or not CPUID function is supported
 *
 * @v function		CPUID function
 * @ret rc		Return status code
 */
int cpuid_supported ( uint32_t function ) {
	uint32_t max_function;
	uint32_t discard_b;
	uint32_t discard_c;
	uint32_t discard_d;
	int rc;

	/* Check that CPUID instruction is available */
	if ( ( rc = cpuid_instruction_supported() ) != 0 )
		return rc;

	/* Find highest supported function number within this family */
	cpuid ( ( function & CPUID_EXTENDED ), 0, &max_function, &discard_b,
		&discard_c, &discard_d );

	/* Fail if maximum function number is meaningless (e.g. if we
	 * are attempting to call an extended function on a CPU which
	 * does not support them).
	 */
	if ( ( max_function & CPUID_AMD_CHECK_MASK ) !=
	     ( function & CPUID_AMD_CHECK_MASK ) ) {
		DBGC ( colour, "CPUID invalid maximum function %#08x\n",
		       max_function );
		return -EINVAL;
	}

	/* Fail if this function is not supported */
	if ( function > max_function ) {
		DBGC ( colour, "CPUID function %#08x not supported\n",
		       function );
		return -ENOTTY;
	}

	return 0;
}

/**
 * Get Intel-defined x86 CPU features
 *
 * @v features		x86 CPU features to fill in
 */
static void x86_intel_features ( struct x86_features *features ) {
	uint32_t discard_a;
	uint32_t discard_b;
	int rc;

	/* Check that features are available via CPUID */
	if ( ( rc = cpuid_supported ( CPUID_FEATURES ) ) != 0 ) {
		DBGC ( features, "CPUID has no Intel-defined features\n" );
		return;
	}

	/* Get features */
	cpuid ( CPUID_FEATURES, 0, &discard_a, &discard_b,
		&features->intel.ecx, &features->intel.edx );
	DBGC ( features, "CPUID Intel features: %%ecx=%08x, %%edx=%08x\n",
	       features->intel.ecx, features->intel.edx );

}

/**
 * Get AMD-defined x86 CPU features
 *
 * @v features		x86 CPU features to fill in
 */
static void x86_amd_features ( struct x86_features *features ) {
	uint32_t discard_a;
	uint32_t discard_b;
	int rc;

	/* Check that features are available via CPUID */
	if ( ( rc = cpuid_supported ( CPUID_AMD_FEATURES ) ) != 0 ) {
		DBGC ( features, "CPUID has no AMD-defined features\n" );
		return;
	}

	/* Get features */
	cpuid ( CPUID_AMD_FEATURES, 0, &discard_a, &discard_b,
		&features->amd.ecx, &features->amd.edx );
	DBGC ( features, "CPUID AMD features: %%ecx=%08x, %%edx=%08x\n",
	       features->amd.ecx, features->amd.edx );
}

/**
 * Get x86 CPU features
 *
 * @v features		x86 CPU features to fill in
 */
void x86_features ( struct x86_features *features ) {

	/* Clear all features */
	memset ( features, 0, sizeof ( *features ) );

	/* Get Intel-defined features */
	x86_intel_features ( features );

	/* Get AMD-defined features */
	x86_amd_features ( features );
}
