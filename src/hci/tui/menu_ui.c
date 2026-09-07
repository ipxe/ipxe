/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * Menu interface
 *
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <curses.h>
#include <ipxe/keys.h>
#include <ipxe/timer.h>
#include <ipxe/console.h>
#include <ipxe/ansicol.h>
#include <ipxe/jumpscroll.h>
#include <ipxe/dynui.h>
#include <ipxe/editstring.h>

/* Screen layout */
#define TITLE_ROW	1U
#define MENU_ROW	3U
#define FILTER_MENU_ROW	5U
#define FILTER_ROW	3U
#define MENU_COL	1U
#define MENU_COLS	( COLS - 2U )
#define MENU_PAD	2U
#define FILTER_LABEL	"Search: "

/** A menu user interface */
struct menu_ui {
	/** Dynamic user interface */
	struct dynamic_ui *dynui;
	/** Visible dynamic user interface items */
	struct dynamic_item **visible;
	/** Enable interactive filtering */
	int filter;
	/** Search field has focus */
	int filter_focus;
	/** Search text */
	char *query;
	/** Editable search text */
	struct edit_string edit;
	/** First row of menu items */
	unsigned int menu_row;
	/** Maximum number of visible menu rows */
	unsigned int menu_rows;
	/** Jump scroller */
	struct jump_scroller scroll;
	/** Remaining timeout (0=indefinite) */
	unsigned long timeout;
	/** Post-activity timeout (0=indefinite) */
	unsigned long retimeout;
};

/**
 * Get visible menu item
 *
 * @v ui		Menu user interface
 * @v index		Visible index
 * @ret item		Dynamic user interface item, or NULL
 */
static struct dynamic_item * menu_item ( struct menu_ui *ui,
					 unsigned int index ) {

	if ( index >= ui->scroll.count )
		return NULL;

	return ui->visible[index];
}

/**
 * Rebuild visible menu items
 *
 * @v ui		Menu user interface
 * @v preferred		Preferred selected item, or NULL
 * @ret named_count	Number of selectable items
 */
static unsigned int rebuild_menu ( struct menu_ui *ui,
				   struct dynamic_item *preferred ) {
	struct dynamic_item *item;
	const char *query = ( ui->query ? ui->query : "" );
	unsigned int preferred_index = 0;
	unsigned int default_index = 0;
	unsigned int named_count = 0;
	unsigned int count = 0;
	int have_preferred = 0;

	list_for_each_entry ( item, &ui->dynui->items, list ) {
		if ( ! dynui_item_matches ( item, query ) )
			continue;
		ui->visible[count] = item;
		if ( item->name ) {
			if ( ! named_count )
				default_index = count;
			named_count++;
			if ( item == preferred ) {
				preferred_index = count;
				have_preferred = 1;
			}
			if ( item->flags & DYNUI_DEFAULT ) {
				default_index = count;
			}
		}
		count++;
	}

	ui->scroll.count = count;
	ui->scroll.first = 0;
	ui->scroll.current = ( have_preferred ? preferred_index : default_index );

	if ( count && named_count )
		jump_scroll ( &ui->scroll );

	return named_count;
}

/**
 * Draw filter field
 *
 * @v ui		Menu user interface
 */
static void draw_filter ( struct menu_ui *ui ) {
	const char *query = ( ui->query ? ui->query : "" );
	size_t label_len = strlen ( FILTER_LABEL );
	size_t query_len = strlen ( query );
	size_t field_cols = ( ( MENU_COLS > ( label_len + 3 ) ) ?
			      ( MENU_COLS - label_len - 3 ) : 0 );
	size_t first = 0;
	size_t shown;
	char buf[ MENU_COLS + 1 /* NUL */ ];

	if ( ui->edit.cursor > field_cols )
		first = ( ui->edit.cursor - field_cols );
	shown = ( query_len - first );
	if ( shown > field_cols )
		shown = field_cols;

	memset ( buf, ' ', ( sizeof ( buf ) - 1 ) );
	buf[ sizeof ( buf ) - 1 ] = '\0';
	buf[0] = '[';
	memcpy ( &buf[1], FILTER_LABEL, label_len );
	memcpy ( &buf[1 + label_len], &query[first], shown );
	buf[MENU_COLS - 2] = ' ';
	buf[MENU_COLS - 1] = ']';

	if ( ui->filter_focus ) {
		color_set ( CPAIR_SELECT, NULL );
		attron ( A_BOLD );
	}
	mvprintw ( FILTER_ROW, MENU_COL, "%s", buf );
	color_set ( CPAIR_NORMAL, NULL );
	attroff ( A_BOLD );

	if ( ui->filter_focus ) {
		curs_set ( 1 );
		move ( FILTER_ROW,
		       ( MENU_COL + 1 + label_len + ui->edit.cursor - first ) );
	} else {
		curs_set ( 0 );
	}
}

