
FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/segment.h>
#include <landing_zone.h>

struct sl_header {
	u16 lz_offet;
	u16 lz_length;
} __attribute__ (( packed ));

struct lz_header {
	u8  uuid[16];
	u32 slaunch_loader_size;
	u32 zero_page_addr;
	u8  msb_key_hash[20];
} __attribute__ (( packed ));

const unsigned char
lz_header_uuid[16] = { 0x78, 0xf1, 0x26, 0x8e, 0x04, 0x92, 0x11, 0xe9,
                       0x83, 0x2a, 0xc8, 0x5b, 0x76, 0xc4, 0xcc, 0x02 };

/**
 * Update LZ header
 *
 * @v image		LZ file
 * @v zeropage	Address of zero page
 */
void landing_zone_set_bzimage ( struct image *image, userptr_t zeropage ) {
	struct sl_header sl_hdr;
	copy_from_user ( &sl_hdr, image->data, 0, sizeof ( sl_hdr ) );
	struct lz_header hdr;
	copy_from_user ( &hdr, image->data, sl_hdr.lz_length, sizeof ( hdr ) );

	hdr.zero_page_addr = user_to_phys ( zeropage, 0 );

	DBGC ( image, "LZ %p writing zeropage address: 0x%lx\n",
			   image, user_to_phys ( zeropage, 0 ) );

	/* Write out header structure */
	copy_to_user ( image->data, sl_hdr.lz_length, &hdr, sizeof ( hdr ) );
}

/**
 * Execute Landing Zone image
 *
 * @v image		LZ image
 * @ret rc		Return status code
 */
static int lz_exec ( struct image *image ) {
	int rc;
	physaddr_t target = image->flags & ~( LZ_ALIGN - 1 );

	/* TODO: remove hardcoded values */
	/* Set APs in wait-for-SIPI state */
	*((volatile uint32_t *)phys_to_user( 0xfee00300ULL )) = 0x000c0500;

	/* Relinquish all TPM localities */
	*((volatile uint8_t *)phys_to_user(0xFED40000ULL)) = 0x20;
	*((volatile uint8_t *)phys_to_user(0xFED41000ULL)) = 0x20;
	*((volatile uint8_t *)phys_to_user(0xFED42000ULL)) = 0x20;
	*((volatile uint8_t *)phys_to_user(0xFED43000ULL)) = 0x20;
	*((volatile uint8_t *)phys_to_user(0xFED44000ULL)) = 0x20;

	/* TODO: initrds can overwrite LZ while it is temporarily unregistered.
	 * This and memcpy below should be done before that.
	 */
	if ( ( rc = prep_segment ( target, image->len, LZ_SIZE ) ) != 0 ) {
		DBGC ( image, "LZ %p could not prepare segment: %s\n",
			   image, strerror ( rc ) );
		return rc;
	}

	memcpy_user ( phys_to_user ( target ), 0, image->data, 0, image->len );

	DBGC ( image, "LZ %p performing SKINIT with eax=0x%lx now\n", image,
	       target );

	/* Fill UART buffer with padding, SKINIT may happen before it empties */
	DBGC ( image, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n" );

	__asm__ __volatile__ ( "skinit"
			       : : "a" ( target ) );

	/* There is no way for the image to return, since we provide
	 * no return address.
	 */
	assert ( 0 );

	return -12; //-ECANCELED; /* -EIMPOSSIBLE */
}

/**
 * Probe Landing Zone image
 *
 * @v image		LZ file
 * @ret rc		Return status code
 */
static int lz_probe ( struct image *image ) {
	int rc;

	struct sl_header sl_hdr;
	copy_from_user ( &sl_hdr, image->data, 0, sizeof ( sl_hdr ) );
	struct lz_header hdr;
	copy_from_user ( &hdr, image->data, sl_hdr.lz_length, sizeof ( hdr ) );

	rc = memcmp ( hdr.uuid, lz_header_uuid, sizeof ( lz_header_uuid ) );

	if ( rc == 0 )
		image_set_name ( image, "landing_zone" );

	return rc;
}

/** Landing Zone image type */
struct image_type lz_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "landing_zone",
	.probe = lz_probe,
	.exec = lz_exec,
};
