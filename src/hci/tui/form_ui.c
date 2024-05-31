/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * Text widget forms
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ipxe/ansicol.h>
#include <ipxe/dynui.h>
#include <ipxe/jumpscroll.h>
#include <ipxe/settings.h>
#include <ipxe/editbox.h>
#include <ipxe/message.h>

/** Form title row */
#define TITLE_ROW 1U

/** Starting control row */
#define START_ROW 3U

/** Ending control row */
#define END_ROW ( LINES - 3U )

/** Instructions row */
#define INSTRUCTION_ROW ( LINES - 2U )

/** Padding between instructions */
#define INSTRUCTION_PAD "     "

/** Input field width */
#define INPUT_WIDTH ( COLS / 2U )

/** Input field column */
#define INPUT_COL ( ( COLS - INPUT_WIDTH ) / 2U )

/** A form */
struct form {
	/** Dynamic user interface */
	struct dynamic_ui *dynui;
	/** Jump scroller */
	struct jump_scroller scroll;
	/** Array of form controls */
	struct form_control *controls;
};

/** A form control */
struct form_control {
	/** Dynamic user interface item */
	struct dynamic_item *item;
	/** Settings block */
	struct settings *settings;
	/** Setting */
	struct setting setting;
	/** Label row */
	unsigned int row;
	/** Editable text box */
	struct edit_box editbox;
	/** Modifiable setting name */
	char *name;
	/** Modifiable setting value */
	char *value;
	/** Most recent error in saving */
	int rc;
};

/**
 * Allocate form
 *
 * @v dynui		Dynamic user interface
 * @ret form		Form, or NULL on error
 */
static struct form * alloc_form ( struct dynamic_ui *dynui ) {
	struct form *form;
	struct form_control *control;
	struct dynamic_item *item;
	char *name;
	size_t len;

	/* Calculate total length */
	len = sizeof ( *form );
	list_for_each_entry ( item, &dynui->items, list ) {
		len += sizeof ( *control );
		if ( item->name )
			len += ( strlen ( item->name ) + 1 /* NUL */ );
	}

	/* Allocate and initialise structure */
	form = zalloc ( len );
	if ( ! form )
		return NULL;
	control = ( ( ( void * ) form ) + sizeof ( *form ) );
	name = ( ( ( void * ) control ) +
		 ( dynui->count * sizeof ( *control ) ) );
	form->dynui = dynui;
	form->controls = control;
	list_for_each_entry ( item, &dynui->items, list ) {
		control->item = item;
		if ( item->name ) {
			control->name = name;
			name = ( stpcpy ( name, item->name ) + 1 /* NUL */ );
		}
		control++;
	}
	assert ( ( ( void * ) name ) == ( ( ( void * ) form ) + len ) );

	return form;
}

/**
 * Free form
 *
 * @v form		Form
 */
static void free_form ( struct form *form ) {
	unsigned int i;

	/* Free input value buffers */
	for ( i = 0 ; i < form->dynui->count ; i++ )
		free ( form->controls[i].value );

	/* Free form */
	free ( form );
}

/**
 * Assign form rows
 *
 * @v form		Form
 * @ret rc		Return status code
 */
