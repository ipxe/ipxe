#ifndef _ASN1_TEST_H
#define _ASN1_TEST_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/image.h>
#include <ipxe/sha1.h>
#include <ipxe/test.h>

/** Digest algorithm used for ASN.1 tests */
#define asn1_test_digest_algorithm sha1_algorithm

/** Digest size used for ASN.1 tests */
#define ASN1_TEST_DIGEST_SIZE SHA1_DIGEST_SIZE

/** An ASN.1 test digest */
struct asn1_test_digest {
	/** Digest value */
	uint8_t digest[ASN1_TEST_DIGEST_SIZE];
};

/** An ASN.1 test */
struct asn1_test {
	/** Image type */
	struct image_type *type;
	/** Source image */
	struct image *image;
	/** Expected digests of ASN.1 objects */
	struct asn1_test_digest *expected;
	/** Number of ASN.1 objects */
	unsigned int count;
};

/**
 * Define an ASN.1 test
 *
 * @v _name		Test name
 * @v _type		Test image file type
 * @v _file		Test image file data
 * @v ...		Expected ASN.1 object digests
 * @ret test		ASN.1 test
 */
#define ASN1( _name, _type, _file, ... )				\
	static const char _name ## __file[] = _file;			\
	static struct image _name ## __image = {			\
		.refcnt = REF_INIT ( ref_no_free ),			\
		.name = #_name,						\
		.data = ( userptr_t ) ( _name ## __file ),		\
		.len = sizeof ( _name ## __file ),			\
	};								\
	static struct asn1_test_digest _name ## _expected[] = {		\
		__VA_ARGS__						\
	};								\
	static struct asn1_test _name = {				\
		.type = _type,						\
		.image = & _name ## __image,				\
		.expected = _name ## _expected,				\
		.count = ( sizeof ( _name ## _expected ) /		\
			   sizeof ( _name ## _expected[0] ) ),		\
	};

extern void asn1_okx ( struct asn1_test *test, const char *file,
		       unsigned int line );

/**
 * Report ASN.1 test result
 *
 * @v test		ASN.1 test
 */
#define asn1_ok( test ) asn1_okx ( test, __FILE__, __LINE__ )

#endif /* _ASN1_TEST_H */