/**
 * Draw a numbered menu item
 *
 * @v ui		Menu user interface
 * @v index		Index
 */
static void draw_menu_item ( struct menu_ui *ui, unsigned int index ) {
	struct dynamic_item *item;
	unsigned int row_offset;
	char buf[ MENU_COLS + 1 /* NUL */ ];
	char timeout_buf[6]; /* "(xxx)" + NUL */
	size_t timeout_len;
	size_t max_len;
	size_t len;

	/* Move to start of row */
	row_offset = ( index - ui->scroll.first );
	move ( ( ui->menu_row + row_offset ), MENU_COL );

	/* Get menu item */
	item = menu_item ( ui, index );
	if ( item ) {

		/* Draw separators in a different colour */
		if ( ! item->name )
			color_set ( CPAIR_SEPARATOR, NULL );

		/* Highlight if this is the selected item */
		if ( index == ui->scroll.current ) {
			color_set ( CPAIR_SELECT, NULL );
			attron ( A_BOLD );
		}

		/* Construct row */
		memset ( buf, ' ', ( sizeof ( buf ) - 1 ) );
		buf[ sizeof ( buf ) -1 ] = '\0';
		len = strlen ( item->text );
		max_len = ( sizeof ( buf ) - 1 /* NUL */ - ( 2 * MENU_PAD ) );
		if ( len > max_len )
			len = max_len;
		memcpy ( ( buf + MENU_PAD ), item->text, len );

		/* Add timeout if applicable */
		timeout_len =
			snprintf ( timeout_buf, sizeof ( timeout_buf ), "(%ld)",
				   ( ( ui->timeout + TICKS_PER_SEC - 1 ) /
				     TICKS_PER_SEC ) );
		if ( ( index == ui->scroll.current ) && ( ui->timeout != 0 ) ) {
			memcpy ( ( buf + MENU_COLS - MENU_PAD - timeout_len ),
				 timeout_buf, timeout_len );
		}

		/* Print row */
		printw ( "%s", buf );

		/* Reset attributes */
		color_set ( CPAIR_NORMAL, NULL );
		attroff ( A_BOLD );

	} else {
		/* Clear row if there is no corresponding menu item */
		clrtoeol();
	}

	/* Move cursor back to start of row */
	move ( ( ui->menu_row + row_offset ), MENU_COL );
}

/**
 * Draw the current block of menu items
 *
 * @v ui		Menu user interface
 */
static void draw_menu_items ( struct menu_ui *ui ) {
	unsigned int i;

	/* Draw ellipses before and/or after the list as necessary */
	color_set ( CPAIR_SEPARATOR, NULL );
	mvaddstr ( ( ui->menu_row - 1 ), ( MENU_COL + MENU_PAD ),
		   ( jump_scroll_is_first ( &ui->scroll ) ? "   " : "..." ) );
	mvaddstr ( ( ui->menu_row + ui->menu_rows ),
		   ( MENU_COL + MENU_PAD ),
		   ( jump_scroll_is_last ( &ui->scroll ) ? "   " : "..." ) );
	color_set ( CPAIR_NORMAL, NULL );

	/* Draw visible items */
	for ( i = 0 ; i < ui->menu_rows ; i++ )
		draw_menu_item ( ui, ( ui->scroll.first + i ) );

	/* Explain an empty result set */
	if ( ! ui->scroll.count ) {
		color_set ( CPAIR_SEPARATOR, NULL );
		mvaddstr ( ui->menu_row, ( MENU_COL + MENU_PAD ),
			   "No matching items" );
		color_set ( CPAIR_NORMAL, NULL );
	}
}

/**
 * Find a visible item by shortcut key
 *
 * @v ui		Menu user interface
 * @v key		Shortcut key
 * @v index		Starting visible index
 * @ret item		Matching visible item, or NULL
 */
