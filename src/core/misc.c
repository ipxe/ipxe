/**************************************************************************
MISC Support Routines
**************************************************************************/

#include "etherboot.h"
#include "console.h"
#include <stdlib.h>

/**************************************************************************
IPCHKSUM - Checksum IP Header
**************************************************************************/
uint16_t ipchksum(const void *data, unsigned long length)
{
	unsigned long sum;
	unsigned long i;
	const uint8_t *ptr;

	/* In the most straight forward way possible,
	 * compute an ip style checksum.
	 */
	sum = 0;
	ptr = data;
	for(i = 0; i < length; i++) {
		unsigned long value;
		value = ptr[i];
		if (i & 1) {
			value <<= 8;
		}
		/* Add the new value */
		sum += value;
		/* Wrap around the carry */
		if (sum > 0xFFFF) {
			sum = (sum + (sum >> 16)) & 0xFFFF;
		}
	}
	return (~cpu_to_le16(sum)) & 0xFFFF;
}

uint16_t add_ipchksums(unsigned long offset, uint16_t sum, uint16_t new)
{
	unsigned long checksum;
	sum = ~sum & 0xFFFF;
	new = ~new & 0xFFFF;
	if (offset & 1) {
		/* byte swap the sum if it came from an odd offset 
		 * since the computation is endian independant this
		 * works.
		 */
		new = bswap_16(new);
	}
	checksum = sum + new;
	if (checksum > 0xFFFF) {
		checksum -= 0xFFFF;
	}
	return (~checksum) & 0xFFFF;
}

/**************************************************************************
SLEEP
**************************************************************************/
unsigned int sleep(unsigned int secs)
{
	unsigned long tmo;

	for (tmo = currticks()+secs*TICKS_PER_SEC; currticks() < tmo; ) {
	}
	return 0;
}

/**************************************************************************
INTERRUPTIBLE SLEEP
**************************************************************************/
void interruptible_sleep(int secs)
{
	printf("<sleep>\n");
	sleep(secs);
}

/**************************************************************************
STRCASECMP (not entirely correct, but this will do for our purposes)
**************************************************************************/
int strcasecmp(const char *a, const char *b)
{
	while (*a && *b && (*a & ~0x20) == (*b & ~0x20)) {a++; b++; }
	return((*a & ~0x20) - (*b & ~0x20));
}

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

int isspace ( int c ) {
	switch ( c ) {
	case ' ':
	case '\f':
	case '\n':
	case '\r':
	case '\t':
	case '\v':
		return 1;
	default:
		return 0;
	}
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
		} else {
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
