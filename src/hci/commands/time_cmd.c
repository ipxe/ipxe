/*
 * Copyright (C) 2009 Daniel Verkamp <daniel@drv.nu>.
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gpxe/command.h>
#include <gpxe/timer.h>

static int time_exec ( int argc, char **argv ) {
	unsigned long start;
	int rc, secs;

	if ( argc == 1 ||
	     !strcmp ( argv[1], "--help" ) ||
	     !strcmp ( argv[1], "-h" ) )
	{
		printf ( "Usage:\n"
			 "  %s <command>\n"
			 "\n"
			 "Time a command\n",
			 argv[0] );
		return 1;
	}

	start = currticks();
	rc = execv ( argv[1], argv + 1 );
	secs = (currticks() - start) / ticks_per_sec();

	printf ( "%s: %ds\n", argv[0], secs );

	return rc;
}

struct command time_command __command = {
	.name = "time",
	.exec = time_exec,
};

