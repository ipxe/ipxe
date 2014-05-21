/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** @file
 *
 * Version number
 *
 */

#include <ipxe/features.h>
#include <ipxe/version.h>

/** Version number feature */
FEATURE_VERSION ( VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH );

/** Product major version */
const int product_major_version = VERSION_MAJOR;

/** Product minor version */
const int product_minor_version = VERSION_MINOR;

/** Product version string */
const char *product_version = VERSION;
