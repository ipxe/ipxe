#ifndef _IPXE_TEST_H
#define _IPXE_TEST_H

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Self-test infrastructure
 *
 */

#include <ipxe/tables.h>

/** A self-test set */
struct self_test {
	/** Test set name */
	const char *name;
	/** Run self-tests */
	void ( * exec ) ( void );
	/** Number of tests run */
	unsigned int total;
	/** Number of test failures */
	unsigned int failures;
	/** Number of assertion failures */
	unsigned int assertion_failures;
};

/** Self-test table */
#define SELF_TESTS __table ( struct self_test, "self_tests" )

/** Declare a self-test */
#define __self_test __table_entry ( SELF_TESTS, 01 )

extern void test_ok ( int success, const char *file, unsigned int line );

/**
 * Report test result
 *
 * @v success		Test succeeded
 */
#define ok( success ) do {				\
	test_ok ( (success), __FILE__, __LINE__ );	\
	} while ( 0 )

#endif /* _IPXE_TEST_H */
