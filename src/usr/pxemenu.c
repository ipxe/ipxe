/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <byteswap.h>
#include <curses.h>
#include <console.h>
#include <gpxe/dhcp.h>
#include <gpxe/vsprintf.h>
#include <gpxe/keys.h>
#include <gpxe/timer.h>
#include <usr/dhcpmgmt.h>
#include <usr/autoboot.h>

/** @file
 *
 * PXE Boot Menus
 *
 */

/* Colour pairs */
#define CPAIR_NORMAL	1
#define CPAIR_SELECT	2

/** A PXE boot menu item */
struct pxe_menu_item {
	/** Boot Server type */
	unsigned int type;
	/** Description */
	char *desc;
};

/**
 * A PXE boot menu
 *
 * This structure encapsulates the menu information provided via DHCP
 * options.
 */
struct pxe_menu {
	/** Timeout (in seconds)
	 *
	 * Negative indicates no timeout (i.e. wait indefinitely)
	 */
	int timeout;
	/** Number of menu items */
	unsigned int num_items;
	/** Selected menu item */
	unsigned int selection;
	/** Menu items */
	struct pxe_menu_item items[0];
};

/**
 * Parse and allocate PXE boot menu
 *
 * @v menu		PXE boot menu to fill in
 * @ret rc		Return status code
 *
 * It is the callers responsibility to eventually free the allocated
 * boot menu.
 */
static int pxe_menu_parse ( struct pxe_menu **menu ) {
	struct setting tmp_setting = { .name = NULL };
	struct dhcp_pxe_boot_menu_prompt prompt = { .timeout = 0 };
	uint8_t raw_menu[256];
	int raw_menu_len;
	struct dhcp_pxe_boot_menu *raw_menu_item;
	void *raw_menu_end;
	unsigned int num_menu_items;
	unsigned int i;
	int rc;

	/* Fetch relevant settings */
	tmp_setting.tag = DHCP_PXE_BOOT_MENU_PROMPT;
	fetch_setting ( NULL, &tmp_setting, &prompt, sizeof ( prompt ) );
	tmp_setting.tag = DHCP_PXE_BOOT_MENU;
	memset ( raw_menu, 0, sizeof ( raw_menu ) );
	if ( ( raw_menu_len = fetch_setting ( NULL, &tmp_setting, raw_menu,
					      sizeof ( raw_menu ) ) ) < 0 ) {
		rc = raw_menu_len;
		DBG ( "Could not retrieve raw PXE boot menu: %s\n",
		      strerror ( rc ) );
		return rc;
	}
	if ( raw_menu_len >= ( int ) sizeof ( raw_menu ) ) {
		DBG ( "Raw PXE boot menu too large for buffer\n" );
		return -ENOSPC;
	}
	raw_menu_end = ( raw_menu + raw_menu_len );

	/* Count menu items */
	num_menu_items = 0;
	raw_menu_item = ( ( void * ) raw_menu );
	while ( 1 ) {
		if ( ( ( ( void * ) raw_menu_item ) +
		       sizeof ( *raw_menu_item ) ) > raw_menu_end )
			break;
		if ( ( ( ( void * ) raw_menu_item ) +
		       sizeof ( *raw_menu_item ) +
		       raw_menu_item->desc_len ) > raw_menu_end )
			break;
		num_menu_items++;
		raw_menu_item = ( ( ( void * ) raw_menu_item ) +
				  sizeof ( *raw_menu_item ) +
				  raw_menu_item->desc_len );
	}

	/* Allocate space for parsed menu */
	*menu = zalloc ( sizeof ( **menu ) +
			 ( num_menu_items * sizeof ( (*menu)->items[0] ) ) +
			 raw_menu_len + 1 /* NUL */ );
	if ( ! *menu ) {
		DBG ( "Could not allocate PXE boot menu\n" );
		return -ENOMEM;
	}

	/* Fill in parsed menu */
	(*menu)->timeout =
		( ( prompt.timeout == 0xff ) ? -1 : prompt.timeout );
	(*menu)->num_items = num_menu_items;
	raw_menu_item = ( ( ( void * ) (*menu) ) + sizeof ( **menu ) +
			  ( num_menu_items * sizeof ( (*menu)->items[0] ) ) );
	memcpy ( raw_menu_item, raw_menu, raw_menu_len );
	for ( i = 0 ; i < num_menu_items ; i++ ) {
		(*menu)->items[i].type = ntohs ( raw_menu_item->type );
		(*menu)->items[i].desc = raw_menu_item->desc;
		/* Set type to 0; this ensures that the description
		 * for the previous menu item is NUL-terminated.
		 * (Final item is NUL-terminated anyway.)
		 */
		raw_menu_item->type = 0;
		raw_menu_item = ( ( ( void * ) raw_menu_item ) +
				  sizeof ( *raw_menu_item ) +
				  raw_menu_item->desc_len );
	}

	return 0;
}

/**
 * Draw PXE boot menu item
 *
 * @v menu		PXE boot menu
 * @v index		Index of item to draw
 */
