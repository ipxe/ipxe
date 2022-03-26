/** @file
 *
 * Random number generation
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdlib.h>
#include <ipxe/timer.h>
#include <ipxe/profile.h>

static int32_t rnd_seed = 0;

/**
 * Seed the pseudo-random number generator
 *
 * @v seed		Seed value
 *
 * Will use the following entropy sources if @c seed is 0:
 *  - system time ticks
 *  - cpu profiling timestamp
 *  - address of stack variable
 *
 * The RNG implementation requires that the seed is non-zero;
 * this function guarantees that with `| 4`
 */
void srandom ( unsigned int seed ) {
	if ( ! ( rnd_seed = seed ) ) {
		rnd_seed = ( currticks()
			^ profile_timestamp()
			^ ( size_t ) &seed
		) | 4; /* Chosen by fair dice roll */
	}
	DBG ( "seed=%08x ", rnd_seed );
}

/**
 * Generate a pseudo-random number between 0 and 2147483647L or 2147483562?
 *
 * @ret rand		Pseudo-random number
 */
long int random ( void ) {
	int32_t q;

	if ( ! rnd_seed )
		/* Initialize linear congruential generator,
		   providing 0 to autoselect a seed */
		srandom ( 0 );

	/* simplified version of the LCG given in Bruce Schneier's
	   "Applied Cryptography" */
	q = ( rnd_seed / 53668 );
	rnd_seed = ( 40014 * ( rnd_seed - 53668 * q ) - 12211 * q );
	if ( rnd_seed < 0 )
		rnd_seed += 2147483563L;
	return rnd_seed;
}
