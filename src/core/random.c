/** @file
 *
 * Random number generation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stddef.h>
#include <stdlib.h>
#include <time.h>

static int32_t rnd_seed = 0;

/**
 * Seed the pseudo-random number generator
 *
 * @v seed		Seed value
 */
void srandom ( unsigned int seed ) {
	rnd_seed = seed;
	if ( ! rnd_seed )
		rnd_seed = 4; /* Chosen by fair dice roll */
}

/**
 * Generate a pseudo-random number between 0 and 2147483647L or 2147483562?
 *
 * @ret rand		Pseudo-random number
 */
long int random ( void ) {
	int32_t q;

	/* Initialize linear congruential generator */
	if ( ! rnd_seed )
		srandom ( time ( NULL ) );

	/* simplified version of the LCG given in Bruce Schneier's
	   "Applied Cryptography" */
	q = ( rnd_seed / 53668 );
	rnd_seed = ( 40014 * ( rnd_seed - 53668 * q ) - 12211 * q );
	if ( rnd_seed < 0 )
		rnd_seed += 2147483563L;
	return rnd_seed;
}
