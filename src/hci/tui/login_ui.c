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

#include <ipxe/dynui.h>
#include <ipxe/login_ui.h>

static struct dynamic_item username;
static struct dynamic_item password;

static struct dynamic_ui login = {
	.items = {
		.prev = &password.list,
		.next = &username.list,
	},
	.count = 2,
};

static struct dynamic_item username = {
	.list = {
		.prev = &login.items,
		.next = &password.list,
	},
	.name = "username",
	.text = "Username",
	.index = 0,
};

static struct dynamic_item password = {
	.list = {
		.prev = &username.list,
		.next = &login.items,
	},
	.name = "password",
	.text = "Password",
	.index = 1,
	.flags = DYNUI_SECRET,
};

int login_ui ( void ) {

	return show_form ( &login );
}
