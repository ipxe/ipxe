/** @file
 *
 * Embedded image support
 *
 * Embedded images are images built into the gPXE binary and do not require
 * fetching over the network.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <gpxe/image.h>
#include <gpxe/uaccess.h>
#include <gpxe/init.h>

/**
 * Free embedded image
 *
 * @v refcnt		Reference counter
 */
static void __attribute__ (( unused ))
embedded_image_free ( struct refcnt *refcnt __unused ) {
	/* Do nothing */
}

/* Raw image data for all embedded images */
#undef EMBED
#define EMBED( _index, _path, _name )					\
	extern char embedded_image_ ## _index ## _data[];		\
	extern char embedded_image_ ## _index ## _len[];		\
	__asm__ ( ".section \".rodata\", \"a\", @progbits\n\t"		\
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
	.refcnt = { .free = embedded_image_free, },			\
	.name = _name,							\
	.data = ( userptr_t ) ( embedded_image_ ## _index ## _data ),	\
	.len = ( size_t ) embedded_image_ ## _index ## _len,		\
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

		/* virt_to_user() cannot be used in a static
		 * initialiser, so we cast the pointer to a userptr_t
		 * in the initialiser and fix it up here.  (This will
		 * actually be a no-op on most platforms.)
		 */
		data = ( ( void * ) image->data );
		image->data = virt_to_user ( data );

		DBG ( "Embedded image \"%s\": %zd bytes at %p\n",
		      image->name, image->len, data );

		if ( ( rc = register_image ( image ) ) != 0 ) {
			DBG ( "Could not register embedded image \"%s\": "
			      "%s\n", image->name, strerror ( rc ) );
			return;
		}
	}

	/* Load the first image */
	image = &embedded_images[0];
	if ( ( rc = image_autoload ( image ) ) != 0 ) {
		DBG ( "Could not load embedded image \"%s\": %s\n",
		      image->name, strerror ( rc ) );
		return;
	}
}

/** Embedded image initialisation function */
struct init_fn embedded_init_fn __init_fn ( INIT_NORMAL ) = {
	.initialise = embedded_init,
};
