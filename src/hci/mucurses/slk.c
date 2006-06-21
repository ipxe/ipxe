#include <curses.h>
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include "core.h"

/** @file
 *
 * Soft label key functions
 */

#define MIN_SPACE_SIZE 2

struct _softlabel {
	// label string
	char *label;
	/* Format of soft label 
	   0: left justify
	   1: centre justify
	   2: right justify
	 */
	unsigned short fmt;
};

struct _softlabelkeys {
	struct _softlabel fkeys[12];
	attr_t attrs;
	/* Soft label layout format
	   0: 3-2-3
	   1: 4-4
	   2: 4-4-4
	   3: 4-4-4 with index line
	*/
	unsigned short fmt;
	unsigned short max_label_len;
	unsigned short maj_space_len;
	unsigned short num_labels;
	unsigned short num_spaces;
	unsigned short *spaces;
};

struct _softlabelkeys *slks;

/*
  I either need to break the primitives here, or write a collection of
  functions specifically for SLKs that directly access the screen
  functions - since this technically isn't part of stdscr, I think
  this should be ok...
 */

static void _movetoslk ( void ) {
	stdscr->scr->movetoyx( stdscr->scr, LINES, 0 );
}

/**
 * Return the attribute used for the soft function keys
 *
 * @ret attrs	the current attributes of the soft function keys
 */
attr_t slk_attr ( void ) {
	return ( slks == NULL ? 0 : slks->attrs );
}

/**
 * Turn off soft function key attributes
 *
 * @v attrs	attribute bit mask
 * @ret rc	return status code
 */
int slk_attroff ( const chtype attrs ) {
	if ( slks == NULL ) 
		return ERR;
	slks->attrs &= ~( attrs & A_ATTRIBUTES );
	return OK;
}

/**
 * Turn on soft function key attributes
 *
 * @v attrs	attribute bit mask
 * @ret rc	return status code
 */
int slk_attron ( const chtype attrs ) {
	if ( slks == NULL )
		return ERR;
	slks->attrs |= ( attrs & A_ATTRIBUTES );
	return OK;
}

/**
 * Set soft function key attributes
 *
 * @v attrs	attribute bit mask
 * @ret rc	return status code
 */
int slk_attrset ( const chtype attrs ) {
	if ( slks == NULL ) 
		return ERR;
	slks->attrs = ( attrs & A_ATTRIBUTES );
	return OK;
}

/**
 * Turn off soft function key attributes
 *
 * @v attrs	attribute bit mask
 * @v *opts	undefined (for future implementation)
 * @ret rc	return status code
 */
int slk_attr_off ( const attr_t attrs, void *opts __unused ) {
	return slk_attroff( attrs );
}

/**
 * Turn on soft function key attributes
 *
 * @v attrs	attribute bit mask
 * @v *opts	undefined (for future implementation)
 * @ret rc	return status code
 */
int slk_attr_on ( attr_t attrs, void *opts __unused ) {
	return slk_attron( attrs );
}

/**
 * Set soft function key attributes
 *
 * @v attrs			attribute bit mask
 * @v colour_pair_number	colour pair integer
 * @v *opts			undefined (for future implementation)
 * @ret rc			return status code
 */
int slk_attr_set ( const attr_t attrs, short colour_pair_number,
		   void *opts __unused ) {
	if ( slks == NULL ) 
		return ERR;

	if ( ( unsigned short )colour_pair_number > COLORS )
		return ERR;

	slks->attrs = ( (unsigned short)colour_pair_number << CPAIR_SHIFT ) |
		( attrs & A_ATTRIBUTES );
	return OK;
}

/**
 * Clear the soft function key labels from the screen
 *
 * @ret rc	return status code
 */
int slk_clear ( void ) {
	if ( slks == NULL )
		return ERR;
	return 0;
}

/**
 * Set soft label colour pair
 */
