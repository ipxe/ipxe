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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <gpxe/tables.h>
#include <gpxe/command.h>

/** @file
 *
 * Command execution
 *
 */

static struct command commands[0]
	__table_start ( struct command, commands );
static struct command commands_end[0]
	__table_end ( struct command, commands );

/* Avoid dragging in getopt.o unless a command really uses it */
int optind;
int nextchar;

/**
 * Execute command
 *
 * @v command		Command name
 * @v argv		Argument list
 * @ret rc		Command exit status
 *
 * Execute the named command.  Unlike a traditional POSIX execv(),
 * this function returns the exit status of the command.
 */
int execv ( const char *command, char * const argv[] ) {
	struct command *cmd;
	int argc;

	/* Count number of arguments */
	for ( argc = 0 ; argv[argc] ; argc++ ) {}

	/* Sanity checks */
	if ( ! command ) {
		DBG ( "No command\n" );
		return -EINVAL;
	}
	if ( ! argc ) {
		DBG ( "%s: empty argument list\n", command );
		return -EINVAL;
	}

	/* Reset getopt() library ready for use by the command.  This
	 * is an artefact of the POSIX getopt() API within the context
	 * of Etherboot; see the documentation for reset_getopt() for
	 * details.
	 */
	reset_getopt();

	/* Hand off to command implementation */
	for ( cmd = commands ; cmd < commands_end ; cmd++ ) {
		if ( strcmp ( command, cmd->name ) == 0 )
			return cmd->exec ( argc, ( char ** ) argv );
	}

	printf ( "%s: command not found\n", command );
	return -ENOEXEC;
}

/**
 * Split command line into argv array
 *
 * @v args		Command line
 * @v argv		Argument array to populate, or NULL
 * @ret argc		Argument count
 *
 * Splits the command line into whitespace-delimited arguments.  If @c
 * argv is non-NULL, any whitespace in the command line will be
 * replaced with NULs.
 */
static int split_args ( char *args, char * argv[] ) {
	int argc = 0;

	while ( 1 ) {
		/* Skip over any whitespace / convert to NUL */
		while ( *args == ' ' ) {
			if ( argv )
				*args = '\0';
			args++;
		}
		/* Check for end of line */
		if ( ! *args )
			break;
		/* We have found the start of the next argument */
		if ( argv )
			argv[argc] = args;
		argc++;
		/* Skip to start of next whitespace, if any */
		while ( *args && ( *args != ' ' ) ) {
			args++;
		}
	}
	return argc;
}

/**
 * Execute command line
 *
 * @v command		Command line
 * @ret rc		Command exit status
 *
 * Execute the named command and arguments.
 */
int system ( const char *command ) {
	char *args;
	int argc;
	int rc = 0;

	/* Obtain temporary modifiable copy of command line */
	args = strdup ( command );
	if ( ! args )
		return -ENOMEM;

	/* Count arguments */
	argc = split_args ( args, NULL );

	/* Create argv array and execute command */
	if ( argc ) {
		char * argv[argc + 1];
		
		split_args ( args, argv );
		argv[argc] = NULL;

		if ( argv[0][0] != '#' )
			rc = execv ( argv[0], argv );
	}

	free ( args );
	return rc;
}
