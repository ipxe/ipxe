/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Dynamic user interface commands
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <ipxe/dynui.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <ipxe/settings.h>
#include <ipxe/features.h>

FEATURE ( FEATURE_MISC, "Menu", DHCP_EB_FEATURE_MENU, 1 );

/** "dynui" options */
struct dynui_options {
	/** Name */
	char *name;
	/** Delete */
	int delete;
};

/** "dynui" option list */
static struct option_descriptor dynui_opts[] = {
	OPTION_DESC ( "name", 'n', required_argument,
		      struct dynui_options, name, parse_string ),
	OPTION_DESC ( "delete", 'd', no_argument,
		      struct dynui_options, delete, parse_flag ),
};

/** "dynui" command descriptor */
static struct command_descriptor dynui_cmd =
	COMMAND_DESC ( struct dynui_options, dynui_opts, 0, MAX_ARGUMENTS,
		       "[<title>]" );

/**
 * The "dynui" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int dynui_exec ( int argc, char **argv ) {
	struct dynui_options opts;
	struct dynamic_ui *dynui;
	char *title;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &dynui_cmd, &opts ) ) != 0 )
		goto err_parse_options;

	/* Parse title */
	title = concat_args ( &argv[optind] );
	if ( ! title ) {
		rc = -ENOMEM;
		goto err_parse_title;
	}

	/* Create dynamic user interface */
	dynui = create_dynui ( opts.name, title );
	if ( ! dynui ) {
		rc = -ENOMEM;
		goto err_create_dynui;
	}

	/* Destroy dynamic user interface, if applicable */
	if ( opts.delete )
		destroy_dynui ( dynui );

	/* Success */
	rc = 0;

 err_create_dynui:
	free ( title );
 err_parse_title:
 err_parse_options:
	return rc;
}

/** "item" options */
struct item_options {
	/** Dynamic user interface name */
	char *dynui;
	/** Shortcut key */
	unsigned int key;
	/** Use as default */
	int is_default;
	/** Value is a secret */
	int is_secret;
	/** Use as a separator */
	int is_gap;
};

/** "item" option list */
static struct option_descriptor item_opts[] = {
	OPTION_DESC ( "menu", 'm', required_argument,
		      struct item_options, dynui, parse_string ),
	OPTION_DESC ( "form", 'f', required_argument,
		      struct item_options, dynui, parse_string ),
	OPTION_DESC ( "key", 'k', required_argument,
		      struct item_options, key, parse_key ),
	OPTION_DESC ( "default", 'd', no_argument,
		      struct item_options, is_default, parse_flag ),
	OPTION_DESC ( "secret", 's', no_argument,
		      struct item_options, is_secret, parse_flag ),
	OPTION_DESC ( "gap", 'g', no_argument,
		      struct item_options, is_gap, parse_flag ),
};

/** "item" command descriptor */
static struct command_descriptor item_cmd =
	COMMAND_DESC ( struct item_options, item_opts, 0, MAX_ARGUMENTS,
		       "[<name> [<text>]]" );

/**
 * The "item" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int item_exec ( int argc, char **argv ) {
	struct item_options opts;
	struct dynamic_ui *dynui;
	struct dynamic_item *item;
	unsigned int flags = 0;
	char *name = NULL;
	char *text = NULL;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &item_cmd, &opts ) ) != 0 )
		goto err_parse_options;

	/* Parse name, if present */
	if ( ! opts.is_gap )
		name = argv[optind++]; /* May be NULL */

	/* Parse text, if present */
	if ( optind < argc ) {
		text = concat_args ( &argv[optind] );
		if ( ! text ) {
			rc = -ENOMEM;
			goto err_parse_text;
		}
	}

	/* Identify dynamic user interface */
	if ( ( rc = parse_dynui ( opts.dynui, &dynui ) ) != 0 )
		goto err_parse_dynui;

	/* Add dynamic user interface item */
	if ( opts.is_default )
		flags |= DYNUI_DEFAULT;
	if ( opts.is_secret )
		flags |= DYNUI_SECRET;
	item = add_dynui_item ( dynui, name, ( text ? text : "" ), flags,
				opts.key );
	if ( ! item ) {
		rc = -ENOMEM;
		goto err_add_dynui_item;
	}

	/* Success */
	rc = 0;

 err_add_dynui_item:
 err_parse_dynui:
	free ( text );
 err_parse_text:
 err_parse_options:
	return rc;
}

