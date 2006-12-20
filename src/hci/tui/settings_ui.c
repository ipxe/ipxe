/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <unistd.h>
#include <string.h>
#include <curses.h>
#include <console.h>
#include <gpxe/settings.h>
#include <gpxe/editbox.h>

/** @file
 *
 * Configuration settings UI
 *
 */

#include <gpxe/nvo.h>
extern struct nvo_block *ugly_nvo_hack;

/* Colour pairs */
#define CPAIR_NORMAL	1
#define CPAIR_SELECT	2
#define CPAIR_EDIT	3
#define CPAIR_ALERT	4

/* Screen layout */
#define SETTINGS_LIST_ROW	3
#define SETTINGS_LIST_COL	1
#define ALERT_ROW		20

/** Layout of text within a setting widget */
struct setting_row {
	char start[0];
	char pad1[1];
	char name[15];
	char pad2[1];
	char value[60];
	char pad3[1];
	char nul;
} __attribute__ (( packed ));

/** A setting widget */
struct setting_widget {
	/** Configuration context */
	struct config_context *context;
	/** Configuration setting */
	struct config_setting *setting;
	/** Screen row */
	unsigned int row;
	/** Screen column */
	unsigned int col;
	/** Edit box widget used for editing setting */
	struct edit_box editbox;
	/** Editing active flag */
	int editing;
	/** Buffer for setting's value */
	char value[256]; /* enough size for a DHCP string */
};

/** Registered configuration settings */
static struct config_setting
config_settings[0] __table_start ( config_settings );
static struct config_setting
config_settings_end[0] __table_end ( config_settings );
#define NUM_SETTINGS ( ( unsigned ) ( config_settings_end - config_settings ) )

/**
 * Load setting widget value from configuration context
 *
 * @v widget		Setting widget
 *
 */
static void load_setting ( struct setting_widget *widget ) {

	/* Mark as not editing */
	widget->editing = 0;

	/* Read current setting value */
	if ( widget->setting->type->show ( widget->context, widget->setting,
					   widget->value,
					   sizeof ( widget->value ) ) != 0 ) {
		widget->value[0] = '\0';
	}	

	/* Initialise edit box */
	init_editbox ( &widget->editbox, widget->value,
		       sizeof ( widget->value ), NULL, widget->row,
		       ( widget->col + offsetof ( struct setting_row, value )),
		       sizeof ( ( ( struct setting_row * ) NULL )->value ) );
}

/**
 * Save setting widget value back to configuration context
 *
 * @v widget		Setting widget
 */
static int save_setting ( struct setting_widget *widget ) {
	return widget->setting->type->set ( widget->context, widget->setting,
					    widget->value );
}

/**
 * Initialise setting widget
 *
 * @v widget		Setting widget
 * @v context		Configuration context
 * @v setting		Configuration setting
 * @v row		Screen row
 * @v col		Screen column
 */
static void init_setting ( struct setting_widget *widget,
			   struct config_context *context,
			   struct config_setting *setting,
			   unsigned int row, unsigned int col ) {

	/* Initialise widget structure */
	memset ( widget, 0, sizeof ( *widget ) );
	widget->context = context;
	widget->setting = setting;
	widget->row = row;
	widget->col = col;

	/* Read current setting value */
	load_setting ( widget );
}

/**
 * Draw setting widget
 *
 * @v widget		Setting widget
 */
static void draw_setting ( struct setting_widget *widget ) {
	struct setting_row row;
	unsigned int len;
	unsigned int curs_col;
	char *value;

	/* Fill row with spaces */
	memset ( &row, ' ', sizeof ( row ) );
	row.nul = '\0';

	/* Construct dot-padded name */
	memset ( row.name, '.', sizeof ( row.name ) );
	len = strlen ( widget->setting->name );
	if ( len > sizeof ( row.name ) )
		len = sizeof ( row.name );
	memcpy ( row.name, widget->setting->name, len );

	/* Construct space-padded value */
	value = widget->value;
	if ( ! *value )
		value = "<not specified>";
	len = strlen ( value );
	if ( len > sizeof ( row.value ) )
		len = sizeof ( row.value );
	memcpy ( row.value, value, len );
	curs_col = ( widget->col + offsetof ( typeof ( row ), value )
		     + len );

	/* Print row */
	mvprintw ( widget->row, widget->col, "%s", row.start );
	move ( widget->row, curs_col );
	if ( widget->editing )
		draw_editbox ( &widget->editbox );
}

