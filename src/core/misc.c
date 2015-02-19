/**************************************************************************
MISC Support Routines
**************************************************************************/

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <ctype.h>
#include <byteswap.h>
#include <ipxe/in.h>
#include <ipxe/timer.h>

unsigned int strtoul_charval ( unsigned int charval ) {

	if ( charval >= 'a' ) {
		charval = ( charval - 'a' + 10 );
	} else if ( charval >= 'A' ) {
		charval = ( charval - 'A' + 10 );
	} else if ( charval <= '9' ) {
		charval = ( charval - '0' );
	}

	return charval;
}

unsigned long strtoul ( const char *p, char **endp, int base ) {
	unsigned long ret = 0;
	int negative = 0;
	unsigned int charval;

	while ( isspace ( *p ) )
		p++;

	if ( *p == '-' ) {
		negative = 1;
		p++;
	}

	base = strtoul_base ( &p, base );

	while ( 1 ) {
		charval = strtoul_charval ( *p );
		if ( charval >= ( unsigned int ) base )
			break;
		ret = ( ( ret * base ) + charval );
		p++;
	}

	if ( negative )
		ret = -ret;

	if ( endp )
		*endp = ( char * ) p;

	return ( ret );
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
