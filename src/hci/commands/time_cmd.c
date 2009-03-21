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
 *
 * March-19-2009 @ 02:44: Added sleep command.
 * Shao Miller <shao.miller@yrdsb.edu.on.ca>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gpxe/command.h>
#include <gpxe/nap.h>
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

static int sleep_exec ( int argc, char **argv ) {
	unsigned long start, delay;

	if ( argc == 1 ||
	     !strcmp ( argv[1], "--help" ) ||
	     !strcmp ( argv[1], "-h" ))
	{
		printf ( "Usage:\n"
			 "  %s <seconds>\n"
			 "\n"
			 "Sleep for <seconds> seconds\n",
			 argv[0] );
		return 1;
	}
	start = currticks();
	delay = strtoul ( argv[1], NULL, 0 ) * ticks_per_sec();
	while ( ( currticks() - start ) <= delay )
		cpu_nap();
	return 0;
}

struct command sleep_command __command = {
	.name = "sleep",
	.exec = sleep_exec,
};