/**
 * Edit setting widget
 *
 * @v widget		Setting widget
 * @v key		Key pressed by user
 * @ret key		Key returned to application, or zero
 */
static int edit_setting ( struct setting_widget *widget, int key ) {
	widget->editing = 1;
	return edit_editbox ( &widget->editbox, key );
}

/**
 * Initialise setting widget by index
 *
 * @v widget		Setting widget
 * @v context		Configuration context
 * @v index		Index of setting with settings list
 */
static void init_setting_index ( struct setting_widget *widget,
				 struct config_context *context,
				 unsigned int index ) {
	init_setting ( widget, context, &config_settings[index],
		       ( SETTINGS_LIST_ROW + index ), SETTINGS_LIST_COL );
}

static void alert ( const char *fmt, ... ) {
	char buf[COLS];
	va_list args;
	size_t len;

	va_start ( args, fmt );
	len = vsnprintf ( buf, sizeof ( buf ), fmt, args );
	va_end ( args );

	color_set ( CPAIR_ALERT, NULL );
	mvprintw ( ALERT_ROW, ( ( COLS - len ) / 2 ), "%s", buf );
	sleep ( 2 );
	color_set ( CPAIR_NORMAL, NULL );
	move ( ALERT_ROW, 0 );
	clrtoeol();
}

static void main_loop ( struct config_context *context ) {
	struct setting_widget widget;
	unsigned int current = 0;
	unsigned int next;
	int i;
	int key;
	int rc;

	/* Print initial screen content */
	color_set ( CPAIR_NORMAL, NULL );
	for ( i = ( NUM_SETTINGS - 1 ) ; i >= 0 ; i-- ) {
		init_setting_index ( &widget, context, i );
		draw_setting ( &widget );
	}

	while ( 1 ) {
		/* Redraw current setting */
		color_set ( ( widget.editing ? CPAIR_EDIT : CPAIR_SELECT ),
			    NULL );
		draw_setting ( &widget );
		color_set ( CPAIR_NORMAL, NULL );

		key = getchar();
		if ( widget.editing ) {
			key = edit_setting ( &widget, key );
			switch ( key ) {
			case 0x0a: /* Enter */
				if ( ( rc = save_setting ( &widget ) ) != 0 ) {
					alert ( " Could not set %s: %s ",
						widget.setting->name,
						strerror ( -rc ) );
				}
				/* Fall through */
			case 0x03: /* Ctrl-C */
				load_setting ( &widget );
				break;
			default:
				/* Do nothing */
				break;
			}
		} else {
			next = current;
			switch ( key ) {
			case '+':
				if ( next < ( NUM_SETTINGS - 1 ) )
					next++;
				break;
			case '-':
				if ( next > 0 )
					next--;
				break;
			default:
				edit_setting ( &widget, key );
				break;
			}	
			if ( next != current ) {
				draw_setting ( &widget );
				init_setting_index ( &widget, context, next );
				current = next;
			}
		}
	}
	
}

void uitest ( void ) {
	struct config_context dummy_context;

	dummy_context.options = ugly_nvo_hack->options;

	initscr();
	start_color();
	init_pair ( CPAIR_NORMAL, COLOR_WHITE, COLOR_BLUE );
	init_pair ( CPAIR_SELECT, COLOR_BLACK, COLOR_WHITE );
	init_pair ( CPAIR_EDIT, COLOR_BLACK, COLOR_CYAN );
	init_pair ( CPAIR_ALERT, COLOR_WHITE, COLOR_RED );
	color_set ( CPAIR_NORMAL, NULL );
	erase();
	
	main_loop ( &dummy_context );

	endwin();
}