static int layout_form ( struct form *form ) {
	struct form_control *control;
	struct dynamic_item *item;
	unsigned int labels = 0;
	unsigned int inputs = 0;
	unsigned int pad_control = 0;
	unsigned int pad_label = 0;
	unsigned int minimum;
	unsigned int remaining;
	unsigned int between;
	unsigned int row;
	unsigned int flags;
	unsigned int i;

	/* Count labels and inputs */
	for ( i = 0 ; i < form->dynui->count ; i++ ) {
		control = &form->controls[i];
		item = control->item;
		if ( item->text[0] )
			labels++;
		if ( item->name ) {
			if ( ! inputs )
				form->scroll.current = i;
			inputs++;
			if ( item->flags & DYNUI_DEFAULT )
				form->scroll.current = i;
			form->scroll.count = ( i + 1 );
		}
	}
	form->scroll.rows = form->scroll.count;
	DBGC ( form, "FORM %p has %d controls (%d labels, %d inputs)\n",
	       form, form->dynui->count, labels, inputs );

	/* Refuse to create forms with no inputs */
	if ( ! inputs )
		return -EINVAL;

	/* Calculate minimum number of rows */
	minimum = ( labels + ( inputs * 2 /* edit box and error message */ ) );
	remaining = ( END_ROW - START_ROW );
	DBGC ( form, "FORM %p has %d (of %d) usable rows\n",
	       form, remaining, LINES );
	if ( minimum > remaining )
		return -ERANGE;
	remaining -= minimum;

	/* Insert blank row between controls, if space exists */
	between = ( form->dynui->count - 1 );
	if ( between <= remaining ) {
		pad_control = 1;
		remaining -= between;
		DBGC ( form, "FORM %p padding between controls\n", form );
	}

	/* Insert blank row after label, if space exists */
	if ( labels <= remaining ) {
		pad_label = 1;
		remaining -= labels;
		DBGC ( form, "FORM %p padding after labels\n", form );
	}

	/* Centre on screen */
	DBGC ( form, "FORM %p has %d spare rows\n", form, remaining );
	row = ( START_ROW + ( remaining / 2 ) );

	/* Position each control */
	for ( i = 0 ; i < form->dynui->count ; i++ ) {
		control = &form->controls[i];
		item = control->item;
		if ( item->text[0] ) {
			control->row = row;
			row++; /* Label text */
			row += pad_label;
		}
		if ( item->name ) {
			flags = ( ( item->flags & DYNUI_SECRET ) ?
				  WIDGET_SECRET : 0 );
			init_editbox ( &control->editbox, row, INPUT_COL,
				       INPUT_WIDTH, flags, &control->value );
			row++; /* Edit box */
			row++; /* Error message (if any) */
		}
		row += pad_control;
	}
	assert ( row <= END_ROW );

	return 0;
}

/**
 * Draw form
 *
 * @v form		Form
 */
static void draw_form ( struct form *form ) {
	struct form_control *control;
	unsigned int i;

	/* Clear screen */
	color_set ( CPAIR_NORMAL, NULL );
	erase();

	/* Draw title, if any */
	attron ( A_BOLD );
	if ( form->dynui->title )
		msg ( TITLE_ROW, "%s", form->dynui->title );
	attroff ( A_BOLD );

	/* Draw controls */
	for ( i = 0 ; i < form->dynui->count ; i++ ) {
		control = &form->controls[i];

		/* Draw label, if any */
		if ( control->row )
			msg ( control->row, "%s", control->item->text );

		/* Draw input, if any */
		if ( control->name )
			draw_widget ( &control->editbox.widget );
	}

	/* Draw instructions */
	msg ( INSTRUCTION_ROW, "%s", "Ctrl-X - save changes"
	      INSTRUCTION_PAD "Ctrl-C - discard changes" );
}

/**
 * Draw (or clear) error messages
 *
 * @v form		Form
 */
static void draw_errors ( struct form *form ) {
	struct form_control *control;
	unsigned int row;
	unsigned int i;

	/* Draw (or clear) errors */
	for ( i = 0 ; i < form->dynui->count ; i++ ) {
		control = &form->controls[i];

		/* Skip non-input controls */
		if ( ! control->name )
			continue;

		/* Draw or clear error message as appropriate */
		row = ( control->editbox.widget.row + 1 );
		if ( control->rc != 0 ) {
			color_set ( CPAIR_ALERT, NULL );
			msg ( row, " %s ", strerror ( control->rc ) );
			color_set ( CPAIR_NORMAL, NULL );
		} else {
			clearmsg ( row );
		}
	}
}

/**
 * Parse setting names
 *
 * @v form		Form
 * @ret rc		Return status code
 */
static int parse_names ( struct form *form ) {
	struct form_control *control;
	unsigned int i;
	int rc;

	/* Parse all setting names */
	for ( i = 0 ; i < form->dynui->count ; i++ ) {
		control = &form->controls[i];

		/* Skip labels */
		if ( ! control->name ) {
			DBGC ( form, "FORM %p item %d is a label\n", form, i );
			continue;
		}

		/* Parse setting name */
		DBGC ( form, "FORM %p item %d is for %s\n",
		       form, i, control->name );
		if ( ( rc = parse_setting_name ( control->name,
						 autovivify_child_settings,
						 &control->settings,
						 &control->setting ) ) != 0 )
			return rc;

		/* Apply default type if necessary */
		if ( ! control->setting.type )
			control->setting.type = &setting_type_string;
	}

	return 0;
}

/**
 * Load current input values
 *
 * @v form		Form
 */
