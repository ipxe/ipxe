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

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <ipxe/tables.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/settings.h>

/** @file
 *
 * Command execution
 *
 */

/** Shell exit flag */
int shell_exit;

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

	/* An empty command is deemed to do nothing, successfully */
	if ( command == NULL )
		return 0;

	/* Sanity checks */
	if ( argc == 0 ) {
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
	for_each_table_entry ( cmd, COMMANDS ) {
		if ( strcmp ( command, cmd->name ) == 0 )
			return cmd->exec ( argc, ( char ** ) argv );
	}

	printf ( "%s: command not found\n", command );
	return -ENOEXEC;
}

/**
 * Expand variables within command line
 *
 * @v command		Command line
 * @ret expcmd		Expanded command line
 *
 * The expanded command line is allocated with malloc() and the caller
 * must eventually free() it.
 */
static char * expand_command ( const char *command ) {
	char *expcmd;
	char *start;
	char *end;
	char *head;
	char *name;
	char *tail;
	int setting_len;
	int new_len;
	char *tmp;

	/* Obtain temporary modifiable copy of command line */
	expcmd = strdup ( command );
	if ( ! expcmd )
		return NULL;

	/* Expand while expansions remain */
	while ( 1 ) {

		head = expcmd;

		/* Locate opener */
		start = strstr ( expcmd, "${" );
		if ( ! start )
			break;
		*start = '\0';
		name = ( start + 2 );

		/* Locate closer */
		end = strstr ( name, "}" );
		if ( ! end )
			break;
		*end = '\0';
		tail = ( end + 1 );

		/* Determine setting length */
		setting_len = fetchf_named_setting ( name, NULL, 0 );
		if ( setting_len < 0 )
			setting_len = 0; /* Treat error as empty setting */

		/* Read setting into temporary buffer */
		{
			char setting_buf[ setting_len + 1 ];

			setting_buf[0] = '\0';
			fetchf_named_setting ( name, setting_buf,
					       sizeof ( setting_buf ) );

			/* Construct expanded string and discard old string */
			tmp = expcmd;
			new_len = asprintf ( &expcmd, "%s%s%s",
					     head, setting_buf, tail );
			free ( tmp );
			if ( new_len < 0 )
				return NULL;
		}
	}

	return expcmd;
}

/**
 * Split command line into tokens
 *
 * @v command		Command line
 * @v tokens		Token list to populate, or NULL
 * @ret count		Number of tokens
 *
 * Splits the command line into whitespace-delimited tokens.  If @c
 * tokens is non-NULL, any whitespace in the command line will be
 * replaced with NULs.
 */
static int split_command ( char *command, char **tokens ) {
	int count = 0;

	while ( 1 ) {
		/* Skip over any whitespace / convert to NUL */
		while ( isspace ( *command ) ) {
			if ( tokens )
				*command = '\0';
			command++;
		}
		/* Check for end of line */
		if ( ! *command )
			break;
		/* We have found the start of the next argument */
		if ( tokens )
			tokens[count] = command;
		count++;
		/* Skip to start of next whitespace, if any */
		while ( *command && ! isspace ( *command ) ) {
			command++;
		}
	}
	return count;
}

/**
 * Terminate command unconditionally
 *
 * @v rc		Status of previous command
 * @ret terminate	Terminate command
 */
static int terminate_always ( int rc __unused ) {
	return 1;
}

/**
 * Terminate command only if previous command succeeded
 *
 * @v rc		Status of previous command
 * @ret terminate	Terminate command
 */
static int terminate_on_success ( int rc ) {
	return ( rc == 0 );
}

/**
 * Terminate command only if previous command failed
 *
 * @v rc		Status of previous command
 * @ret terminate	Terminate command
 */
static int terminate_on_failure ( int rc ) {
	return ( rc != 0 );
}

/**
 * Find command terminator
 *
 * @v tokens		Token list
 * @ret terminator	Terminator type
 * @ret argc		Argument count
 */
static int command_terminator ( char **tokens,
				int ( **terminator ) ( int rc ) ) {
	unsigned int i;

	/* Find first terminating token */
	for ( i = 0 ; tokens[i] ; i++ ) {
		if ( tokens[i][0] == '#' ) {
			/* Start of a comment */
			*terminator = terminate_always;
			return i;
		} else if ( strcmp ( tokens[i], "||" ) == 0 ) {
			/* Short-circuit logical OR */
			*terminator = terminate_on_success;
			return i;
		} else if ( strcmp ( tokens[i], "&&" ) == 0 ) {
			/* Short-circuit logical AND */
			*terminator = terminate_on_failure;
			return i;
		}
	}

	/* End of token list */
	*terminator = terminate_always;
	return i;
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
	int ( * terminator ) ( int rc );
	char *expcmd;
	char **argv;
	int argc;
	int count;
	int rc = 0;

	/* Reset exit flag */
	shell_exit = 0;

	/* Perform variable expansion */
	expcmd = expand_command ( command );
	if ( ! expcmd )
		return -ENOMEM;

	/* Count tokens */
	count = split_command ( expcmd, NULL );

	/* Create token array */
	if ( count ) {
		char * tokens[count + 1];
		
		split_command ( expcmd, tokens );
		tokens[count] = NULL;

		for ( argv = tokens ; ; argv += ( argc + 1 ) ) {

			/* Find command terminator */
			argc = command_terminator ( argv, &terminator );

			/* Execute command */
			argv[argc] = NULL;
			rc = execv ( argv[0], argv );

			/* Check exit flag */
			if ( shell_exit )
				break;

			/* Handle terminator */
			if ( terminator ( rc ) )
				break;
		}
	}

	/* Free expanded command */
	free ( expcmd );

	return rc;
}

/**
 * "echo" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Exit code
 */
static int echo_exec ( int argc, char **argv ) {
	int i;

	for ( i = 1 ; i < argc ; i++ ) {
		printf ( "%s%s", ( ( i == 1 ) ? "" : " " ), argv[i] );
	}
	printf ( "\n" );
	return 0;
}

/** "echo" command */
struct command echo_command __command = {
	.name = "echo",
	.exec = echo_exec,
};

/** "exit" options */
struct exit_options {};

/** "exit" option list */
static struct option_descriptor exit_opts[] = {};

/** "exit" command descriptor */
static struct command_descriptor exit_cmd =
	COMMAND_DESC ( struct exit_options, exit_opts, 0, 1,
		       "[<status>]", "" );

/**
 * "exit" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int exit_exec ( int argc, char **argv ) {
	struct exit_options opts;
	unsigned int exit_code = 0;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &exit_cmd, &opts ) ) != 0 )
		return rc;

	/* Parse exit status, if present */
	if ( optind != argc ) {
		if ( ( rc = parse_integer ( argv[optind], &exit_code ) ) != 0 )
			return rc;
	}

	/* Set exit flag */
	shell_exit = 1;

	return exit_code;
}

/** "exit" command */
struct command exit_command __command = {
	.name = "exit",
	.exec = exit_exec,
};

