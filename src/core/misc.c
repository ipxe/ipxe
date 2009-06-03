/**************************************************************************
MISC Support Routines
**************************************************************************/

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <ctype.h>
#include <byteswap.h>
#include <gpxe/in.h>
#include <gpxe/timer.h>

/**************************************************************************
INET_ATON - Convert an ascii x.x.x.x to binary form
**************************************************************************/
int inet_aton ( const char *cp, struct in_addr *inp ) {
	const char *p = cp;
	const char *digits_start;
	unsigned long ip = 0;
	unsigned long val;
	int j;
	for(j = 0; j <= 3; j++) {
		digits_start = p;
		val = strtoul(p, ( char ** ) &p, 10);
		if ((p == digits_start) || (val > 255)) return 0;
		if ( ( j < 3 ) && ( *(p++) != '.' ) ) return 0;
		ip = (ip << 8) | val;
	}
	if ( *p == '\0' ) {
		inp->s_addr = htonl(ip);
		return 1;
	}
	return 0;
}

unsigned long strtoul ( const char *p, char **endp, int base ) {
	unsigned long ret = 0;
	unsigned int charval;

	while ( isspace ( *p ) )
		p++;

	if ( base == 0 ) {
		base = 10;
		if ( *p == '0' ) {
			p++;
			base = 8;
			if ( ( *p | 0x20 ) == 'x' ) {
				p++;
				base = 16;
			}
		}
	}

	while ( 1 ) {
		charval = *p;
		if ( charval >= 'a' ) {
			charval = ( charval - 'a' + 10 );
		} else if ( charval >= 'A' ) {
			charval = ( charval - 'A' + 10 );
		} else if ( charval <= '9' ) {
			charval = ( charval - '0' );
		}
		if ( charval >= ( unsigned int ) base )
			break;
		ret = ( ( ret * base ) + charval );
		p++;
	}

	if ( endp )
		*endp = ( char * ) p;

	return ( ret );
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
