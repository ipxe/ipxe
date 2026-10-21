/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
FILE_SECBOOT ( PERMITTED );

#include <string.h>
#include <stdio.h>
#include <ipxe/peerdist.h>
#include <ipxe/peerdisc.h>
#include <usr/peermgmt.h>

/** @file
 *
 * Peer Content Caching and Retrieval (PeerDist) protocol management
 *
 */

/**
 * Print PeerDist error breakdown
 *
 * @v stats		Error statistics
 */
static void peerstat_errors ( struct error_statistics *stats ) {
	unsigned int i;

	/* Print breakdown */
	for ( i = 0 ; i < ( sizeof ( stats->errors ) /
			    sizeof ( stats->errors[0] ) ) ; i++ ) {
		if ( stats->errors[i].count ) {
			printf ( "     [%d x \"%s\"]\n",
				 stats->errors[i].count,
				 strerror ( stats->errors[i].rc ) );
		}
	}
}

/**
 * Print PeerDist discovery socket statistics
 *
 * @v socket		PeerDist discovery socket
 */
static void peerstat_socket ( struct peerdisc_socket *socket ) {
	struct error_statistics *stats = &socket->stats;

	/* Print breakdown */
	printf ( "%s discovery replies [OK:%d ERR:%d]\n",
		 socket->name, stats->good, stats->bad );
	peerstat_errors ( stats );
}

/**
 * Print PeerDist retrieval method statistics
 *
 * @v stats		PeerDist retrieval method statistics
 * @v name		Retrieval type name
 */
static void peerstat_method ( struct error_statistics *stats,
			      const char *name ) {
	char buf[ 4 /* "xxx" + NUL */ ];
	unsigned int percentage;

	/* Construct percentage */
	percentage = ( peerstats.blocks ?
		       ( ( stats->good * 100 ) / peerstats.blocks ) : 0 );
	snprintf ( buf, sizeof ( buf ), "%3d", percentage );
	if ( ( stats->good > 0 ) && ( percentage == 0 ) )
		snprintf ( buf, sizeof ( buf ), " <1" );

	/* Print breakdown */
	printf ( "%s%% %s retrievals [OK:%d ERR:%d]\n",
		 buf, name, stats->good, stats->bad );
	peerstat_errors ( stats );
}

/**
 * Print PeerDist statistics
 *
 */
void peerstat ( void ) {
	struct peerdisc_socket *socket;

	/* Overall statistics */
	printf ( "Downloads:%d Blocks:%d Discoveries:%d(+%d) Peers:%d\n",
		 peerstats.downloads, peerstats.blocks, peerstats.discoveries,
		 peerstats.rediscoveries, peerstats.peers );
	peerstat_errors ( &peerstats.general );

	/* Statistics for each retrieval method */
	peerstat_method ( &peerstats.path, "local cache" );
	peerstat_method ( &peerstats.host, "hosted cache" );
	peerstat_method ( &peerstats.disc, "discovered peer" );
	peerstat_method ( &peerstats.prev, "prediscovered peer" );
	peerstat_method ( &peerstats.origin, "origin server" );

	/* Statistics for each discovery socket address family */
	for_each_table_entry ( socket, PEERDISC_SOCKETS )
		peerstat_socket ( socket );
}
