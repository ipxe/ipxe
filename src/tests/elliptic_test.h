#ifndef _ELLIPTIC_TEST_H
#define _ELLIPTIC_TEST_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>

/** An elliptic curve point multiplication test */
struct elliptic_test {
	/** Elliptic curve */
	struct elliptic_curve *curve;
	/** Base point */
	const void *base;
	/** Length of base point (or 0 to use generator) */
	size_t base_len;
	/** Scalar multiple */
	const void *scalar;
	/** Length of scalar multiple */
	size_t scalar_len;
	/** Expected result point */
	const void *expected;
	/** Length of expected result point (or 0 to expect failure) */
	size_t expected_len;
};

/** Define inline base point */
#define BASE(...) { __VA_ARGS__ }

/** Define base point to be curve's generator */
#define BASE_GENERATOR BASE()

/** Define inline scalar multiple */
#define SCALAR(...) { __VA_ARGS__ }

/** Define inline expected result point */
#define EXPECTED(...) { __VA_ARGS__ }

/** Define result as an expected failure */
#define EXPECTED_FAIL EXPECTED()

/**
 * Define an elliptic curve point multiplication test
 *
 * @v name		Test name
 * @v CURVE		Elliptic curve
 * @v BASE		Base point
 * @v SCALAR		Scalar multiple
 * @v EXPECTED		Expected result point
 * @ret test		Elliptic curve point multiplication test
 */
#define ELLIPTIC_TEST( name, CURVE, BASE, SCALAR, EXPECTED )		\
	static const uint8_t name ## _base[] = BASE;			\
	static const uint8_t name ## _scalar[] = SCALAR;		\
	static const uint8_t name ## _expected[] = EXPECTED;		\
	static struct elliptic_test name = {				\
		.curve = CURVE,						\
		.base = name ## _base,					\
		.base_len = sizeof ( name ## _base ),			\
		.scalar = name ## _scalar,				\
		.scalar_len = sizeof ( name ## _scalar ),		\
		.expected = name ## _expected,				\
		.expected_len = sizeof ( name ## _expected ),		\
	};

extern void elliptic_okx ( struct elliptic_test *test, const char *file,
			   unsigned int line );

/**
 * Report an elliptic curve point multiplication test result
 *
 * @v test		Elliptic curve point multiplication test
 */
#define elliptic_ok( test ) elliptic_okx ( test, __FILE__, __LINE__ )

#endif /* _ELLIPTIC_TEST_H */
