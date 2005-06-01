#ifndef BACKGROUND_H
#define BACKGROUND_H

/** @file
 *
 * Background protocols
 *
 * Some protocols (e.g. ARP, IGMP) operate in the background; the
 * upper layers are not aware of their operation.  When an ARP query
 * for the local station's IP address arrives, Etherboot must reply to
 * it regardless of what other operations are currently in progress.
 *
 * Background protocols are called in two circumstances: when
 * Etherboot is about to poll for a packet, and when Etherboot has
 * received a packet that the upper layer (whatever that may currently
 * be) isn't interested in.
 *
 */

#include "tables.h"
#include "ip.h"

/** A background protocol */
struct background {
	/** Send method
	 *
	 * This method will be called whenever Etherboot is about to
	 * poll for a packet.  The background protocol should use this
	 * method to send out any periodic transmissions that it may
	 * require.
	 */
	void ( *send ) ( unsigned long timestamp );
	/** Process method
	 *
	 * This method will be called whenever Etherboot has received
	 * a packet and doesn't know what to do with it.
	 */
	void ( *process ) ( unsigned long timestamp, unsigned short ptype,
			    struct iphdr *ip );
};

/** A member of the background protocols table */
#define __background __table ( background, 01 )

/* Functions in background.c */

extern void background_send ( unsigned long timestamp );

extern void background_process ( unsigned long timestamp, unsigned short ptype,
				 struct iphdr *ip );

#endif /* BACKGROUND_H */
