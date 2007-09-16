#include <strings.h>

int __flsl ( long x ) {
	int r = 0;

	for ( r = 0 ; x ; r++ ) {
		x >>= 1;
	}
	return r;
}