static void pxe_menu_draw_item ( struct pxe_menu *menu,
				 unsigned int index ) {
	int selected = ( menu->selection == index );
	char buf[COLS+1];
	char *tmp = buf;
	ssize_t remaining = sizeof ( buf );
	size_t len;
	unsigned int row;

	/* Prepare space-padded row content */
	len = ssnprintf ( tmp, remaining, " %c. %s",
			  ( 'A' + index ), menu->items[index].desc );
	tmp += len;
	remaining -= len;
	if ( selected && ( menu->timeout > 0 ) ) {
		len = ssnprintf ( tmp, remaining, " (%d)", menu->timeout );
		tmp += len;
		remaining -= len;
	}
	for ( ; remaining > 1 ; tmp++, remaining-- )
		*tmp = ' ';
	*tmp = '\0';

	/* Draw row */
	row = ( LINES - menu->num_items + index );
	color_set ( ( selected ? CPAIR_SELECT : CPAIR_NORMAL ), NULL );
	mvprintw ( row, 0, "%s", buf );
	move ( row, 1 );
}

/**
 * Make selection from PXE boot menu
 *
 * @v menu		PXE boot menu
 * @ret rc		Return status code
 */
int pxe_menu_select ( struct pxe_menu *menu ) {
	unsigned long start = currticks();
	unsigned long now;
	unsigned long elapsed;
	unsigned int old_selection;
	int key;
	unsigned int key_selection;
	unsigned int i;
	int rc = 0;

	/* Initialise UI */
	initscr();
	start_color();
	init_pair ( CPAIR_NORMAL, COLOR_WHITE, COLOR_BLACK );
	init_pair ( CPAIR_SELECT, COLOR_BLACK, COLOR_WHITE );
	color_set ( CPAIR_NORMAL, NULL );

	/* Draw initial menu */
	for ( i = 0 ; i < menu->num_items ; i++ )
		printf ( "\n" );
	for ( i = 0 ; i < menu->num_items ; i++ )
		pxe_menu_draw_item ( menu, ( menu->num_items - i - 1 ) );

	while ( 1 ) {

		/* Decrease timeout if necessary */
		if ( menu->timeout > 0 ) {
			now = currticks();
			elapsed = ( now - start );
			if ( elapsed >= TICKS_PER_SEC ) {
				start = now;
				menu->timeout--;
				pxe_menu_draw_item ( menu, menu->selection );
			}
		}

		/* Select current item if we have timed out */
		if ( menu->timeout == 0 )
			break;

		/* Check for keyboard input */
		if ( ! iskey() )
			continue;
		key = getkey();

		/* Any keyboard input cancels the timeout */
		menu->timeout = -1;
		pxe_menu_draw_item ( menu, menu->selection );

		/* Act upon key */
		old_selection = menu->selection;
		if ( ( key == CR ) || ( key == LF ) ) {
			break;
		} else if ( key == CTRL_C ) {
			rc = -ECANCELED;
			break;
		} else if ( key == KEY_UP ) {
			if ( menu->selection > 0 )
				menu->selection--;
		} else if ( key == KEY_DOWN ) {
			if ( menu->selection < ( menu->num_items - 1 ) )
				menu->selection++;
		} else if ( ( key < KEY_MIN ) &&
			    ( ( key_selection = ( toupper ( key ) - 'A' ) )
			      < menu->num_items ) ) {
			menu->selection = key_selection;
			menu->timeout = 0;
		}
		pxe_menu_draw_item ( menu, old_selection );
		pxe_menu_draw_item ( menu, menu->selection );
	}

	/* Shut down UI */
	endwin();

	return rc;
}

/**
 * Boot using PXE boot menu
 *
 * @ret rc		Return status code
 *
 * Note that a success return status indicates that a PXE boot menu
 * item has been selected, and that the DHCP session should perform a
 * boot server request/ack.
 */
int pxe_menu_boot ( struct net_device *netdev ) {
	struct pxe_menu *menu;
	unsigned int pxe_type;
	struct settings *pxebs_settings;
	struct in_addr next_server;
	char filename[256];
	int rc;

	/* Parse and allocate boot menu */
	if ( ( rc = pxe_menu_parse ( &menu ) ) != 0 )
		return rc;

	/* Make selection from boot menu */
	if ( ( rc = pxe_menu_select ( menu ) ) != 0 ) {
		free ( menu );
		return rc;
	}
	pxe_type = menu->items[menu->selection].type;

	/* Free boot menu */
	free ( menu );

	/* Return immediately if local boot selected */
	if ( ! pxe_type )
		return 0;

	/* Attempt PXE Boot Server Discovery */
	if ( ( rc = pxebs ( netdev, pxe_type ) ) != 0 )
		return rc;

	/* Attempt boot */
	pxebs_settings = find_settings ( PXEBS_SETTINGS_NAME );
	assert ( pxebs_settings );
	fetch_ipv4_setting ( pxebs_settings, &next_server_setting,
			     &next_server );
	fetch_string_setting ( pxebs_settings, &filename_setting,
			       filename, sizeof ( filename ) );
	return boot_next_server_and_filename ( next_server, filename );
}
