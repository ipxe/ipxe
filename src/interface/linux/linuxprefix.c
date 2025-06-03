/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
 */

#include <stdlib.h>
#include <ipxe/linux_api.h>

/**
 * Linux entry point
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
int __asmcall _linux_start ( int argc, char **argv ) {

	/* Store command-line arguments */
	linux_argc = argc;
	linux_argv = argv;

	/* Run iPXE */
	return main();
}
