#ifndef _EXCHANGE_TEST_H
#define _EXCHANGE_TEST_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/test.h>

/** A key exchange test */
struct exchange_test {
	/** Key exchange algorithm */
	struct exchange_algorithm *exchange;
	/** Private key */
	const void *private;
	/** Length of private key */
	size_t private_len;
	/** Partner public key */
	const void *partner;
	/** Length of partner key */
	size_t partner_len;
	/** Expected public key */
	const void *public;
	/** Length of expected public key */
	size_t public_len;
	/** Expected shared secret */
	const void *shared;
	/** Length of expected shared secret, or 0 to expect failure */
	size_t shared_len;
};

/** Define inline private key */
#define PRIVATE(...) { __VA_ARGS__ }

/** Define inline partner public key */
#define PARTNER(...) { __VA_ARGS__ }

/** Define inline expected public key */
#define PUBLIC(...) { __VA_ARGS__ }

/** Define inline expected shared secret */
#define SHARED(...) { __VA_ARGS__ }

/** Define inline expected failure result */
#define SHARED_FAIL SHARED()

/**
 * Define a key exchange test
 *
 * @v name		Test name
 * @v EXCHANGE		Key exchange algorithm
 * @v PRIVATE		Private key
 * @v PARTNER		Partner public key
 * @v PUBLIC		Expected public key
 * @v SHARED		Expected shared secret
 * @ret test		Key exchange test
 */
#define EXCHANGE_TEST( name, EXCHANGE, PRIVATE, PARTNER, PUBLIC,	\
		       SHARED )						\
	static const uint8_t name ## _private[] = PRIVATE;		\
	static const uint8_t name ## _partner[] = PARTNER;		\
	static const uint8_t name ## _public[] = PUBLIC;		\
	static const uint8_t name ## _shared[] = SHARED;		\
	static struct exchange_test name = {				\
		.exchange = EXCHANGE,					\
		.private = name ## _private,				\
		.private_len = sizeof ( name ## _private ),		\
		.partner = name ## _partner,				\
		.partner_len = sizeof ( name ## _partner ),		\
		.public = name ## _public,				\
		.public_len = sizeof ( name ## _public ),		\
		.shared = name ## _shared,				\
		.shared_len = sizeof ( name ## _shared ),		\
	};

/**
 * Report a key exchange test result
 *
 * @v test		Key exchange test
 */
#define exchange_ok(test) exchange_okx ( test, __FILE__, __LINE__ )

extern void exchange_okx ( struct exchange_test *test, const char *file,
			   unsigned int line );

#endif /* _EXCHANGE_TEST_H */
