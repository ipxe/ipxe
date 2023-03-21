/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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

/**
 * @file
 *
 * Quiesce system
 *
 */

#include <ipxe/quiesce.h>

/** Quiesce system */
void quiesce ( void ) {
	struct quiescer *quiescer;

	/* Call all quiescers */
	for_each_table_entry ( quiescer, QUIESCERS ) {
		quiescer->quiesce();
	}
}

/** Unquiesce system */
void unquiesce ( void ) {
	struct quiescer *quiescer;

	/* Call all quiescers */
	for_each_table_entry ( quiescer, QUIESCERS ) {
		quiescer->unquiesce();
	}
}
