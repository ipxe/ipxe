#include "background.h"

static struct background backgrounds[0]
	__table_start ( struct background, background );
static struct background backgrounds_end[0]
	__table_end ( struct background, background );

/** @file */

/**
 * Call send method of all background protocols
 *
 * @v timestamp		Current time
 * @ret None		-
 * @err None		-
 *
 * This calls each background protocol's background::send() method.
 */
void background_send ( unsigned long timestamp ) {
	struct background *background;

	for ( background = backgrounds ; background < backgrounds_end ;
	      background++ ) {
		if ( background->send )
			background->send ( timestamp );
	}
}

/**
 * Call process method of all background protocols
 *
 * @v timestamp		Current time
 * @v ptype		Packet type
 * @v ip		IP header, if present
 * @ret None		-
 * @err None		-
 *
 * This calls each background protocol's background::process() method.
 */
void background_process ( unsigned long timestamp, unsigned short ptype,
			  struct iphdr *ip ) {
	struct background *background;

	for ( background = backgrounds ; background < backgrounds_end ;
	      background++ ) {
		if ( background->process )
			background->process ( timestamp, ptype, ip );
	}
}
