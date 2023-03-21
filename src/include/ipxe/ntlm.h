#ifndef _IPXE_NTLM_H
#define _IPXE_NTLM_H

/** @file
 *
 * NT LAN Manager (NTLM) authentication
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/crypto.h>
#include <ipxe/md5.h>

/** A message header */
struct ntlm_header {
	/** Magic signature */
	uint8_t magic[8];
	/** Message type */
	uint32_t type;
} __attribute__ (( packed ));

/** Magic signature */
#define NTLM_MAGIC { 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0' }

/** Message types */
enum ntlm_type {
	/** Negotiate message type */
	NTLM_NEGOTIATE = 0x00000001UL,
	/** Challenge message type */
	NTLM_CHALLENGE = 0x00000002UL,
	/** Authenticate message */
	NTLM_AUTHENTICATE = 0x00000003UL,
};

/** Negotiation flags */
enum ntlm_flags {
	/** Negotiate key exchange */
	NTLM_NEGOTIATE_KEY_EXCH = 0x20000000UL,
	/** Negotiate extended security */
	NTLM_NEGOTIATE_EXTENDED_SESSIONSECURITY = 0x00080000UL,
	/** Negotiate always sign */
	NTLM_NEGOTIATE_ALWAYS_SIGN = 0x00008000UL,
	/** Negotiate NTLM key */
	NTLM_NEGOTIATE_NTLM = 0x00000200UL,
	/** Request target name and information */
	NTLM_REQUEST_TARGET = 0x00000004UL,
	/** Negotiate Unicode character encoding */
	NTLM_NEGOTIATE_UNICODE = 0x00000001UL,
};

/** A version descriptor */
struct ntlm_version {
	/** Product major version */
	uint8_t major;
	/** Product minor version */
	uint8_t minor;
	/** Product build number */
	uint16_t build;
	/** Reserved */
	uint8_t reserved[3];
	/** NTLMSSP revision */
	uint8_t revision;
} __attribute__ (( packed ));

/** A nonce */
struct ntlm_nonce {
	/** Raw bytes */
	uint8_t raw[8];
} __attribute__ (( packed ));

/** A variable-length data descriptor */
struct ntlm_data {
	/** Length (in bytes) */
	uint16_t len;
	/** Maximum length (in bytes)
	 *
	 * Should always be set equal to the length; this field is
	 * entirely superfluous.
	 */
	uint16_t max_len;
	/** Offset from start of message header */
	uint32_t offset;
} __attribute__ (( packed ));

/** A Negotiate message */
struct ntlm_negotiate {
	/** Message header */
	struct ntlm_header header;
	/** Negotiation flags */
	uint32_t flags;
	/** Domain name */
	struct ntlm_data domain;
	/** Workstation name */
	struct ntlm_data workstation;
} __attribute__ (( packed ));

/** A Challenge message */
struct ntlm_challenge {
	/** Message header */
	struct ntlm_header header;
	/** Target name */
	struct ntlm_data name;
	/** Negotiation flags */
	uint32_t flags;
	/** Server nonce */
	struct ntlm_nonce nonce;
	/** Reserved */
	uint8_t reserved[8];
	/** Target information */
	struct ntlm_data info;
} __attribute__ (( packed ));

/** An Authenticate message */
struct ntlm_authenticate {
	/** Message header */
	struct ntlm_header header;
	/** LAN Manager response */
	struct ntlm_data lm;
	/** NT response */
	struct ntlm_data nt;
	/** Domain name */
	struct ntlm_data domain;
	/** User name */
	struct ntlm_data user;
	/** Workstation name */
	struct ntlm_data workstation;
	/** Session key */
	struct ntlm_data session;
	/** Negotiation flags */
	uint32_t flags;
} __attribute__ (( packed ));

/** A LAN Manager response */
struct ntlm_lm_response {
	/** HMAC-MD5 digest */
	uint8_t digest[MD5_DIGEST_SIZE];
	/** Client nonce */
	struct ntlm_nonce nonce;
} __attribute__ (( packed ));

/** An NT response */
struct ntlm_nt_response {
	/** HMAC-MD5 digest */
	uint8_t digest[MD5_DIGEST_SIZE];
	/** Response version */
	uint8_t version;
	/** Highest response version */
	uint8_t high;
	/** Reserved */
	uint8_t reserved_a[6];
	/** Current time */
	uint64_t time;
	/** Client nonce */
	struct ntlm_nonce nonce;
	/** Must be zero */
	uint32_t zero;
} __attribute__ (( packed ));

/** NTLM version */
#define NTLM_VERSION_NTLMV2 0x01

/** NTLM challenge information */
struct ntlm_challenge_info {
	/** Server nonce */
	struct ntlm_nonce *nonce;
	/** Target information */
	void *target;
	/** Length of target information */
	size_t len;
};

/** An NTLM verification key */
struct ntlm_key {
	/** Raw bytes */
	uint8_t raw[MD5_DIGEST_SIZE];
};

extern const struct ntlm_negotiate ntlm_negotiate;
extern int ntlm_challenge ( struct ntlm_challenge *challenge, size_t len,
			    struct ntlm_challenge_info *info );
extern void ntlm_key ( const char *domain, const char *username,
		       const char *password, struct ntlm_key *key );
extern void ntlm_response ( struct ntlm_challenge_info *info,
			    struct ntlm_key *key, struct ntlm_nonce *nonce,
			    struct ntlm_lm_response *lm,
			    struct ntlm_nt_response *nt );
extern size_t ntlm_authenticate ( struct ntlm_challenge_info *info,
				  const char *domain, const char *username,
				  const char *workstation,
				  struct ntlm_lm_response *lm,
				  struct ntlm_nt_response *nt,
				  struct ntlm_authenticate *auth );
extern size_t ntlm_authenticate_len ( struct ntlm_challenge_info *info,
				      const char *domain, const char *username,
				      const char *workstation );

#endif /* _IPXE_NTLM_H */
