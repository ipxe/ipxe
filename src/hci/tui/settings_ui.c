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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <curses.h>
#include <console.h>
#include <gpxe/settings.h>
#include <gpxe/editbox.h>
#include <gpxe/keys.h>
#include <gpxe/settings_ui.h>

/** @file
 *
 * Option configuration console
 *
 */

/* Colour pairs */
#define CPAIR_NORMAL	1
#define CPAIR_SELECT	2
#define CPAIR_EDIT	3
#define CPAIR_ALERT	4

/* Screen layout */
#define TITLE_ROW		1
#define SETTINGS_LIST_ROW	3
#define SETTINGS_LIST_COL	1
#define INFO_ROW		20
#define ALERT_ROW		20
#define INSTRUCTION_ROW		22
#define INSTRUCTION_PAD "     "

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
	/** Settings block */
	struct settings *settings;
	/** Configuration setting */
	struct setting *setting;
	/** Screen row */
	unsigned int row;
	/** Screen column */
	unsigned int col;
	/** Edit box widget used for editing setting */
	struct edit_box editbox;
	/** Editing in progress flag */
	int editing;
	/** Buffer for setting's value */
	char value[256]; /* enough size for a DHCP string */
};

/** Number of registered configuration settings */
#define NUM_SETTINGS table_num_entries ( SETTINGS )

static void load_setting ( struct setting_widget *widget ) __nonnull;
static int save_setting ( struct setting_widget *widget ) __nonnull;
static void init_setting ( struct setting_widget *widget,
                           struct settings *settings,
                           struct setting *setting,
                           unsigned int row, unsigned int col ) __nonnull;
static void draw_setting ( struct setting_widget *widget ) __nonnull;
static int edit_setting ( struct setting_widget *widget, int key ) __nonnull;
static void init_setting_index ( struct setting_widget *widget,
                                 struct settings *settings,
                                 unsigned int index ) __nonnull;
static void vmsg ( unsigned int row, const char *fmt, va_list args ) __nonnull;
static void msg ( unsigned int row, const char *fmt, ... ) __nonnull;
static void valert ( const char *fmt, va_list args ) __nonnull;
static void alert ( const char *fmt, ... ) __nonnull;
static void draw_info_row ( struct setting *setting ) __nonnull;
static int main_loop ( struct settings *settings ) __nonnull;

/**
 * Load setting widget value from configuration settings
 *
 * @v widget		Setting widget
 *
 */
static void load_setting ( struct setting_widget *widget ) {

	/* Mark as not editing */
	widget->editing = 0;

	/* Read current setting value */
	if ( fetchf_setting ( widget->settings, widget->setting,
			      widget->value, sizeof ( widget->value ) ) < 0 ) {
		widget->value[0] = '\0';
	}	

	/* Initialise edit box */
	init_editbox ( &widget->editbox, widget->value,
		       sizeof ( widget->value ), NULL, widget->row,
		       ( widget->col + offsetof ( struct setting_row, value )),
		       sizeof ( ( ( struct setting_row * ) NULL )->value ), 0);
}

/**
 * Save setting widget value back to configuration settings
 *
 * @v widget		Setting widget
 */
static int save_setting ( struct setting_widget *widget ) {
	return storef_setting ( widget->settings, widget->setting,
				widget->value );
}

/**
 * Initialise setting widget
 *
 * @v widget		Setting widget
 * @v settings		Settings block
 * @v setting		Configuration setting
 * @v row		Screen row
 * @v col		Screen column
 */
