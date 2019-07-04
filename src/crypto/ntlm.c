/*
 * Copyright (C) 2017 Michael Brown <mbrown@fensystems.co.uk>.
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
 * NT LAN Manager (NTLM) authentication
 *
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/md4.h>
#include <ipxe/md5.h>
#include <ipxe/hmac.h>
#include <ipxe/ntlm.h>

/** Negotiate message
 *
 * This message content is fixed since there is no need to specify the
 * calling workstation name or domain name, and the set of flags is
 * mandated by the MS-NLMP specification.
 */
const struct ntlm_negotiate ntlm_negotiate = {
	.header = {
		.magic = NTLM_MAGIC,
		.type = cpu_to_le32 ( NTLM_NEGOTIATE ),
	},
	.flags = cpu_to_le32 ( NTLM_NEGOTIATE_EXTENDED_SESSIONSECURITY |
			       NTLM_NEGOTIATE_ALWAYS_SIGN |
			       NTLM_NEGOTIATE_NTLM |
			       NTLM_REQUEST_TARGET |
			       NTLM_NEGOTIATE_UNICODE ),
};

/**
 * Parse NTLM Challenge
 *
 * @v challenge		Challenge message
 * @v len		Length of Challenge message
 * @v info		Challenge information to fill in
 * @ret rc		Return status code
 */
int ntlm_challenge ( struct ntlm_challenge *challenge, size_t len,
		     struct ntlm_challenge_info *info ) {
	size_t offset;

	DBGC ( challenge, "NTLM challenge message:\n" );
	DBGC_HDA ( challenge, 0, challenge, len );

	/* Sanity checks */
	if ( len < sizeof ( *challenge ) ) {
		DBGC ( challenge, "NTLM underlength challenge (%zd bytes)\n",
		       len );
		return -EINVAL;
	}

	/* Extract nonce */
	info->nonce = &challenge->nonce;
	DBGC ( challenge, "NTLM challenge nonce:\n" );
	DBGC_HDA ( challenge, 0, info->nonce, sizeof ( *info->nonce ) );

	/* Extract target information */
	info->len = le16_to_cpu ( challenge->info.len );
	offset = le32_to_cpu ( challenge->info.offset );
	if ( ( offset > len ) ||
	     ( info->len > ( len - offset ) ) ) {
		DBGC ( challenge, "NTLM target information outside "
		       "challenge\n" );
		DBGC_HDA ( challenge, 0, challenge, len );
		return -EINVAL;
	}
	info->target = ( ( ( void * ) challenge ) + offset );
	DBGC ( challenge, "NTLM challenge target information:\n" );
	DBGC_HDA ( challenge, 0, info->target, info->len );

	return 0;
}

/**
 * Calculate NTLM verification key
 *
 * @v domain		Domain name (or NULL)
 * @v username		User name (or NULL)
 * @v password		Password (or NULL)
 * @v key		Key to fill in
 *
 * This is the NTOWFv2() function as defined in MS-NLMP.
 */
void ntlm_key ( const char *domain, const char *username,
		const char *password, struct ntlm_key *key ) {
	struct digest_algorithm *md4 = &md4_algorithm;
	struct digest_algorithm *md5 = &md5_algorithm;
	union {
		uint8_t md4[MD4_CTX_SIZE];
		uint8_t md5[MD5_CTX_SIZE];
	} ctx;
	uint8_t digest[MD4_DIGEST_SIZE];
	size_t digest_len;
	uint8_t c;
	uint16_t wc;

	/* Use empty usernames/passwords if not specified */
	if ( ! domain )
		domain = "";
	if ( ! username )
		username = "";
	if ( ! password )
		password = "";

	/* Construct MD4 digest of (Unicode) password */
	digest_init ( md4, ctx.md4 );
	while ( ( c = *(password++) ) ) {
		wc = cpu_to_le16 ( c );
		digest_update ( md4, ctx.md4, &wc, sizeof ( wc ) );
	}
	digest_final ( md4, ctx.md4, digest );

	/* Construct HMAC-MD5 of (Unicode) upper-case username */
	digest_len = sizeof ( digest );
	hmac_init ( md5, ctx.md5, digest, &digest_len );
	while ( ( c = *(username++) ) ) {
		wc = cpu_to_le16 ( toupper ( c ) );
		hmac_update ( md5, ctx.md5, &wc, sizeof ( wc ) );
	}
	while ( ( c = *(domain++) ) ) {
		wc = cpu_to_le16 ( c );
		hmac_update ( md5, ctx.md5, &wc, sizeof ( wc ) );
	}
	hmac_final ( md5, ctx.md5, digest, &digest_len, key->raw );
	DBGC ( key, "NTLM key:\n" );
	DBGC_HDA ( key, 0, key, sizeof ( *key ) );
}