static void load_values ( struct form *form ) {
	struct form_control *control;
	unsigned int i;

	/* Fetch all current setting values */
	for ( i = 0 ; i < form->dynui->count ; i++ ) {
		control = &form->controls[i];
		if ( ! control->name )
			continue;
		fetchf_setting_copy ( control->settings, &control->setting,
				      NULL, &control->setting,
				      &control->value );
	}
}

/**
 * Store current input values
 *
 * @v form		Form
 * @ret rc		Return status code
 */
static int save_values ( struct form *form ) {
	struct form_control *control;
	unsigned int i;
	int rc = 0;

	/* Store all current setting values */
	for ( i = 0 ; i < form->dynui->count ; i++ ) {
		control = &form->controls[i];
		if ( ! control->name )
			continue;
		control->rc = storef_setting ( control->settings,
					       &control->setting,
					       control->value );
		if ( control->rc != 0 )
			rc = control->rc;
	}

	return rc;
}

/**
 * Submit form
 *
 * @v form		Form
 * @ret rc		Return status code
 */
static int submit_form ( struct form *form ) {
	int rc;

	/* Attempt to save values */
	rc = save_values ( form );

	/* Draw (or clear) errors */
	draw_errors ( form );

	return rc;
}

/**
 * Form main loop
 *
 * @v form		Form
 * @ret rc		Return status code
 */
static int form_loop ( struct form *form ) {
	struct jump_scroller *scroll = &form->scroll;
	struct form_control *control;
	struct dynamic_item *item;
	unsigned int move;
	unsigned int i;
	int key;
	int rc;

	/* Main loop */
	while ( 1 ) {

		/* Draw current input */
		control = &form->controls[scroll->current];
		draw_widget ( &control->editbox.widget );

		/* Process keypress */
		key = edit_widget ( &control->editbox.widget, getkey ( 0 ) );

		/* Handle scroll keys */
		move = jump_scroll_key ( &form->scroll, key );

		/* Handle special keys */
		switch ( key ) {
		case CTRL_C:
		case ESC:
			/* Cancel form */
			return -ECANCELED;
		case KEY_ENTER:
			/* Attempt to do the most intuitive thing when
			 * Enter is pressed.  If we are on the last
			 * input, then submit the form.  If we are
			 * editing an input which failed, then
			 * resubmit the form.  Otherwise, move to the
			 * next input.
			 */
			if ( ( control->rc == 0 ) &&
			     ( scroll->current < ( scroll->count - 1 ) ) ) {
				move = SCROLL_DOWN;
				break;
			}
			/* fall through */
		case CTRL_X:
			/* Submit form */
			if ( ( rc = submit_form ( form ) ) == 0 )
				return 0;
			/* If current input is not the problem, move
			 * to the first input that needs fixing.
			 */
			if ( control->rc == 0 ) {
				for ( i = 0 ; i < form->dynui->count ; i++ ) {
					if ( form->controls[i].rc != 0 ) {
						scroll->current = i;
						break;
					}
				}
			}
			break;
		default:
			/* Move to input with matching shortcut key, if any */
			item = dynui_shortcut ( form->dynui, key );
			if ( item ) {
				scroll->current = item->index;
				if ( ! item->name )
					move = SCROLL_DOWN;
			}
			break;
		}

		/* Move selection, if applicable */
		while ( move ) {
			move = jump_scroll_move ( &form->scroll, move );
			control = &form->controls[scroll->current];
			if ( control->name )
				break;
		}
	}
}

/**
 * Show form
 *
 * @v dynui		Dynamic user interface
 * @ret rc		Return status code
 */
int show_form ( struct dynamic_ui *dynui ) {
	struct form *form;
	int rc;

	/* Allocate and initialise structure */
	form = alloc_form ( dynui );
	if ( ! form ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Parse setting names and load current values */
	if ( ( rc = parse_names ( form ) ) != 0 )
		goto err_parse_names;
	load_values ( form );

	/* Lay out form on screen */
	if ( ( rc = layout_form ( form ) ) != 0 )
		goto err_layout;

	/* Draw initial form */
	initscr();
	start_color();
	draw_form ( form );

	/* Run main loop */
	if ( ( rc = form_loop ( form ) ) != 0 )
		goto err_loop;

 err_loop:
	color_set ( CPAIR_NORMAL, NULL );
	endwin();
 err_layout:
 err_parse_names:
	free_form ( form );
 err_alloc:
	return rc;
}
