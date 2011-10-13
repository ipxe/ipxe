/*
 * Copyright (C) 2011 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Self-test infrastructure
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <ipxe/test.h>
#include <ipxe/init.h>

/** Current self-test set */
static struct self_test *current_tests;

/**
 * Report test result
 *
 * @v success		Test succeeded
 * @v file		Test code file
 * @v line		Test code line
 */
void test_ok ( int success, const char *file, unsigned int line ) {

	/* Sanity check */
	assert ( current_tests != NULL );

	/* Increment test counter */
	current_tests->total++;

	/* Report failure if applicable */
	if ( ! success ) {
		current_tests->failures++;
		printf ( "FAILURE: \"%s\" test failed at %s line %d\n",
			 current_tests->name, file, line );
	}
}

/**
 * Run self-test set
 *
 */
static void run_tests ( struct self_test *tests ) {
	unsigned int old_assertion_failures = assertion_failures;

	/* Sanity check */
	assert ( current_tests == NULL );

	/* Record current test set */
	current_tests = tests;

	/* Run tests */
	tests->exec();

	/* Clear current test set */
	current_tests = NULL;

	/* Record number of assertion failures */
	tests->assertion_failures =
		( assertion_failures - old_assertion_failures );

	/* Print test set summary */
	if ( tests->failures || tests->assertion_failures ) {
		printf ( "FAILURE: \"%s\" %d of %d tests failed",
			 tests->name, tests->failures, tests->total );
		if ( tests->assertion_failures ) {
			printf ( " with %d assertion failures",
				 tests->assertion_failures );
		}
		printf ( "\n" );
	} else {
		printf ( "OK: \"%s\" %d tests passed\n",
			 tests->name, tests->total );
	}
}

/**
 * Run all self-tests
 *
 */
static void test_init ( void ) {
	struct self_test *tests;
	unsigned int failures = 0;
	unsigned int assertions = 0;
	unsigned int total = 0;

	/* Run all compiled-in self-tests */
	printf ( "Starting self-tests\n" );
	for_each_table_entry ( tests, SELF_TESTS )
		run_tests ( tests );

	/* Print overall summary */
	for_each_table_entry ( tests, SELF_TESTS ) {
		total += tests->total;
		failures += tests->failures;
		assertions += tests->assertion_failures;
	}
	if ( failures || assertions ) {
		printf ( "FAILURE: %d of %d tests failed",
			 failures, total );
		if ( assertions ) {
			printf ( " with %d assertion failures", assertions );
		}
		printf ( "\n" );
	} else {
		printf ( "OK: all %d tests passed\n", total );
	}

	/* Lock system */
	while ( 1 ) {}
}

/** Self-test initialisation function */
struct init_fn test_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = test_init,
};

/* Drag in all applicable self-tests */
REQUIRE_OBJECT ( list_test );
