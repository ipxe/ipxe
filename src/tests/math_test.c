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
 * Mathematical self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <strings.h>
#include <assert.h>
#include <ipxe/test.h>

/**
 * Force a call to the non-constant implementation of flsl()
 *
 * @v value		Value
 * @ret msb		Most significant bit set in value (LSB=1), or zero
 */
__attribute__ (( noinline )) int flsl_var ( long value ) {
	return flsl ( value );
}

/**
 * Report a flsl() test result
 *
 * @v value		Value
 * @v msb		Expected MSB
 * @v file		Test code file
 * @v line		Test code line
 */
static inline __attribute__ (( always_inline )) void
flsl_okx ( long value, int msb, const char *file, unsigned int line ) {

	/* Verify as a constant (requires to be inlined) */
	okx ( flsl ( value ) == msb, file, line );

	/* Verify as a non-constant */
	okx ( flsl_var ( value ) == msb, file, line );
}
#define flsl_ok( value, msb ) flsl_okx ( value, msb, __FILE__, __LINE__ )

/**
 * Perform mathematical self-tests
 *
 */
static void math_test_exec ( void ) {

	/* Test flsl() */
	flsl_ok ( 0, 0 );
	flsl_ok ( 1, 1 );
	flsl_ok ( 255, 8 );
	flsl_ok ( 256, 9 );
	flsl_ok ( 257, 9 );
	flsl_ok ( 0x69505845, 31 );
	flsl_ok ( -1U, ( 8 * sizeof ( int ) ) );
	flsl_ok ( -1UL, ( 8 * sizeof ( long ) ) );
}

/** Mathematical self-tests */
struct self_test math_test __self_test = {
	.name = "math",
	.exec = math_test_exec,
};