static void init_setting ( struct setting_widget *widget,
			   struct settings *settings,
			   struct setting *setting,
			   unsigned int row, unsigned int col ) {

	/* Initialise widget structure */
	memset ( widget, 0, sizeof ( *widget ) );
	widget->settings = settings;
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
 * @v settings		Settings block
 * @v index		Index of setting with settings list
 */
static void init_setting_index ( struct setting_widget *widget,
				 struct settings *settings,
				 unsigned int index ) {
	struct setting *all_settings = table_start ( SETTINGS );

	init_setting ( widget, settings, &all_settings[index],
		       ( SETTINGS_LIST_ROW + index ), SETTINGS_LIST_COL );
}

/**
 * Print message centred on specified row
 *
 * @v row		Row
 * @v fmt		printf() format string
 * @v args		printf() argument list
 */
static void vmsg ( unsigned int row, const char *fmt, va_list args ) {
	char buf[COLS];
	size_t len;

	len = vsnprintf ( buf, sizeof ( buf ), fmt, args );
	mvprintw ( row, ( ( COLS - len ) / 2 ), "%s", buf );
}

/**
 * Print message centred on specified row
 *
 * @v row		Row
 * @v fmt		printf() format string
 * @v ..		printf() arguments
 */
static void msg ( unsigned int row, const char *fmt, ... ) {
	va_list args;

	va_start ( args, fmt );
	vmsg ( row, fmt, args );
	va_end ( args );
}

/**
 * Clear message on specified row
 *
 * @v row		Row
 */
static void clearmsg ( unsigned int row ) {
	move ( row, 0 );
	clrtoeol();
}

/**
 * Print alert message
 *
 * @v fmt		printf() format string
 * @v args		printf() argument list
 */
static void valert ( const char *fmt, va_list args ) {
	clearmsg ( ALERT_ROW );
	color_set ( CPAIR_ALERT, NULL );
	vmsg ( ALERT_ROW, fmt, args );
	sleep ( 2 );
	color_set ( CPAIR_NORMAL, NULL );
	clearmsg ( ALERT_ROW );
}

/**
 * Print alert message
 *
 * @v fmt		printf() format string
 * @v ...		printf() arguments
 */
static void alert ( const char *fmt, ... ) {
	va_list args;

	va_start ( args, fmt );
	valert ( fmt, args );
	va_end ( args );
}

/**
 * Draw title row
 */
static void draw_title_row ( void ) {
	attron ( A_BOLD );
	msg ( TITLE_ROW, "gPXE option configuration console" );
	attroff ( A_BOLD );
}

/**
 * Draw information row
 *
 * @v setting		Current configuration setting
 */
static void draw_info_row ( struct setting *setting ) {
	clearmsg ( INFO_ROW );
	attron ( A_BOLD );
	msg ( INFO_ROW, "%s - %s", setting->name, setting->description );
	attroff ( A_BOLD );
}

/**
 * Draw instruction row
 *
 * @v editing		Editing in progress flag
 */
static void draw_instruction_row ( int editing ) {
	clearmsg ( INSTRUCTION_ROW );
	if ( editing ) {
		msg ( INSTRUCTION_ROW,
		      "Enter - accept changes" INSTRUCTION_PAD
		      "Ctrl-C - discard changes" );
	} else {
		msg ( INSTRUCTION_ROW,
		      "Ctrl-X - exit configuration utility" );
	}
}

static int main_loop ( struct settings *settings ) {
	struct setting_widget widget;
	unsigned int current = 0;
	unsigned int next;
	int i;
	int key;
	int rc;

	/* Print initial screen content */
	draw_title_row();
	color_set ( CPAIR_NORMAL, NULL );
	for ( i = ( NUM_SETTINGS - 1 ) ; i >= 0 ; i-- ) {
		init_setting_index ( &widget, settings, i );
		draw_setting ( &widget );
	}

	while ( 1 ) {
		/* Redraw information and instruction rows */
		draw_info_row ( widget.setting );
		draw_instruction_row ( widget.editing );

		/* Redraw current setting */
		color_set ( ( widget.editing ? CPAIR_EDIT : CPAIR_SELECT ),
			    NULL );
		draw_setting ( &widget );
		color_set ( CPAIR_NORMAL, NULL );

		key = getkey();
		if ( widget.editing ) {
			key = edit_setting ( &widget, key );
			switch ( key ) {
			case CR:
			case LF:
				if ( ( rc = save_setting ( &widget ) ) != 0 ) {
					alert ( " Could not set %s: %s ",
						widget.setting->name,
						strerror ( rc ) );
				}
				/* Fall through */
			case CTRL_C:
				load_setting ( &widget );
				break;
			default:
				/* Do nothing */
				break;
			}
		} else {
			next = current;
			switch ( key ) {
			case KEY_DOWN:
				if ( next < ( NUM_SETTINGS - 1 ) )
					next++;
				break;
			case KEY_UP:
				if ( next > 0 )
					next--;
				break;
			case CTRL_X:
				return 0;
			default:
				edit_setting ( &widget, key );
				break;
			}	
			if ( next != current ) {
				draw_setting ( &widget );
				init_setting_index ( &widget, settings, next );
				current = next;
			}
		}
	}
	
}

int settings_ui ( struct settings *settings ) {
	int rc;

	initscr();
	start_color();
	init_pair ( CPAIR_NORMAL, COLOR_WHITE, COLOR_BLUE );
	init_pair ( CPAIR_SELECT, COLOR_WHITE, COLOR_RED );
	init_pair ( CPAIR_EDIT, COLOR_BLACK, COLOR_CYAN );
	init_pair ( CPAIR_ALERT, COLOR_WHITE, COLOR_RED );
	color_set ( CPAIR_NORMAL, NULL );
	erase();
	
	rc = main_loop ( settings );

	endwin();

	return rc;
}
