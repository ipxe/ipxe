/*
 * Copyright (c) 2009 Joshua Oreman <oremanj@rwcr.net>.
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

#ifndef _GPXE_SEC80211_H
#define _GPXE_SEC80211_H

FILE_LICENCE ( GPL2_OR_LATER );

#include <gpxe/net80211.h>
#include <errno.h>

/** @file
 *
 * Definitions for general secured-network routines.
 *
 * Any function in this file which may be referenced by code which is
 * not exclusive to encryption-enabled builds (e.g. sec80211_detect(),
 * which is called by net80211_probe_step() to fill the net80211_wlan
 * structure's security fields) must be declared as a weak symbol,
 * using an inline interface similar to that used for
 * sec80211_detect() below. This prevents secure network support from
 * bloating general builds by any more than a few tiny hooks to call
 * crypto functions when crypto structures are non-NULL.
 */

int _sec80211_detect ( struct io_buffer *iob,
		       enum net80211_security_proto *secprot,
		       enum net80211_crypto_alg *crypt )
	__attribute__ (( weak ));


/**
 * Inline safety wrapper for _sec80211_detect()
 *
 * @v iob	I/O buffer containing beacon frame
 * @ret secprot	Security handshaking protocol used by network
 * @ret crypt	Cryptosystem used by network
 * @ret rc	Return status code
 *
 * This function transparently calls _sec80211_detect() if the file
 * containing it was compiled in, or returns an error indication of
 * @c -ENOTSUP if not.
 */
static inline int sec80211_detect ( struct io_buffer *iob,
				    enum net80211_security_proto *secprot,
				    enum net80211_crypto_alg *crypt ) {
	if ( _sec80211_detect )
		return _sec80211_detect ( iob, secprot, crypt );
	return -ENOTSUP;
}

int sec80211_detect_ie ( int is_rsn, u8 *start, u8 *end,
			 enum net80211_security_proto *secprot,
			 enum net80211_crypto_alg *crypt );
u8 * sec80211_find_rsn ( union ieee80211_ie *ie, void *ie_end,
			 int *is_rsn, u8 **end );

int sec80211_install ( struct net80211_crypto **which,
		       enum net80211_crypto_alg crypt,
		       const void *key, int len, const void *rsc );

u32 sec80211_rsn_get_crypto_desc ( enum net80211_crypto_alg crypt, int rsnie );
u32 sec80211_rsn_get_akm_desc ( enum net80211_security_proto secprot,
				int rsnie );
enum net80211_crypto_alg sec80211_rsn_get_net80211_crypt ( u32 desc );

#endif /* _GPXE_SEC80211_H */

