#include <assert.h>
#include <gpxe/buffer.h>


struct buffer_test {
	struct buffer buffer;
	const char *source;
	size_t source_len;
	char *dest;
	size_t dest_len;
};

static int test_fill_buffer ( struct buffer_test *test,
			      size_t start, size_t end ) {
	const void *data = ( test->source + start );
	size_t len = ( end - start );

	assert ( end <= test->source_len );
	assert ( end <= test->dest_len );

	fill_buffer ( &test->buffer, data, start, len );
	assert ( memcmp ( ( test->dest + start ), data, len ) == 0 );
	assert ( test->buffer.free >= end );
	return 0;
}

int test_buffer ( void ) {
	char source[123];
	char dest[123];
	struct buffer_test test;

	memset ( &test, 0, sizeof ( test ) );
	test.source = source;
	test.source_len = sizeof ( source );
	test.dest = dest;
	test.dest_len = sizeof ( dest );
	test.buffer.addr = virt_to_user ( dest );
	test.buffer.len = sizeof ( dest );

	test_fill_buffer ( &test,  20,  38 );
	test_fill_buffer ( &test,  60,  61 );
	test_fill_buffer ( &test,  38,  42 );
	test_fill_buffer ( &test,  42,  60 );
	test_fill_buffer ( &test,  16,  80 );
	test_fill_buffer ( &test,   0,  16 );
	test_fill_buffer ( &test,  99, 123 );
	test_fill_buffer ( &test,  80,  99 );

	assert ( test.buffer.fill == sizeof ( source ) );
	assert ( test.buffer.free == sizeof ( source ) );
	assert ( memcmp ( source, dest, sizeof ( source ) ) == 0 );

	return 0;
}
