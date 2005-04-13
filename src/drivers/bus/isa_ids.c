#include "etherboot.h"
#include "isa_ids.h"

/* 
 * EISA and ISAPnP IDs are actually mildly human readable, though in a
 * somewhat brain-damaged way.
 *
 */
char * isa_id_string ( uint16_t vendor, uint16_t product ) {
	static unsigned char buf[7];
	int i;

	/* Vendor ID is a compressed ASCII string */
	vendor = __bswap_16 ( vendor );
	for ( i = 2 ; i >= 0 ; i-- ) {
		buf[i] = ( 'A' - 1 + ( vendor & 0x1f ) );
		vendor >>= 5;
	}
	
	/* Product ID is a 4-digit hex string */
	sprintf ( &buf[3], "%hx", __bswap_16 ( product ) );

	return buf;
}
