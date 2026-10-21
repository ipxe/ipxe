#ifndef _IPXE_PEERDIST_H
#define _IPXE_PEERDIST_H

/** @file
 *
 * Peer Content Caching and Retrieval (PeerDist) protocol
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

#include <ipxe/errstat.h>

/** PeerDist statistics */
struct peerdist_statistics {
	/** Number of downloads */
	unsigned int downloads;
	/** Total number of blocks */
	unsigned int blocks;
	/** Number of unique discovery attempts */
	unsigned int discoveries;
	/** Number of repeated discovery attempts */
	unsigned int rediscoveries;
	/** Highest observed number of peers */
	unsigned int peers;

	/** General error statistics */
	struct error_statistics general;

	/** Retrievals from local cache directory */
	struct error_statistics path;
	/** Retrievals from hosted cache server */
	struct error_statistics host;
	/** Retrievals from discovered peer */
	struct error_statistics disc;
	/** Retrievals from previously discovered peer */
	struct error_statistics prev;
	/** Retrievals from origin server */
	struct error_statistics origin;
};

/** PeerDist statistics */
extern struct peerdist_statistics peerstats;

#endif /* _IPXE_PEERDIST_H */
