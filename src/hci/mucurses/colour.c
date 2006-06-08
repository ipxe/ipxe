#include <curses.h>

/**
 * Indicates whether the underlying terminal device is capable of
 * having colours redefined
 *
 * @ret bool	returns boolean
 */
bool can_change_colour ( void ) {
	return (bool)TRUE;
}

/**
 * Identify the RGB components of a given colour value
 *
 * @v colour	colour value
 * @v *red	address to store red component
 * @v *green	address to store green component
 * @v *blue	address to store blue component
 * @ret rc	return status code
 */
int colour_content ( short colour, short *red, short *green, short *blue ) {
	/* we do not have a particularly large range of colours (3
	   primary, 3 secondary and black), so let's just put in a
	   basic switch... */
	switch(colour) {
	case COLOUR_BLACK:
		*red = 0; *green = 0; *blue = 0;
		break;
	case COLOUR_BLUE:
		*red = 0; *green = 0; *blue = 1000;
		break;
	case COLOUR_GREEN:
		*red = 0; *green = 1000; *blue = 0;
		break;
	case COLOUR_CYAN:
		*red = 0; *green = 1000; *blue = 1000;
		break;
	case COLOUR_RED:
		*red = 1000; *green = 0; *blue = 0;
		break;
	case COLOUR_MAGENTA:
		*red = 1000; *green = 0; *blue = 1000;
		break;
	case COLOUR_YELLOW:
		*red = 1000; *green = 1000; *blue = 0;
		break;
	}
	return OK;
}
