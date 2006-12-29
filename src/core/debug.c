#include <stdint.h>
#include <stdarg.h>
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

#define NUM_AUTO_COLOURS 6

struct autocolour {
	void * id;
	unsigned long last_used;
};

static int autocolourise ( void *id ) {
	static struct autocolour acs[NUM_AUTO_COLOURS];
	static unsigned long use;
	unsigned int i;
	unsigned int oldest;
	unsigned int oldest_last_used;

	/* Increment usage iteration counter */
	use++;

	/* Scan through list for a currently assigned colour */
	for ( i = 0 ; i < ( sizeof ( acs ) / sizeof ( acs[0] ) ) ; i++ ) {
		if ( acs[i].id == id ) {
			acs[i].last_used = use;
			return i;
		}
	}

	/* No colour found; evict the oldest from the list */
	oldest = 0;
	oldest_last_used = use;
	for ( i = 0 ; i < ( sizeof ( acs ) / sizeof ( acs[0] ) ) ; i++ ) {
		if ( acs[i].last_used < oldest_last_used ) {
			oldest_last_used = acs[i].last_used;
			oldest = i;
		}
	}
	acs[oldest].id = id;
	acs[oldest].last_used = use;
	return oldest;
}

/** printf() for debugging with automatic colourisation
 *
 * @v id		Message stream ID
 * @v fmt		printf() format
 * @v ...		printf() argument list
 */
void dbg_printf_autocolour ( void *id, const char *fmt, ... ) {
	va_list args;

	printf ( "\033[%dm", ( id ? ( 31 + autocolourise ( id ) ) : 0 ) );
	va_start ( args, fmt );
	vprintf ( fmt, args );
	va_end ( args );
	printf ( "\033[0m" );
}
