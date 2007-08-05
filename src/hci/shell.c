/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <readline/readline.h>
#include <gpxe/command.h>
#include <gpxe/shell.h>

/** @file
 *
 * Minimal command shell
 *
 */

static struct command commands[0]
	__table_start ( struct command, commands );
static struct command commands_end[0]
	__table_end ( struct command, commands );

/** The shell prompt string */
static const char shell_prompt[] = "gPXE> ";

/** Flag set in order to exit shell */
static int exit_flag = 0;

/** "exit" command body */
static int exit_exec ( int argc, char **argv __unused ) {

	if ( argc == 1 ) {
		exit_flag = 1;
	} else {
		printf ( "Usage: exit\n"
			 "Exits the command shell\n" );
	}

	return 0;
}

/** "exit" command definition */
struct command exit_command __command = {
	.name = "exit",
	.exec = exit_exec,
};

/** "help" command body */
static int help_exec ( int argc __unused, char **argv __unused ) {
	struct command *command;
	unsigned int hpos = 0;

	printf ( "\nAvailable commands:\n\n" );
	for ( command = commands ; command < commands_end ; command++ ) {
		hpos += printf ( "  %s", command->name );
		if ( hpos > ( 16 * 4 ) ) {
			printf ( "\n" );
			hpos = 0;
		} else {
			while ( hpos % 16 ) {
				printf ( " " );
				hpos++;
			}
		}
	}
	printf ( "\n\nType \"<command> --help\" for further information\n\n" );
	return 0;
}

/** "help" command definition */
struct command help_command __command = {
	.name = "help",
	.exec = help_exec,
};

/**
 * Start command shell
 *
 */
void shell ( void ) {
	char *line;

	exit_flag = 0;
	while ( ! exit_flag ) {
		line = readline ( shell_prompt );
		if ( line ) {
			system ( line );
			free ( line );
		}
	}
}