static struct dynamic_item * menu_shortcut ( struct menu_ui *ui, int key,
					     unsigned int index ) {
	struct dynamic_item *item;
	unsigned int i;

	if ( ! key )
		return NULL;

	for ( i = index ; i < ui->scroll.count ; i++ ) {
		item = menu_item ( ui, i );
		if ( key == item->shortcut )
			return item;
	}
	for ( i = 0 ; i < index ; i++ ) {
		item = menu_item ( ui, i );
		if ( key == item->shortcut )
			return item;
	}

	return NULL;
}

/**
 * Find visible index for an item
 *
 * @v ui		Menu user interface
 * @v item		Dynamic user interface item
 * @ret index		Visible index
 */
static unsigned int menu_index ( struct menu_ui *ui,
				 struct dynamic_item *item ) {
	unsigned int i;

	for ( i = 0 ; i < ui->scroll.count ; i++ ) {
		if ( menu_item ( ui, i ) == item )
			return i;
	}

	return 0;
}

/**
 * Check whether a selectable item precedes the current item
 *
 * @v ui		Menu user interface
 * @ret precedes		A selectable item precedes the current item
 */
static int menu_item_precedes ( struct menu_ui *ui ) {
	unsigned int i;

	for ( i = 0 ; i < ui->scroll.current ; i++ ) {
		if ( menu_item ( ui, i )->name )
			return 1;
	}

	return 0;
}

/**
 * Menu main loop
 *
 * @v ui		Menu user interface
 * @ret selected	Selected item
 * @ret rc		Return status code
 */
static int menu_loop ( struct menu_ui *ui, struct dynamic_item **selected ) {
	struct dynamic_item *item;
	unsigned int named_count;
	unsigned long timeout;
	unsigned int previous;
	unsigned int move;
	int key;
	int edited;
	int chosen = 0;
	int rc = 0;

	do {
		/* Record current selection */
		previous = ui->scroll.current;

		/* Calculate timeout as remainder of current second */
		timeout = ( ui->timeout % TICKS_PER_SEC );
		if ( ( timeout == 0 ) && ( ui->timeout != 0 ) )
			timeout = TICKS_PER_SEC;
		ui->timeout -= timeout;

		/* Get key */
		move = SCROLL_NONE;
		key = getkey ( timeout );
		if ( key < 0 ) {
			/* Choose default if we finally time out */
			if ( ui->timeout == 0 ) {
				/* A filtered menu must be entered explicitly */
				if ( ui->filter && ui->filter_focus ) {
					continue;
				} else if ( ui->scroll.count ) {
					chosen = 1;
				} else {
					rc = -ENOENT;
				}
			}
		} else {
			/* Reset timeout after activity */
			ui->timeout = ui->retimeout;

			/* Handle search field */
			if ( ui->filter && ui->filter_focus ) {
				switch ( key ) {
				case ESC:
				case CTRL_C:
					rc = -ECANCELED;
					break;
				case KEY_DOWN:
				case TAB:
				case CR:
				case LF:
					if ( ui->scroll.count )
						ui->filter_focus = 0;
					break;
				default:
					edited = edit_string ( &ui->edit, key );
					if ( edited < 0 ) {
						rc = edited;
					} else if ( edited == 0 ) {
						item = menu_item ( ui,
								   ui->scroll.current );
						named_count = rebuild_menu ( ui, item );
						( void ) named_count;
					}
					break;
				}
			} else {
				/* Handle scroll keys */
				if ( ui->scroll.count ) {
					move = jump_scroll_key ( &ui->scroll, key );
				} else {
					move = SCROLL_NONE;
				}

				/* Up from the first result returns to search */
				if ( ui->filter && ( key == KEY_UP ) &&
				     ( ! menu_item_precedes ( ui ) ) ) {
					ui->filter_focus = 1;
					move = SCROLL_NONE;
				}

				/* Handle other keys */
				switch ( key ) {
				case ESC:
				case CTRL_C:
					rc = -ECANCELED;
					break;
				case CR:
				case LF:
					chosen = 1;
					break;
				default:
					item = menu_shortcut ( ui, key,
							       ui->scroll.current );
					if ( item ) {
						ui->scroll.current =
							menu_index ( ui, item );
						if ( item->name ) {
							chosen = 1;
						} else {
							move = SCROLL_DOWN;
						}
					}
					break;
				}
			}
		}

		/* Move selection, if applicable */
		while ( move ) {
			move = jump_scroll_move ( &ui->scroll, move );
			item = menu_item ( ui, ui->scroll.current );
			if ( item->name )
				break;
		}

		/* Redraw */
		if ( ui->filter ) {
			if ( ui->scroll.count )
				jump_scroll ( &ui->scroll );
			draw_filter ( ui );
			draw_menu_items ( ui );
			if ( ui->scroll.count )
				draw_menu_item ( ui, ui->scroll.current );
		} else if ( ( ui->scroll.current != previous ) ||
			    ( timeout != 0 ) ) {
			draw_menu_item ( ui, previous );
			if ( jump_scroll ( &ui->scroll ) )
				draw_menu_items ( ui );
			draw_menu_item ( ui, ui->scroll.current );
		}

		/* Record selection */
		if ( ui->scroll.count ) {
			item = menu_item ( ui, ui->scroll.current );
			assert ( item != NULL );
			assert ( item->name != NULL );
			*selected = item;
		}

	} while ( ( rc == 0 ) && ! chosen );

	return rc;
}