/**
 * Construct NTLM responses
 *
 * @v info		Challenge information
 * @v key		Verification key
 * @v nonce		Nonce, or NULL to use a random nonce
 * @v lm		LAN Manager response to fill in
 * @v nt		NT response to fill in
 */
void ntlm_response ( struct ntlm_challenge_info *info, struct ntlm_key *key,
		     struct ntlm_nonce *nonce, struct ntlm_lm_response *lm,
		     struct ntlm_nt_response *nt ) {
	struct digest_algorithm *md5 = &md5_algorithm;
	struct ntlm_nonce tmp_nonce;
	uint8_t ctx[MD5_CTX_SIZE];
	size_t key_len = sizeof ( *key );
	unsigned int i;

	/* Generate random nonce, if needed */
	if ( ! nonce ) {
		for ( i = 0 ; i < sizeof ( tmp_nonce ) ; i++ )
			tmp_nonce.raw[i] = random();
		nonce = &tmp_nonce;
	}

	/* Construct LAN Manager response */
	memcpy ( &lm->nonce, nonce, sizeof ( lm->nonce ) );
	hmac_init ( md5, ctx, key->raw, &key_len );
	hmac_update ( md5, ctx, info->nonce, sizeof ( *info->nonce ) );
	hmac_update ( md5, ctx, &lm->nonce, sizeof ( lm->nonce ) );
	hmac_final ( md5, ctx, key->raw, &key_len, lm->digest );
	DBGC ( key, "NTLM LAN Manager response:\n" );
	DBGC_HDA ( key, 0, lm, sizeof ( *lm ) );

	/* Construct NT response */
	memset ( nt, 0, sizeof ( *nt ) );
	nt->version = NTLM_VERSION_NTLMV2;
	nt->high = NTLM_VERSION_NTLMV2;
	memcpy ( &nt->nonce, nonce, sizeof ( nt->nonce ) );
	hmac_init ( md5, ctx, key->raw, &key_len );
	hmac_update ( md5, ctx, info->nonce, sizeof ( *info->nonce ) );
	hmac_update ( md5, ctx, &nt->version,
		      ( sizeof ( *nt ) -
			offsetof ( typeof ( *nt ), version ) ) );
	hmac_update ( md5, ctx, info->target, info->len );
	hmac_update ( md5, ctx, &nt->zero, sizeof ( nt->zero ) );
	hmac_final ( md5, ctx, key->raw, &key_len, nt->digest );
	DBGC ( key, "NTLM NT response prefix:\n" );
	DBGC_HDA ( key, 0, nt, sizeof ( *nt ) );
}

/**
 * Append data to NTLM message
 *
 * @v header		Message header, or NULL to only calculate next payload
 * @v data		Data descriptor
 * @v payload		Data payload
 * @v len		Length of data
 * @ret payload		Next data payload
 */
static void * ntlm_append ( struct ntlm_header *header, struct ntlm_data *data,
			    void *payload, size_t len ) {

	/* Populate data descriptor */
	if ( header ) {
		data->offset = cpu_to_le32 ( payload - ( ( void * ) header ) );
		data->len = data->max_len = cpu_to_le16 ( len );
	}

	return ( payload + len );
}

