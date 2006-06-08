#include <curses.h>
#include <vsprintf.h>

/**
 * Audible signal
 *
 * @ret rc	return status code
 */
int beep ( void ) {
	printf("\a");
	return OK;
}
