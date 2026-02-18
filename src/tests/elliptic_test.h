#ifndef _ELLIPTIC_TEST_H
#define _ELLIPTIC_TEST_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>

/** An elliptic curve point multiplication test */
struct elliptic_multiply_test {
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

/** An elliptic curve point addition test */
struct elliptic_add_test {
	/** Elliptic curve */
	struct elliptic_curve *curve;
	/** Addend point */
	const void *addend;
	/** Length of addend point */
	size_t addend_len;
	/** Augend point */
	const void *augend;
	/** Length of augend point */
	size_t augend_len;
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

/** Define inline addend point */
#define ADDEND(...) { __VA_ARGS__ }

/** Define inline augend point */
#define AUGEND(...) { __VA_ARGS__ }

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
#define ELLIPTIC_MULTIPLY_TEST( name, CURVE, BASE, SCALAR, EXPECTED )	\
	static const uint8_t name ## _base[] = BASE;			\
	static const uint8_t name ## _scalar[] = SCALAR;		\
	static const uint8_t name ## _expected[] = EXPECTED;		\
	static struct elliptic_multiply_test name = {			\
		.curve = CURVE,						\
		.base = name ## _base,					\
		.base_len = sizeof ( name ## _base ),			\
		.scalar = name ## _scalar,				\
		.scalar_len = sizeof ( name ## _scalar ),		\
		.expected = name ## _expected,				\
		.expected_len = sizeof ( name ## _expected ),		\
	};

/**
 * Define an elliptic curve point addition test
 *
 * @v name		Test name
 * @v CURVE		Elliptic curve
 * @v ADDEND		Addend point
 * @v AUGEND		Augend point
 * @v EXPECTED		Expected result point
 * @ret test		Elliptic curve point multiplication test
 */
#define ELLIPTIC_ADD_TEST( name, CURVE, ADDEND, AUGEND, EXPECTED )	\
	static const uint8_t name ## _addend[] = ADDEND;		\
	static const uint8_t name ## _augend[] = AUGEND;		\
	static const uint8_t name ## _expected[] = EXPECTED;		\
	static struct elliptic_add_test name = {			\
		.curve = CURVE,						\
		.addend = name ## _addend,				\
		.addend_len = sizeof ( name ## _addend ),		\
		.augend = name ## _augend,				\
		.augend_len = sizeof ( name ## _augend ),		\
		.expected = name ## _expected,				\
		.expected_len = sizeof ( name ## _expected ),		\
	};

extern void elliptic_curve_okx ( struct elliptic_curve *curve,
				 const char *file, unsigned int line );
extern void elliptic_multiply_okx ( struct elliptic_multiply_test *test,
				    const char *file, unsigned int line );
extern void elliptic_add_okx ( struct elliptic_add_test *test,
			       const char *file, unsigned int line );

/**
 * Report an elliptic curve sanity test result
 *
 * @v curve		Elliptic curve
 */
#define elliptic_curve_ok( curve ) \
	elliptic_curve_okx ( curve, __FILE__, __LINE__ )

/**
 * Report an elliptic curve point multiplication test result
 *
 * @v test		Elliptic curve point multiplication test
 */
#define elliptic_multiply_ok( test ) \
	elliptic_multiply_okx ( test, __FILE__, __LINE__ )

/**
 * Report an elliptic curve point addition test result
 *
 * @v test		Elliptic curve point addition test
 */
#define elliptic_add_ok( test ) \
	elliptic_add_okx ( test, __FILE__, __LINE__ )

#endif /* _ELLIPTIC_TEST_H */