int slk_colour ( short colour_pair_number ) {
	if ( slks == NULL ) 
		return ERR;
	if ( ( unsigned short )colour_pair_number > COLORS )
		return ERR;

	slks->attrs = ( (unsigned short)colour_pair_number << CPAIR_SHIFT )
		| ( slks->attrs & A_ATTRIBUTES );

	return OK;
}

/**
 * Initialise the soft function keys
 *
 * @v fmt	format of keys
 * @ret rc	return status code
 */
int slk_init ( int fmt ) {
	unsigned short nmaj, nmin, nblocks, available_width;

	if ( (unsigned)fmt > 3 ) {
		return ERR;
	}

	slks = malloc(sizeof(struct _softlabelkeys));
	slks->attrs = A_DEFAULT;
	slks->fmt = fmt;
	switch(fmt) {
	case 0:
		nblocks = 8; nmaj = 2; nmin = 5;
		slks->spaces = calloc(2, sizeof(unsigned short));
		slks->spaces[0] = 2; slks->spaces[1] = 4;
		break;
	case 1:
		nblocks = 8; nmaj = 1; nmin = 6;
		slks->spaces = calloc(1, sizeof(unsigned short));
		slks->spaces[0] = 3;
		break;
	case 2:
		// same allocations as format 3
	case 3:
		nblocks = 12; nmaj = 2; nmin = 9;
		slks->spaces = calloc(2, sizeof(unsigned short));
		slks->spaces[0] = 3; slks->spaces[1] = 7;
		break;
	default:
		nblocks = 0; nmaj = 0; nmin = 0;
		break;
	}

	// determine maximum label length and major space size
	available_width = COLS - ( ( MIN_SPACE_SIZE * nmaj ) + nmin );
	slks->max_label_len = available_width / nblocks;
	slks->maj_space_len = ( available_width % nblocks ) / nmaj;
	slks->num_spaces = nmaj;

	// strip a line from the screen
	LINES -= 1;

	return OK;
}

/**
 * Return the label for the specified soft key
 *
 * @v labnum	soft key identifier
 * @ret label	return label
 */
char* slk_label ( int labnum ) {
	if ( slks == NULL ) 
		return NULL;

	return slks->fkeys[labnum].label;
}

/**
 * Restore soft function key labels to the screen
 *
 * @ret rc	return status code
 */
int slk_restore ( void ) {
	unsigned short i, j,
		*next_space, *last_space;
	chtype space_ch;
	char c;

	if ( slks == NULL ) 
		return ERR;

	_movetoslk();

	space_ch = (int)' ' | slks->attrs;
	next_space = &(slks->spaces[0]);
	last_space = &(slks->spaces[slks->num_spaces-1]);

	for ( i = 0; i < slks->num_labels ; i++ ) {
		while ( ( c = *(slks->fkeys[i].label++) ) != '\0' ) {
			stdscr->scr->putc( stdscr->scr, (int)c | slks->attrs );
		}
		if ( i == *next_space ) {
			for ( j = 0; j < slks->maj_space_len; j++ )
				stdscr->scr->putc( stdscr->scr, space_ch );
			if ( next_space < last_space )
				next_space++;
		} else {
			stdscr->scr->putc( stdscr->scr, space_ch );
		}
	}

	return OK;
}

/**
 * Configure specified soft key
 *
 * @v labnum	soft label position to configure
 * @v *label	string to use as soft key label
 * @v fmt	justification format of label
 * @ret rc	return status code
 */
int slk_set ( int labnum, const char *label, int fmt ) {
	if ( slks == NULL ) 
		return ERR;
	if ( (unsigned short)labnum > 12 )
		return ERR;
	if ( (unsigned short)fmt >= 3 )
		return ERR;
	if ( strlen(label) > slks->max_label_len )
		return ERR;

	strcpy( slks->fkeys[labnum].label, label );
	slks->fkeys[labnum].fmt = fmt;

	return OK;
}
