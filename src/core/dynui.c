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

/** @file
 *
 * Dynamic user interfaces
 *
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ipxe/list.h>
#include <ipxe/dynui.h>

/** List of all dynamic user interfaces */
static LIST_HEAD ( dynamic_uis );

/**
 * Create dynamic user interface
 *
 * @v name		User interface name, or NULL
 * @v title		User interface title, or NULL
 * @ret dynui		Dynamic user interface, or NULL on failure
 */
struct dynamic_ui * create_dynui ( const char *name, const char *title ) {
	struct dynamic_ui *dynui;
	size_t name_len;
	size_t title_len;
	size_t len;
	char *name_copy;
	char *title_copy;

	/* Destroy any existing user interface of this name */
	dynui = find_dynui ( name );
	if ( dynui )
		destroy_dynui ( dynui );

	/* Use empty title if none given */
	if ( ! title )
		title = "";

	/* Allocate user interface */
	name_len = ( name ? ( strlen ( name ) + 1 /* NUL */ ) : 0 );
	title_len = ( strlen ( title ) + 1 /* NUL */ );
	len = ( sizeof ( *dynui ) + name_len + title_len );
	dynui = zalloc ( len );
	if ( ! dynui )
		return NULL;
	name_copy = ( ( void * ) ( dynui + 1 ) );
	title_copy = ( name_copy + name_len );

	/* Initialise user interface */
	if ( name ) {
		strcpy ( name_copy, name );
		dynui->name = name_copy;
	}
	strcpy ( title_copy, title );
	dynui->title = title_copy;
	INIT_LIST_HEAD ( &dynui->items );

	/* Add to list of user interfaces */
	list_add_tail ( &dynui->list, &dynamic_uis );

	DBGC ( dynui, "DYNUI %s created with title \"%s\"\n",
	       dynui->name, dynui->title );

	return dynui;
}

/**
 * Add dynamic user interface item
 *
 * @v dynui		Dynamic user interface
 * @v name		Name, or NULL
 * @v text		Text, or NULL
 * @v flags		Flags
 * @v shortcut		Shortcut key
 * @ret item		User interface item, or NULL on failure
 */
struct dynamic_item * add_dynui_item ( struct dynamic_ui *dynui,
				       const char *name, const char *text,
				       unsigned int flags, int shortcut ) {
	struct dynamic_item *item;
	size_t name_len;
	size_t text_len;
	size_t len;
	char *name_copy;
	char *text_copy;

	/* Use empty text if none given */
	if ( ! text )
		text = "";

	/* Allocate item */
	name_len = ( name ? ( strlen ( name ) + 1 /* NUL */ ) : 0 );
	text_len = ( strlen ( text ) + 1 /* NUL */ );
	len = ( sizeof ( *item ) + name_len + text_len );
	item = zalloc ( len );
	if ( ! item )
		return NULL;
	name_copy = ( ( void * ) ( item + 1 ) );
	text_copy = ( name_copy + name_len );

	/* Initialise item */
	if ( name ) {
		strcpy ( name_copy, name );
		item->name = name_copy;
	}
	strcpy ( text_copy, text );
	item->text = text_copy;
	item->index = dynui->count++;
	item->flags = flags;
	item->shortcut = shortcut;

	/* Add to list of items */
	list_add_tail ( &item->list, &dynui->items );

	return item;
}

/**
 * Destroy dynamic user interface
 *
 * @v dynui		Dynamic user interface
 */
void destroy_dynui ( struct dynamic_ui *dynui ) {
	struct dynamic_item *item;
	struct dynamic_item *tmp;

	/* Remove from list of user interfaces */
	list_del ( &dynui->list );

	/* Free items */
	list_for_each_entry_safe ( item, tmp, &dynui->items, list ) {
		list_del ( &item->list );
		free ( item );
	}

	/* Free user interface */
	free ( dynui );
}

/**
 * Find dynamic user interface
 *
 * @v name		User interface name, or NULL
 * @ret dynui		Dynamic user interface, or NULL if not found
 */
struct dynamic_ui * find_dynui ( const char *name ) {
	struct dynamic_ui *dynui;

	list_for_each_entry ( dynui, &dynamic_uis, list ) {
		if ( ( dynui->name == name ) ||
		     ( strcmp ( dynui->name, name ) == 0 ) ) {
			return dynui;
		}
	}

	return NULL;
}

/**
 * Find dynamic user interface item by index
 *
 * @v dynui		Dynamic user interface
 * @v index		Index
 * @ret item		User interface item, or NULL if not found
 */
struct dynamic_item * dynui_item ( struct dynamic_ui *dynui,
				   unsigned int index ) {
	struct dynamic_item *item;

	list_for_each_entry ( item, &dynui->items, list ) {
		if ( index-- == 0 )
			return item;
	}

	return NULL;
}

/**
 * Find dynamic user interface item by shortcut key
 *
 * @v dynui		Dynamic user interface
 * @v key		Shortcut key
 * @ret item		User interface item, or NULL if not found
 */
struct dynamic_item * dynui_shortcut ( struct dynamic_ui *dynui, int key ) {
	struct dynamic_item *item;

	list_for_each_entry ( item, &dynui->items, list ) {
		if ( key && ( key == item->shortcut ) )
			return item;
	}

	return NULL;
}
