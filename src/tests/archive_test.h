#ifndef _ARCHIVE_TEST_H
#define _ARCHIVE_TEST_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/test.h>

/** An archive extraction test */
struct archive_test {
	/** Archive image type */
	const struct image_type *type;
	/** Archive image filename */
	const char *archive_name;
	/** Archive image data */
	const void *archive;
	/** Length of archive image data */
	size_t archive_len;
	/** Explicit name for extraction (or NULL) */
	const char *extract_name;
	/** Expected extracted image name */
	const char *expected_name;
	/** Expected extracted image data */
	const void *expected;
	/** Length of expected extracted image data */
	size_t expected_len;
};

/** Define inline archive image content */
#define ARCHIVE(...) { __VA_ARGS__ }

/** Define expected extracted image content
 *
 * For the sake of legibility, we use text strings rather than
 * arbitrary binary data.
 */
#define EXPECTED( text ) text

/** Define an archive extraction test */
#define ARCHIVE_TEST( name, TYPE, ARCHIVE_NAME, EXTRACT_NAME,		\
		      EXPECTED_NAME, ARCHIVE, EXPECTED )		\
	static const uint8_t name ## _archive[] = ARCHIVE;		\
	static const uint8_t name ## _expected				\
		[ sizeof ( EXPECTED ) - 1 ] = EXPECTED;			\
	static struct archive_test name = {				\
		.type = TYPE,						\
		.archive_name = ARCHIVE_NAME,				\
		.archive = name ## _archive,				\
		.archive_len = sizeof ( name ## _archive ),		\
		.extract_name = EXTRACT_NAME,				\
		.expected_name = EXPECTED_NAME,				\
		.expected = name ## _expected,				\
		.expected_len = sizeof ( name ## _expected ),		\
	};

/**
 * Report an archive extraction test result
 *
 * @v test		Archive extraction test
 */
#define archive_ok( test ) archive_okx ( test, __FILE__, __LINE__ )

extern void archive_okx ( struct archive_test *test, const char *file,
			  unsigned int line );

#endif /* _ARCHIVE_TEST_H */
