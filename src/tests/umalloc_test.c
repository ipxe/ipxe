#include <vsprintf.h>
#include <gpxe/uaccess.h>
#include <gpxe/umalloc.h>
#include <gpxe/memmap.h>

void umalloc_test ( void ) {
	struct memory_map memmap;
	userptr_t bob;
	userptr_t fred;

	printf ( "Before allocation:\n" );
	get_memmap ( &memmap );

	bob = ymalloc ( 1234 );
	bob = yrealloc ( bob, 12345 );
	fred = ymalloc ( 999 );

	printf ( "After allocation:\n" );
	get_memmap ( &memmap );

	ufree ( bob );
	ufree ( fred );

	printf ( "After freeing:\n" );
	get_memmap ( &memmap );
}