/**
 * Append Unicode string data to NTLM message
 *
 * @v header		Message header, or NULL to only calculate next payload
 * @v data		Data descriptor
 * @v payload		Data payload
 * @v string		String to append, or NULL
 * @ret payload		Next data payload
 */
static void * ntlm_append_string ( struct ntlm_header *header,
				   struct ntlm_data *data, void *payload,
				   const char *string ) {
	uint16_t *tmp = payload;
	uint8_t c;

	/* Convert string to Unicode */
	for ( tmp = payload ; ( string && ( c = *(string++) ) ) ; tmp++ ) {
		if ( header )
			*tmp = cpu_to_le16 ( c );
	}

	/* Append string data */
	return ntlm_append ( header, data, payload,
			     ( ( ( void * ) tmp ) - payload ) );
}

/**
 * Construct NTLM Authenticate message
 *
 * @v info		Challenge information
 * @v domain		Domain name, or NULL
 * @v username		User name, or NULL
 * @v workstation	Workstation name, or NULL
 * @v lm		LAN Manager response
 * @v nt		NT response
 * @v auth		Message to fill in, or NULL to only calculate length
 * @ret len		Length of message
 */
size_t ntlm_authenticate ( struct ntlm_challenge_info *info, const char *domain,
			   const char *username, const char *workstation,
			   struct ntlm_lm_response *lm,
			   struct ntlm_nt_response *nt,
			   struct ntlm_authenticate *auth ) {
	void *tmp;
	size_t nt_len;
	size_t len;

	/* Construct response header */
	if ( auth ) {
		memset ( auth, 0, sizeof ( *auth ) );
		memcpy ( auth->header.magic, ntlm_negotiate.header.magic,
			 sizeof ( auth->header.magic ) );
		auth->header.type = cpu_to_le32 ( NTLM_AUTHENTICATE );
		auth->flags = ntlm_negotiate.flags;
	}
	tmp = ( ( ( void * ) auth ) + sizeof ( *auth ) );

	/* Construct LAN Manager response */
	if ( auth )
		memcpy ( tmp, lm, sizeof ( *lm ) );
	tmp = ntlm_append ( &auth->header, &auth->lm, tmp, sizeof ( *lm ) );

	/* Construct NT response */
	nt_len = ( sizeof ( *nt ) + info->len + sizeof ( nt->zero ) );
	if ( auth ) {
		memcpy ( tmp, nt, sizeof ( *nt ) );
		memcpy ( ( tmp + sizeof ( *nt ) ), info->target, info->len );
		memset ( ( tmp + sizeof ( *nt ) + info->len ), 0,
			 sizeof ( nt->zero ) );
	}
	tmp = ntlm_append ( &auth->header, &auth->nt, tmp, nt_len );

	/* Populate domain, user, and workstation names */
	tmp = ntlm_append_string ( &auth->header, &auth->domain, tmp, domain );
	tmp = ntlm_append_string ( &auth->header, &auth->user, tmp, username );
	tmp = ntlm_append_string ( &auth->header, &auth->workstation, tmp,
				   workstation );

	/* Calculate length */
	len = ( tmp - ( ( void * ) auth ) );
	if ( auth ) {
		DBGC ( auth, "NTLM authenticate message:\n" );
		DBGC_HDA ( auth, 0, auth, len );
	}

	return len;
}

/**
 * Calculate NTLM Authenticate message length
 *
 * @v info		Challenge information
 * @v domain		Domain name, or NULL
 * @v username		User name, or NULL
 * @v workstation	Workstation name, or NULL
 * @ret len		Length of Authenticate message
 */
size_t ntlm_authenticate_len ( struct ntlm_challenge_info *info,
			       const char *domain, const char *username,
			       const char *workstation ) {

	return ntlm_authenticate ( info, domain, username, workstation,
				   NULL, NULL, NULL );
}
