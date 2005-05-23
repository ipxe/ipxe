/*
 * PXE image loader for Etherboot.
 * 
 * Note: There is no signature check for PXE images because there is
 * no signature.  Well done, Intel!  Consequently, pxe_probe() must be
 * called last of all the image_probe() routines, because it will
 * *always* claim the image.
 */

#ifndef PXE_EXPORT
#error PXE_IMAGE requires PXE_EXPORT
#endif

#include "etherboot.h"
#include "pxe_callbacks.h"
#include "pxe_export.h"
#include "pxe.h"

unsigned long pxe_load_offset;

static sector_t pxe_download ( unsigned char *data,
			       unsigned int len, int eof );

static inline os_download_t pxe_probe ( unsigned char *data __unused,
					unsigned int len __unused ) {
	printf("(PXE)");
	pxe_load_offset = 0;
	return pxe_download;
}

static sector_t pxe_download ( unsigned char *data,
			       unsigned int len, int eof ) {
	unsigned long block_address = PXE_LOAD_ADDRESS + pxe_load_offset;
	PXENV_STATUS_t nbp_exit;

	/* Check segment will fit.  We can't do this in probe()
	 * because there's nothing in the non-existent header to tell
	 * us how long the image is.
	 */
	if ( ! prep_segment ( block_address, block_address + len, 
			      block_address + len,
			      pxe_load_offset, pxe_load_offset + len ) ) {
		longjmp ( restart_etherboot, -2 );
	}

	/* Load block into memory, continue loading until eof */
	memcpy ( phys_to_virt ( block_address ), data, len );
	pxe_load_offset += len;
	if ( ! eof ) { 
		return 0;
	}

	/* Start up PXE NBP */
	done ( 0 );

	/* Install and activate a PXE stack */
	pxe_stack = install_pxe_stack ( NULL );
	if ( ensure_pxe_state ( READY ) ) {
		/* Invoke the NBP */
		nbp_exit = xstartpxe();
	} else {
		/* Fake success so we tear down the stack */
		nbp_exit = PXENV_STATUS_SUCCESS;
	}

	/* NBP has three exit codes:
	 *   PXENV_STATUS_KEEP_UNDI : keep UNDI and boot next device
	 *   PXENV_STATUS_KEEP_ALL  : keep all and boot next device
	 *   anything else : remove all and boot next device
	 * 
	 * Strictly, we're meant to hand back to the BIOS, but this
	 * would prevent the useful combination of "PXE NBP fails, so
	 * let Etherboot try to boot its next device".  We therefore
	 * take liberties.
	 */
	if ( nbp_exit != PXENV_STATUS_KEEP_UNDI &&
	     nbp_exit != PXENV_STATUS_KEEP_ALL ) {
		/* Tear down PXE stack */
		remove_pxe_stack();
	}

	/* Boot next device.  Under strict PXE compliance, exit back
	 * to the BIOS, otherwise let Etherboot move to the next
	 * device.
	 */
#ifdef PXE_STRICT
	longjmp ( restart_etherboot, 255 );
#else
	longjmp ( restart_etherboot, 4 );
#endif
	
	/* Never reached; avoid compiler warning */
	return ( 0 );
}
