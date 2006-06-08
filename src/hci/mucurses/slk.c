#include <curses.h>
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include "core.h"

extern struct _softlabelkeys *slks;

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

	wmove(stdscr,stdscr->height-1,0);
	wclrtoeol(stdscr);
	return 0;
}

/**
 * Initialise the soft function keys
 *
 * @v fmt	format of keys
 * @ret rc	return status code
 */
int slk_init ( int fmt ) {
	if ( (unsigned)fmt > 3 ) {
		return ERR;
	}

	slks = malloc(sizeof(struct _softlabelkeys));
	slks->attrs = A_DEFAULT;
	slks->fmt = (unsigned short)fmt;
	slks->maxlablen = 5;
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
	if ( slks == NULL ) 
		return ERR;

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
	if ( strlen(label) > slks->maxlablen )
		return ERR;

	strcpy( slks->fkeys[labnum].label, label );
	slks->fkeys[labnum].fmt = fmt;

	return OK;
}
