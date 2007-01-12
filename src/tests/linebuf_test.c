#include <stdint.h>
#include <string.h>
#include <vsprintf.h>
#include <gpxe/linebuf.h>

static const char data1[] = 
"Hello world\r\n"
"This is a particularly mean set of lines\n"
"with a mixture of terminators\r\r\n"
"There should be exactly one blank line above\n"
"and this line should never appear at all since it has no terminator";

void linebuf_test ( void ) {
	struct line_buffer linebuf;
	const char *data = data1;
	size_t len = ( sizeof ( data1 ) - 1 /* be mean; strip the NUL */ );
	size_t buffered;

	memset ( &linebuf, 0, sizeof ( linebuf ) );
	while ( ( buffered = line_buffer ( &linebuf, data, len ) ) != len ) {
		printf ( "\"%s\"\n", buffered_line ( &linebuf ) );
		data += buffered;
		len -= buffered;
	}

	empty_line_buffer ( &linebuf );
}