/** "choose" options */
struct choose_options {
	/** Dynamic user interface name */
	char *dynui;
	/** Timeout */
	unsigned long timeout;
	/** Default selection */
	char *select;
	/** Keep dynamic user interface */
	int keep;
};

/** "choose" option list */
static struct option_descriptor choose_opts[] = {
	OPTION_DESC ( "menu", 'm', required_argument,
		      struct choose_options, dynui, parse_string ),
	OPTION_DESC ( "default", 'd', required_argument,
		      struct choose_options, select, parse_string ),
	OPTION_DESC ( "timeout", 't', required_argument,
		      struct choose_options, timeout, parse_timeout ),
	OPTION_DESC ( "keep", 'k', no_argument,
		      struct choose_options, keep, parse_flag ),
};

/** "choose" command descriptor */
static struct command_descriptor choose_cmd =
	COMMAND_DESC ( struct choose_options, choose_opts, 1, 1, "<setting>" );

/**
 * The "choose" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int choose_exec ( int argc, char **argv ) {
	struct choose_options opts;
	struct named_setting setting;
	struct dynamic_ui *dynui;
	struct dynamic_item *item;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &choose_cmd, &opts ) ) != 0 )
		goto err_parse_options;

	/* Parse setting name */
	if ( ( rc = parse_autovivified_setting ( argv[optind],
						 &setting ) ) != 0 )
		goto err_parse_setting;

	/* Identify dynamic user interface */
	if ( ( rc = parse_dynui ( opts.dynui, &dynui ) ) != 0 )
		goto err_parse_dynui;

	/* Show as menu */
	if ( ( rc = show_menu ( dynui, opts.timeout, opts.select,
				&item ) ) != 0 )
		goto err_show_menu;

	/* Apply default type if necessary */
	if ( ! setting.setting.type )
		setting.setting.type = &setting_type_string;

	/* Store setting */
	if ( ( rc = storef_setting ( setting.settings, &setting.setting,
				     item->name ) ) != 0 ) {
		printf ( "Could not store \"%s\": %s\n",
			 setting.setting.name, strerror ( rc ) );
		goto err_store;
	}

	/* Success */
	rc = 0;

 err_store:
 err_show_menu:
	/* Destroy dynamic user interface, if applicable */
	if ( ! opts.keep )
		destroy_dynui ( dynui );
 err_parse_dynui:
 err_parse_setting:
 err_parse_options:
	return rc;
}

/** "present" options */
struct present_options {
	/** Dynamic user interface name */
	char *dynui;
	/** Keep dynamic user interface */
	int keep;
};

/** "present" option list */
static struct option_descriptor present_opts[] = {
	OPTION_DESC ( "form", 'f', required_argument,
		      struct present_options, dynui, parse_string ),
	OPTION_DESC ( "keep", 'k', no_argument,
		      struct present_options, keep, parse_flag ),
};

/** "present" command descriptor */
static struct command_descriptor present_cmd =
	COMMAND_DESC ( struct present_options, present_opts, 0, 0, NULL );

/**
 * The "present" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int present_exec ( int argc, char **argv ) {
	struct present_options opts;
	struct dynamic_ui *dynui;
	int rc;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &present_cmd, &opts ) ) != 0 )
		goto err_parse_options;

	/* Identify dynamic user interface */
	if ( ( rc = parse_dynui ( opts.dynui, &dynui ) ) != 0 )
		goto err_parse_dynui;

	/* Show as form */
	if ( ( rc = show_form ( dynui ) ) != 0 )
		goto err_show_form;

	/* Success */
	rc = 0;

 err_show_form:
	/* Destroy dynamic user interface, if applicable */
	if ( ! opts.keep )
		destroy_dynui ( dynui );
 err_parse_dynui:
 err_parse_options:
	return rc;
}

/** Dynamic user interface commands */
struct command dynui_commands[] __command = {
	{
		.name = "menu",
		.exec = dynui_exec,
	},
	{
		.name = "form",
		.exec = dynui_exec,
	},
	{
		.name = "item",
		.exec = item_exec,
	},
	{
		.name = "choose",
		.exec = choose_exec,
	},
	{
		.name = "present",
		.exec = present_exec,
	},
};
