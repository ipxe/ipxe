#ifndef _IPXE_RBG_H
#define _IPXE_RBG_H

/** @file
 *
 * RBG mechanism
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/drbg.h>

/** An RBG */
struct random_bit_generator {
	/** DRBG state */
	struct drbg_state state;
	/** Startup has been attempted */
	int started;
};

extern struct random_bit_generator rbg;

extern int rbg_generate ( const void *additional, size_t additional_len,
			  int prediction_resist, void *data, size_t len );

#endif /* _IPXE_RBG_H */
