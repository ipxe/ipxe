#ifndef _IPXE_MSCHAPV2_H
#define _IPXE_MSCHAPV2_H

/** @file
 *
 * MS-CHAPv2 authentication
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

/** An MS-CHAPv2 challenge */
struct mschapv2_challenge {
	/** Raw bytes */
	uint8_t byte[16];
} __attribute__ (( packed ));

/** An MS-CHAPv2 NT response */
struct mschapv2_nt_response {
	/** DES-encrypted blocks */
	uint8_t block[3][8];
} __attribute__ (( packed ));

/** An MS-CHAPv2 challenge response */
struct mschapv2_response {
	/** Peer challenge */
	struct mschapv2_challenge peer;
	/** Reserved, must be zero */
	uint8_t reserved[8];
	/** NT response */
	struct mschapv2_nt_response nt;
	/** Flags, must be zero */
	uint8_t flags;
} __attribute__ (( packed ));

/** An MS-CHAPv2 authenticator response */
struct mschapv2_auth {
	/** Authenticator response string
	 *
	 * This is an unterminated 42-byte string of the form
	 * "S=<auth_string>" where <auth_string> is the upper-cased
	 * hexadecimal encoding of the actual authenticator response
	 * value.  Joy.
	 */
	char wtf[42];
} __attribute__ (( packed ));

extern void mschapv2_response ( const char *username, const char *password,
				const struct mschapv2_challenge *challenge,
				const struct mschapv2_challenge *peer,
				struct mschapv2_response *response );
extern void mschapv2_auth ( const char *username, const char *password,
			    const struct mschapv2_challenge *challenge,
			    const struct mschapv2_response *response,
			    struct mschapv2_auth *auth );

#endif /* _IPXE_MSCHAPV2_H */
