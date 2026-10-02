/*
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
 * Dynamic user interface tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <ipxe/dynui.h>
#include <ipxe/test.h>

/** Perform dynamic user interface self-tests */
static void dynui_test_exec ( void ) {
	struct dynamic_ui marked = { 0 };
	struct dynamic_item named = {
		.name = "42",
		.text = "Linux Workstations",
	};
	struct dynamic_item separator = {
		.name = NULL,
		.text = "Available host groups",
	};
	struct dynamic_item sentinel = {
		.name = NULL,
		.text = DYNUI_FILTER_SENTINEL,
	};

	/* The exact gap sentinel enables filtering and is hidden by new clients */
	INIT_LIST_HEAD ( &marked.items );
	INIT_LIST_HEAD ( &sentinel.list );
	list_add_tail ( &sentinel.list, &marked.items );
	ok ( dynui_item_is_filter_sentinel ( &sentinel ) );
	ok ( ! dynui_item_is_filter_sentinel ( &separator ) );
	ok ( dynui_has_filter_sentinel ( &marked ) );
	ok ( ! dynui_item_matches ( &sentinel, "" ) );
	ok ( ! dynui_item_matches ( &sentinel, "filter" ) );

	/* Empty search retains the original menu structure */
	ok ( dynui_item_matches ( &named, "" ) );
	ok ( dynui_item_matches ( &separator, "" ) );
	ok ( dynui_item_matches ( &named, "  \t " ) );
	ok ( dynui_item_matches ( &separator, "  \t " ) );

	/* Search is a case-insensitive substring of value or label */
	ok ( dynui_item_matches ( &named, "42" ) );
	ok ( dynui_item_matches ( &named, "work" ) );
	ok ( dynui_item_matches ( &named, "LINUX" ) );
	ok ( ! dynui_item_matches ( &named, "server" ) );

	/* Whitespace-separated terms are combined using AND */
	ok ( dynui_item_matches ( &named, "linux work" ) );
	ok ( dynui_item_matches ( &named, "42 work" ) );
	ok ( dynui_item_matches ( &named, "  WORK   42  " ) );
	ok ( ! dynui_item_matches ( &named, "linux server" ) );
	ok ( ! dynui_item_matches ( &named, "42 server" ) );

	/* Separators are not searchable values */
	ok ( ! dynui_item_matches ( &separator, "host" ) );
}

/** Dynamic user interface self-test */
struct self_test dynui_test __self_test = {
	.name = "dynui",
	.exec = dynui_test_exec,
};
