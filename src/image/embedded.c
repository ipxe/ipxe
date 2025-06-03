/** @file
 *
 * Embedded image support
 *
 * Embedded images are images built into the iPXE binary and do not require
 * fetching over the network.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>
#include <ipxe/image.h>
#include <ipxe/init.h>

/* Raw image data for all embedded images */
#undef EMBED
#define EMBED( _index, _path, _name )					\
	extern char embedded_image_ ## _index ## _data[];		\
	extern size_t ABS_SYMBOL ( embedded_image_ ## _index ## _len );	\
	__asm__ ( ".section \".data\", \"aw\", " PROGBITS "\n\t"	\
		  "\nembedded_image_" #_index "_data:\n\t"		\
		  ".incbin \"" _path "\"\n\t"				\
		  "\nembedded_image_" #_index "_end:\n\t"		\
		  ".equ embedded_image_" #_index "_len, "		\
			"( embedded_image_" #_index "_end - "		\
			"  embedded_image_" #_index "_data )\n\t"	\
		  ".previous\n\t" );
EMBED_ALL

/* Image structures for all embedded images */
#undef EMBED
#define EMBED( _index, _path, _name ) {					\
	.refcnt = REF_INIT ( free_image ),				\
	.name = _name,							\
	.flags = ( IMAGE_STATIC | IMAGE_STATIC_NAME ),			\
	.rwdata = embedded_image_ ## _index ## _data,			\
	.len = ABS_VALUE_INIT ( embedded_image_ ## _index ## _len ),	\
},
static struct image embedded_images[] = {
	EMBED_ALL
};

/**
 * Register all embedded images
 */
static void embedded_init ( void ) {
	int i;
	struct image *image;
	void *data;
	int rc;

	/* Skip if we have no embedded images */
	if ( ! sizeof ( embedded_images ) )
		return;

	/* Fix up data pointers and register images */
	for ( i = 0 ; i < ( int ) ( sizeof ( embedded_images ) /
				    sizeof ( embedded_images[0] ) ) ; i++ ) {
		image = &embedded_images[i];
		DBG ( "Embedded image \"%s\": %zd bytes at %p\n",
		      image->name, image->len, data );
		if ( ( rc = register_image ( image ) ) != 0 ) {
			DBG ( "Could not register embedded image \"%s\": "
			      "%s\n", image->name, strerror ( rc ) );
			return;
		}
	}

	/* Select the first image */
	image = &embedded_images[0];
	if ( ( rc = image_select ( image ) ) != 0 ) {
		DBG ( "Could not select embedded image \"%s\": %s\n",
		      image->name, strerror ( rc ) );
		return;
	}

	/* Trust the selected image implicitly */
	image_trust ( image );
}

/** Embedded image initialisation function */
struct init_fn embedded_init_fn __init_fn ( INIT_LATE ) = {
	.initialise = embedded_init,
};
