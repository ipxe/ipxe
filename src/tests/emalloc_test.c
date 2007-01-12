#include <vsprintf.h>
#include <gpxe/uaccess.h>
#include <gpxe/emalloc.h>
#include <gpxe/memmap.h>

void emalloc_test ( void ) {
	struct memory_map memmap;
	userptr_t bob;
	userptr_t fred;

	printf ( "Before allocation:\n" );
	get_memmap ( &memmap );

	bob = emalloc ( 1234 );
	bob = erealloc ( bob, 12345 );
	fred = emalloc ( 999 );

	printf ( "After allocation:\n" );
	get_memmap ( &memmap );

	efree ( bob );
	efree ( fred );

	printf ( "After freeing:\n" );
	get_memmap ( &memmap );
}
