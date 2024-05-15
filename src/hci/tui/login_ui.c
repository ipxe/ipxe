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
 * Login UI
 *
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <curses.h>
#include <ipxe/console.h>
#include <ipxe/settings.h>
#include <ipxe/label.h>
#include <ipxe/editbox.h>
#include <ipxe/keys.h>
#include <ipxe/ansicol.h>
#include <ipxe/login_ui.h>

/* Screen layout */
#define USERNAME_LABEL_ROW	( ( LINES / 2U ) - 4U )
#define USERNAME_ROW		( ( LINES / 2U ) - 2U )
#define PASSWORD_LABEL_ROW	( ( LINES / 2U ) + 2U )
#define PASSWORD_ROW		( ( LINES / 2U ) + 4U )
#define WIDGET_COL		( ( COLS / 2U ) - 10U )
#define WIDGET_WIDTH		20U

int login_ui ( void ) {
	char *username;
	char *password;
	struct {
		struct widgets widgets;
		struct label username_label;
		struct label password_label;
		struct edit_box username_box;
		struct edit_box password_box;
	} widgets;
	int rc;

	/* Fetch current setting values */
	fetchf_setting_copy ( NULL, &username_setting, NULL, NULL, &username );
	fetchf_setting_copy ( NULL, &password_setting, NULL, NULL, &password );

	/* Construct user interface */
	memset ( &widgets, 0, sizeof ( widgets ) );
	init_widgets ( &widgets.widgets, NULL );
	init_label ( &widgets.username_label, USERNAME_LABEL_ROW, WIDGET_COL,
		     WIDGET_WIDTH, "Username" );
	init_label ( &widgets.password_label, PASSWORD_LABEL_ROW, WIDGET_COL,
		     WIDGET_WIDTH, "Password" );
	init_editbox ( &widgets.username_box, USERNAME_ROW, WIDGET_COL,
		       WIDGET_WIDTH, 0, &username );
	init_editbox ( &widgets.password_box, PASSWORD_ROW, WIDGET_COL,
		       WIDGET_WIDTH, WIDGET_SECRET, &password );
	add_widget ( &widgets.widgets, &widgets.username_label.widget );
	add_widget ( &widgets.widgets, &widgets.password_label.widget );
	add_widget ( &widgets.widgets, &widgets.username_box.widget );
	add_widget ( &widgets.widgets, &widgets.password_box.widget );

	/* Present user interface */
	if ( ( rc = widget_ui ( &widgets.widgets ) ) != 0 )
		goto err_ui;

	/* Store settings on successful completion */
	if ( ( rc = storef_setting ( NULL, &username_setting, username ) ) !=0)
		goto err_store_username;
	if ( ( rc = storef_setting ( NULL, &password_setting, password ) ) !=0)
		goto err_store_password;

 err_store_username:
 err_store_password:
 err_ui:
	free ( username );
	free ( password );
	return rc;
}
