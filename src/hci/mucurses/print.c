#include <curses.h>
#include <vsprintf.h>
#include <stddef.h>
#include "core.h"

static printw_context {
	struct printf_context ctx;
	WINDOW *win;
};

static void _printw_handler ( struct printf_context *ctx, unsigned int c ) {
	struct printw_context *wctx =
		container_of ( ctx, struct printw_context, ctx );

	_wputch( wctx->win, c | wctx->win->attrs, WRAP );
}

/**
 * Print formatted output in a window
 *
 * @v *win	subject window
 * @v *fmt	formatted string
 * @v varglist	argument list
 * @ret rc	return status code
 */
int vw_printw ( WINDOW *win, const char *fmt, va_list varglist ) {
	struct printw_context wctx = {
		.win = win,
		.ctx = { .handler = _printw_handler, },
	};

	vcprintf ( &(wctx.ctx), fmt, varglist );
	return OK;
}

/**
 * Print formatted output to a window
 *
 * @v *win	subject window
 * @v *fmt	formatted string
 * @v ...	string arguments
 * @ret rc	return status code
 */
int wprintw ( WINDOW *win, const char *fmt, ... ) {
	va_list args;
	int i;

	va_start ( args, fmt );
	i = vw_printw ( win, fmt, args );
	va_end ( args );
	return i;
}