/**
 * Show menu
 *
 * @v dynui		Dynamic user interface
 * @v timeout		Initial timeout period, in ticks (0=indefinite)
 * @v retimeout		Post-activity timeout period, in ticks (0=indefinite)
 * @ret selected	Selected item
 * @ret rc		Return status code
 */
int show_menu ( struct dynamic_ui *dynui, unsigned long timeout,
		unsigned long retimeout, const char *select,
		struct dynamic_item **selected ) {
	struct dynamic_item *item;
	struct dynamic_item *preferred = NULL;
	struct menu_ui ui;
	char buf[ MENU_COLS + 1 /* NUL */ ];
	int named_count = 0;
	int rc;

	/* Initialise UI */
	memset ( &ui, 0, sizeof ( ui ) );
	ui.dynui = dynui;
	ui.filter = dynui_has_filter_sentinel ( dynui );
	ui.filter_focus = ui.filter;
	ui.menu_row = ( ui.filter ? FILTER_MENU_ROW : MENU_ROW );
	ui.menu_rows = ( LINES - 2U - ui.menu_row );
	ui.scroll.rows = ui.menu_rows;
	ui.timeout = timeout;
	ui.retimeout = retimeout;
	ui.visible = calloc ( dynui->count, sizeof ( ui.visible[0] ) );
	if ( ! ui.visible ) {
		rc = -ENOMEM;
		goto err_visible;
	}
	ui.query = strdup ( "" );
	if ( ! ui.query ) {
		rc = -ENOMEM;
		goto err_query;
	}
	init_editstring ( &ui.edit, &ui.query );

	list_for_each_entry ( item, &dynui->items, list ) {
		if ( item->name ) {
			named_count++;
			if ( select ) {
				if ( strcmp ( select, item->name ) == 0 )
					preferred = item;
			} else {
				if ( item->flags & DYNUI_DEFAULT )
					preferred = item;
			}
		}
	}
	if ( ! named_count ) {
		/* Menus with no named items cannot be selected from,
		 * and will seriously confuse the navigation logic.
		 * Refuse to display any such menus.
		 */
		rc = -ENOENT;
		goto err_no_items;
	}
	rebuild_menu ( &ui, preferred );

	/* Initialise screen */
	initscr();
	start_color();
	color_set ( CPAIR_NORMAL, NULL );
	curs_set ( 0 );
	erase();

	/* Draw initial content */
	attron ( A_BOLD );
	snprintf ( buf, sizeof ( buf ), "%s", ui.dynui->title );
	mvprintw ( TITLE_ROW, ( ( COLS - strlen ( buf ) ) / 2 ), "%s", buf );
	attroff ( A_BOLD );
	jump_scroll ( &ui.scroll );
	if ( ui.filter )
		draw_filter ( &ui );
	draw_menu_items ( &ui );
	draw_menu_item ( &ui, ui.scroll.current );

	/* Enter main loop */
	rc = menu_loop ( &ui, selected );
	if ( rc == 0 )
		assert ( *selected );

	/* Clear screen */
	endwin();

	free ( ui.query );
	free ( ui.visible );
	return rc;

 err_no_items:
	free ( ui.query );
 err_query:
	free ( ui.visible );
 err_visible:
	return rc;
}
