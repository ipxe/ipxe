#include <stdint.h>
#include <io.h>
#include <console.h>

void pause ( void ) {
	printf ( "\nPress a key" );
	getchar();
	printf ( "\r           \r" );
}

void more ( void ) {
	printf ( "---more---" );
	getchar();
	printf ( "\r          \r" );
}

/* Produce a paged hex dump of the specified data and length */
void hex_dump ( const unsigned char *data, const unsigned int len ) {
	unsigned int index;
	for ( index = 0; index < len; index++ ) {
		if ( ( index % 16 ) == 0 ) {
			printf ( "\n" );
		}
		if ( ( index % 368 ) == 352 ) {
			more();
		}
		if ( ( index % 16 ) == 0 ) {
			printf ( "%p [%lx] : %04x :", data + index,
				 virt_to_phys ( data + index ), index );
		}
		printf ( " %02x", data[index] );
	}
	printf ( "\n" );
}

#define GUARD_SYMBOL ( ( 'M' << 24 ) | ( 'I' << 16 ) | ( 'N' << 8 ) | 'E' )
/* Fill a region with guard markers.  We use a 4-byte pattern to make
 * it less likely that check_region will find spurious 1-byte regions
 * of non-corruption.
 */
void guard_region ( void *region, size_t len ) {
	uint32_t offset = 0;

	len &= ~0x03;
	for ( offset = 0; offset < len ; offset += 4 ) {
		*((uint32_t *)(region + offset)) = GUARD_SYMBOL;
	}
}

/* Check a region that has been guarded with guard_region() for
 * corruption.
 */
int check_region ( void *region, size_t len ) {
	uint8_t corrupted = 0;
	uint8_t in_corruption = 0;
	uint32_t offset = 0;
	uint32_t test = 0;

	len &= ~0x03;
	for ( offset = 0; offset < len ; offset += 4 ) {
		test = *((uint32_t *)(region + offset)) = GUARD_SYMBOL;
		if ( ( in_corruption == 0 ) &&
		     ( test != GUARD_SYMBOL ) ) {
			/* Start of corruption */
			if ( corrupted == 0 ) {
				corrupted = 1;
				printf ( "Region %p-%p (physical %#lx-%#lx) "
					 "corrupted\n",
					 region, region + len,
					 virt_to_phys ( region ),
					 virt_to_phys ( region + len ) );
			}
			in_corruption = 1;
			printf ( "--- offset %#lx ", offset );
		} else if ( ( in_corruption != 0 ) &&
			    ( test == GUARD_SYMBOL ) ) {
			/* End of corruption */
			in_corruption = 0;
			printf ( "to offset %#lx", offset );
		}

	}
	if ( in_corruption != 0 ) {
		printf ( "to offset %#x (end of region)\n", len-1 );
	}
	return corrupted;
}
