/*
 * Copyright (C) 2011 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * Syslog protocol
 *
 */

#include <stdint.h>
#include <byteswap.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/tcpip.h>
#include <ipxe/dhcp.h>
#include <ipxe/settings.h>
#include <ipxe/console.h>
#include <ipxe/lineconsole.h>
#include <ipxe/syslog.h>
#include <config/console.h>

/* Set default console usage if applicable */
#if ! ( defined ( CONSOLE_SYSLOG ) && CONSOLE_EXPLICIT ( CONSOLE_SYSLOG ) )
#undef CONSOLE_SYSLOG
#define CONSOLE_SYSLOG ( CONSOLE_USAGE_ALL & ~CONSOLE_USAGE_TUI )
#endif

/** The syslog server */
static struct sockaddr_tcpip logserver = {
	.st_family = AF_INET,
	.st_port = htons ( SYSLOG_PORT ),
};

/** Syslog UDP interface operations */
static struct interface_operation syslogger_operations[] = {};

/** Syslog UDP interface descriptor */
static struct interface_descriptor syslogger_desc =
	INTF_DESC_PURE ( syslogger_operations );

/** The syslog UDP interface */
static struct interface syslogger = INTF_INIT ( syslogger_desc );

/******************************************************************************
 *
 * Console driver
 *
 ******************************************************************************
 */

/** Syslog line buffer */
static char syslog_buffer[SYSLOG_BUFSIZE];

/** Syslog severity */
static unsigned int syslog_severity = SYSLOG_DEFAULT_SEVERITY;

/**
 * Handle ANSI set syslog priority (private sequence)
 *
 * @v count		Parameter count
 * @v params		List of graphic rendition aspects
 */
static void syslog_handle_priority ( unsigned int count __unused,
				     int params[] ) {
	if ( params[0] >= 0 ) {
		syslog_severity = params[0];
	} else {
		syslog_severity = SYSLOG_DEFAULT_SEVERITY;
	}
}

/** Syslog ANSI escape sequence handlers */
static struct ansiesc_handler syslog_handlers[] = {
	{ ANSIESC_LOG_PRIORITY, syslog_handle_priority },
	{ 0, NULL }
};

/** Syslog line console */
static struct line_console syslog_line = {
	.buffer = syslog_buffer,
	.len = sizeof ( syslog_buffer ),
	.ctx = {
		.handlers = syslog_handlers,
	},
};

/** Syslog recursion marker */
static int syslog_entered;

/**
 * Print a character to syslog console
 *
 * @v character		Character to be printed
 */
static void syslog_putchar ( int character ) {
	int rc;

	/* Ignore if we are already mid-logging */
	if ( syslog_entered )
		return;

	/* Fill line buffer */
	if ( line_putchar ( &syslog_line, character ) == 0 )
		return;

	/* Guard against re-entry */
	syslog_entered = 1;

	/* Send log message */
	if ( ( rc = xfer_printf ( &syslogger, "<%d>ipxe: %s",
				  SYSLOG_PRIORITY ( SYSLOG_DEFAULT_FACILITY,
						    syslog_severity ),
				  syslog_buffer ) ) != 0 ) {
		DBG ( "SYSLOG could not send log message: %s\n",
		      strerror ( rc ) );
	}

	/* Clear re-entry flag */
	syslog_entered = 0;
}

/** Syslog console driver */
struct console_driver syslog_console __console_driver = {
	.putchar = syslog_putchar,
	.disabled = 1,
	.usage = CONSOLE_SYSLOG,
};

/******************************************************************************
 *
 * Settings
 *
 ******************************************************************************
 */

/** Syslog server setting */
struct setting syslog_setting __setting ( SETTING_MISC ) = {
	.name = "syslog",
	.description = "Syslog server",
	.tag = DHCP_LOG_SERVERS,
	.type = &setting_type_ipv4,
};

/**
 * Apply syslog settings
 *
 * @ret rc		Return status code
 */
static int apply_syslog_settings ( void ) {
	struct sockaddr_in *sin_logserver =
		( struct sockaddr_in * ) &logserver;
	struct in_addr old_addr;
	int len;
	int rc;

	/* Fetch log server */
	syslog_console.disabled = 1;
	old_addr.s_addr = sin_logserver->sin_addr.s_addr;
	if ( ( len = fetch_ipv4_setting ( NULL, &syslog_setting,
					  &sin_logserver->sin_addr ) ) >= 0 ) {
		syslog_console.disabled = 0;
	}

	/* Do nothing unless log server has changed */
	if ( sin_logserver->sin_addr.s_addr == old_addr.s_addr )
		return 0;

	/* Reset syslog connection */
	intf_restart ( &syslogger, 0 );

	/* Do nothing unless we have a log server */
	if ( syslog_console.disabled ) {
		DBG ( "SYSLOG has no log server\n" );
		return 0;
	}

	/* Connect to log server */
	if ( ( rc = xfer_open_socket ( &syslogger, SOCK_DGRAM,
				       ( ( struct sockaddr * ) &logserver ),
				       NULL ) ) != 0 ) {
		DBG ( "SYSLOG cannot connect to log server: %s\n",
		      strerror ( rc ) );
		return rc;
	}
	DBG ( "SYSLOG using log server %s\n",
	      inet_ntoa ( sin_logserver->sin_addr ) );

	return 0;
}

/** Syslog settings applicator */
struct settings_applicator syslog_applicator __settings_applicator = {
	.apply = apply_syslog_settings,
};
