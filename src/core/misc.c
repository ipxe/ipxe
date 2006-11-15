/**************************************************************************
MISC Support Routines
**************************************************************************/

#include "etherboot.h"
#include "console.h"

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
RANDOM - compute a random number between 0 and 2147483647L or 2147483562?
**************************************************************************/
int32_t random(void)
{
	static int32_t seed = 0;
	int32_t q;
	if (!seed) /* Initialize linear congruential generator */
		seed = currticks();
	/* simplified version of the LCG given in Bruce Schneier's
	   "Applied Cryptography" */
	q = seed/53668;
	if ((seed = 40014*(seed-53668*q) - 12211*q) < 0) seed += 2147483563L;
	return seed;
}

/**************************************************************************
SLEEP
**************************************************************************/
void sleep(int secs)
{
	unsigned long tmo;

	for (tmo = currticks()+secs*TICKS_PER_SEC; currticks() < tmo; ) {
	}
}

/**************************************************************************
INTERRUPTIBLE SLEEP
**************************************************************************/
void interruptible_sleep(int secs)
{
	printf("<sleep>\n");
	return sleep(secs);
}

/**************************************************************************
TWIDDLE
**************************************************************************/
void twiddle(void)
{
#ifdef BAR_PROGRESS
	static int count=0;
	static const char tiddles[]="-\\|/";
	static unsigned long lastticks = 0;
	unsigned long ticks;
#endif
	if ( ! as_main_program ) return;
#ifdef	BAR_PROGRESS
	/* Limit the maximum rate at which characters are printed */
	ticks = currticks();
	if ((lastticks + (TICKS_PER_SEC/18)) > ticks)
		return;
	lastticks = ticks;

	putchar(tiddles[(count++)&3]);
	putchar('\b');
#else
	putchar('.');
#endif	/* BAR_PROGRESS */
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

unsigned long strtoul ( const char *p, char **endp, int base ) {
	unsigned long ret = 0;
	unsigned int charval;

	if ( base == 0 ) {
		if ( ( p[0] == '0' ) && ( ( p[1] | 0x20 ) == 'x' ) ) {
			base = 16;
			p += 2;
		} else {
			base = 10;
		}
	}

	while ( 1 ) {
		charval = ( *p - '0' );
		if ( charval > ( 'A' - '0' - 10 ) )
			charval -= ( 'A' - '0' - 10 );
		if ( charval > ( 'a' - 'A' ) )
			charval -= ( 'a' - 'A' );
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
