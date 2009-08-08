/*
 * Copyright (C) 2009 Joshua Oreman <oremanj@rwcr.net>.
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

#include <gpxe/netdevice.h>
#include <gpxe/net80211.h>
#include <gpxe/command.h>
#include <usr/iwmgmt.h>
#include <hci/ifmgmt_cmd.h>

/* "iwstat" command */

static int iwstat_payload ( struct net_device *netdev ) {
	struct net80211_device *dev = net80211_get ( netdev );

	if ( dev )
		iwstat ( dev );

	return 0;
}

static int iwstat_exec ( int argc, char **argv ) {
	return ifcommon_exec ( argc, argv,
			       iwstat_payload, "Display wireless status of" );
}

/* "iwlist" command */

static int iwlist_payload ( struct net_device *netdev ) {
	struct net80211_device *dev = net80211_get ( netdev );

	if ( dev )
		return iwlist ( dev );

	return 0;
}

static int iwlist_exec ( int argc, char **argv ) {
	return ifcommon_exec ( argc, argv, iwlist_payload,
			       "List wireless networks available via" );
}

/** Wireless interface management commands */
struct command iwmgmt_commands[] __command = {
	{
		.name = "iwstat",
		.exec = iwstat_exec,
	},
	{
		.name = "iwlist",
		.exec = iwlist_exec,
	},
};
