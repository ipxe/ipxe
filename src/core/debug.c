#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <gpxe/io.h>
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

/**
 * Print row of a hex dump with specified display address
 *
 * @v dispaddr		Display address
 * @v data		Data to print
 * @v len		Length of data
 * @v offset		Starting offset within data
 */
static void dbg_hex_dump_da_row ( unsigned long dispaddr, const void *data,
				  unsigned long len, unsigned int offset ) {
	const uint8_t *bytes = data;
	unsigned int i;
	uint8_t byte;

	printf ( "%08lx :", ( dispaddr + offset ) );
	for ( i = offset ; i < ( offset + 16 ) ; i++ ) {
		if ( i >= len ) {
			printf ( "   " );
			continue;
		}
		printf ( "%c%02x",
			 ( ( ( i % 16 ) == 8 ) ? '-' : ' ' ), bytes[i] );
	}
	printf ( " : " );
	for ( i = offset ; i < ( offset + 16 ) ; i++ ) {
		if ( i >= len ) {
			printf ( " " );
			continue;
		}
		byte = bytes[i];
		if ( ( byte < 0x20 ) || ( byte >= 0x7f ) )
			byte = '.';
		printf ( "%c", byte );
	}
	printf ( "\n" );
}

/**
 * Print hex dump with specified display address
 *
 * @v dispaddr		Display address
 * @v data		Data to print
 * @v len		Length of data
 */
void dbg_hex_dump_da ( unsigned long dispaddr, const void *data,
		       unsigned long len ) {
	unsigned int offset;

	for ( offset = 0 ; offset < len ; offset += 16 ) {
		dbg_hex_dump_da_row ( dispaddr, data, len, offset );
	}
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
			printf ( "--- offset %#x ", offset );
		} else if ( ( in_corruption != 0 ) &&
			    ( test == GUARD_SYMBOL ) ) {
			/* End of corruption */
			in_corruption = 0;
			printf ( "to offset %#x", offset );
		}

	}
	if ( in_corruption != 0 ) {
		printf ( "to offset %#zx (end of region)\n", len-1 );
	}
	return corrupted;
}

/**
 * Maximum number of separately coloured message streams
 *
 * Six is the realistic maximum; there are 8 basic ANSI colours, one
 * of which will be the terminal default and one of which will be
 * invisible on the terminal because it matches the background colour.
 */
#define NUM_AUTO_COLOURS 6

/** A colour assigned to an autocolourised debug message stream */
struct autocolour {
	/** Message stream ID */
	unsigned long stream;
	/** Last recorded usage */
	unsigned long last_used;
};

/**
 * Choose colour index for debug autocolourisation
 *
 * @v stream		Message stream ID
 * @ret colour		Colour ID
 */
static int dbg_autocolour ( unsigned long stream ) {
	static struct autocolour acs[NUM_AUTO_COLOURS];
	static unsigned long use;
	unsigned int i;
	unsigned int oldest;
	unsigned int oldest_last_used;

	/* Increment usage iteration counter */
	use++;

	/* Scan through list for a currently assigned colour */
	for ( i = 0 ; i < ( sizeof ( acs ) / sizeof ( acs[0] ) ) ; i++ ) {
		if ( acs[i].stream == stream ) {
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
	acs[oldest].stream = stream;
	acs[oldest].last_used = use;
	return oldest;
}

/**
 * Select automatic colour for debug messages
 *
 * @v stream		Message stream ID
 */
void dbg_autocolourise ( unsigned long stream ) {
	printf ( "\033[%dm",
		 ( stream ? ( 31 + dbg_autocolour ( stream ) ) : 0 ) );
}

/**
 * Revert to normal colour
 *
 */
void dbg_decolourise ( void ) {
	printf ( "\033[0m" );
}
