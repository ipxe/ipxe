#ifndef _IPXE_JUMPSCROLL_H
#define _IPXE_JUMPSCROLL_H

/** @file
 *
 * Jump scrolling
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** A jump scroller */
struct jump_scroller {
	/** Maximum number of visible rows */
	unsigned int rows;
	/** Total number of items */
	unsigned int count;
	/** Currently selected item */
	unsigned int current;
	/** First visible item */
	unsigned int first;
};

/**
 * Construct scroll movement
 *
 * @v delta		Change in scroller position
 * @ret move		Scroll movement
 */
#define SCROLL( delta ) ( ( unsigned int ) ( uint16_t ) ( int16_t ) (delta) )

/**
 * Extract change in scroller position
 *
 * @v move		Scroll movement
 * @ret delta		Change in scroller position
 */
#define SCROLL_DELTA( scroll ) ( ( int16_t ) ( (scroll) & 0x0000ffffUL ) )

/** Scroll movement flags */
#define SCROLL_FLAGS	0xffff0000UL
#define SCROLL_WRAP	0x80000000UL	/**< Wrap around scrolling */

/** Do not scroll */
#define SCROLL_NONE SCROLL ( 0 )

/** Scroll up by one line */
#define SCROLL_UP SCROLL ( -1 )

/** Scroll down by one line */
#define SCROLL_DOWN SCROLL ( +1 )

/**
 * Check if jump scroller is currently on first page
 *
 * @v scroll		Jump scroller
 * @ret is_first	Scroller is currently on first page
 */
static inline int jump_scroll_is_first ( struct jump_scroller *scroll ) {

	return ( scroll->first == 0 );
}

/**
 * Check if jump scroller is currently on last page
 *
 * @v scroll		Jump scroller
 * @ret is_last		Scroller is currently on last page
 */
static inline int jump_scroll_is_last ( struct jump_scroller *scroll ) {

	return ( ( scroll->first + scroll->rows ) >= scroll->count );
}

extern unsigned int jump_scroll_key ( struct jump_scroller *scroll, int key );
extern unsigned int jump_scroll_move ( struct jump_scroller *scroll,
				       unsigned int move );
extern int jump_scroll ( struct jump_scroller *scroll );

#endif /* _IPXE_JUMPSCROLL_H */
