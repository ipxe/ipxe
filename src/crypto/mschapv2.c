/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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
 * MS-CHAPv2 authentication
 *
 * The algorithms used for MS-CHAPv2 authentication are defined in
 * RFC 2759 section 8.
 */

#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/md4.h>
#include <ipxe/sha1.h>
#include <ipxe/des.h>
#include <ipxe/mschapv2.h>

/**
 * MS-CHAPv2 context block
 *
 * For no particularly discernible reason, MS-CHAPv2 uses two
 * different digest algorithms and one block cipher.  The uses do not
 * overlap, so share the context storage between these to reduce stack
 * usage.
 */
union mschapv2_context {
	/** SHA-1 digest context */
	uint8_t sha1[SHA1_CTX_SIZE];
	/** MD4 digest context */
	uint8_t md4[MD4_CTX_SIZE];
	/** DES cipher context */
	uint8_t des[DES_CTX_SIZE];
};

/**
 * MS-CHAPv2 challenge hash
 *
 * MS-CHAPv2 calculates the SHA-1 digest of the peer challenge, the
 * authenticator challenge, and the username, and then uses only the
 * first 8 bytes of the result (as a DES plaintext block).
 */
union mschapv2_challenge_hash {
	/** SHA-1 digest */
	uint8_t sha1[SHA1_DIGEST_SIZE];
	/** DES plaintext block */
	uint8_t des[DES_BLOCKSIZE];
};

/**
 * MS-CHAPv2 password hash
 *
 * MS-CHAPv2 calculates the MD4 digest of an unspecified two-byte
 * little-endian Unicode encoding (presumably either UCS-2LE or
 * UTF-16LE) of the password.
 *
 * For constructing the challenge response, the MD4 digest is then
 * zero-padded to 21 bytes and used as three separate 56-bit DES keys.
 *
 * For constructing the authenticator response, the MD4 digest is then
 * used as an input to a SHA-1 digest along with the NT response and a
 * magic constant.
 */
union mschapv2_password_hash {
	/** MD4 digest */
	uint8_t md4[MD4_DIGEST_SIZE];
	/** SHA-1 digest */
	uint8_t sha1[SHA1_DIGEST_SIZE];
	/** DES keys */
	uint8_t des[3][DES_BLOCKSIZE];
	/** DES key expansion */
	uint8_t expand[ 3 * DES_BLOCKSIZE ];
};

/** MS-CHAPv2 magic constant 1 */
static const char mschapv2_magic1[39] =
	"Magic server to client signing constant";

/** MS-CHAPv2 magic constant 2 */
static const char mschapv2_magic2[41] =
	"Pad to make it do more than one iteration";

/**
 * Calculate MS-CHAPv2 challenge hash
 *
 * @v ctx		Context block
 * @v challenge		Authenticator challenge
 * @v peer		Peer challenge
 * @v username		User name (or NULL to use empty string)
 * @v chash		Challenge hash to fill in
 *
 * This is the ChallengeHash() function as documented in RFC 2759
 * section 8.2.
 */
static void
mschapv2_challenge_hash ( union mschapv2_context *ctx,
			  const struct mschapv2_challenge *challenge,
			  const struct mschapv2_challenge *peer,
			  const char *username,
			  union mschapv2_challenge_hash *chash ) {
	struct digest_algorithm *sha1 = &sha1_algorithm;

	/* Calculate SHA-1 hash of challenges and username */
	digest_init ( sha1, ctx->sha1 );
	digest_update ( sha1, ctx->sha1, peer, sizeof ( *peer ) );
	digest_update ( sha1, ctx->sha1, challenge, sizeof ( *challenge ) );
	if ( username ) {
		digest_update ( sha1, ctx->sha1, username,
				strlen ( username ) );
	}
	digest_final ( sha1, ctx->sha1, chash->sha1 );
	DBGC ( ctx, "MSCHAPv2 authenticator challenge:\n" );
	DBGC_HDA ( ctx, 0, challenge, sizeof ( *challenge ) );
	DBGC ( ctx, "MSCHAPv2 peer challenge:\n" );
	DBGC_HDA ( ctx, 0, peer, sizeof ( *peer ) );
	DBGC ( ctx, "MSCHAPv2 challenge hash:\n" );
	DBGC_HDA ( ctx, 0, chash->des, sizeof ( chash->des ) );
}

/**
 * Calculate MS-CHAPv2 password hash
 *
 * @v ctx		Context block
 * @v password		Password (or NULL to use empty string)
 * @v phash		Password hash to fill in
 *
 * This is the NtPasswordHash() function as documented in RFC 2759
 * section 8.3.
 */
static void mschapv2_password_hash ( union mschapv2_context *ctx,
				     const char *password,
				     union mschapv2_password_hash *phash ) {
	struct digest_algorithm *md4 = &md4_algorithm;
	uint16_t wc;
	uint8_t c;

	/* Construct zero-padded MD4 hash of encoded password */
	memset ( phash, 0, sizeof ( *phash ) );
	digest_init ( md4, ctx->md4 );
	if ( password ) {
		while ( ( c = *(password++) ) ) {
			wc = cpu_to_le16 ( c );
			digest_update ( md4, ctx->md4, &wc, sizeof ( wc ) );
		}
	}
	digest_final ( md4, ctx->md4, phash->md4 );
	DBGC ( ctx, "MSCHAPv2 password hash:\n" );
	DBGC_HDA ( ctx, 0, phash->md4, sizeof ( phash->md4 ) );
}

/**
 * Hash the MS-CHAPv2 password hash
 *
 * @v ctx		Context block
 * @v phash		Password hash to be rehashed
 *
 * This is the HashNtPasswordHash() function as documented in RFC 2759
 * section 8.4.
 */
static void mschapv2_hash_hash ( union mschapv2_context *ctx,
				 union mschapv2_password_hash *phash ) {
	struct digest_algorithm *md4 = &md4_algorithm;

	/* Calculate MD4 hash of existing MD4 hash */
	digest_init ( md4, ctx->md4 );
	digest_update ( md4, ctx->md4, phash->md4, sizeof ( phash->md4 ) );
	digest_final ( md4, ctx->md4, phash->md4 );
	DBGC ( ctx, "MSCHAPv2 password hash hash:\n" );
	DBGC_HDA ( ctx, 0, phash->md4, sizeof ( phash->md4 ) );
}

/**
 * Expand MS-CHAPv2 password hash by inserting DES dummy parity bits
 *
 * @v ctx		Context block
 * @v phash		Password hash to expand
 *
 * This is part of the DesEncrypt() function as documented in RFC 2759
 * section 8.6.
 */
static void mschapv2_expand_hash ( union mschapv2_context *ctx,
				   union mschapv2_password_hash *phash ) {
	uint8_t *dst;
	uint8_t *src;
	unsigned int i;

	/* Expand password hash by inserting (unused) DES parity bits */
	for ( i = ( sizeof ( phash->expand ) - 1 ) ; i > 0 ; i-- ) {
		dst = &phash->expand[i];
		src = ( dst - ( i / 8 ) );
		*dst = ( ( ( src[-1] << 8 ) | src[0] ) >> ( i % 8 ) );
	}
	DBGC ( ctx, "MSCHAPv2 expanded password hash:\n" );
	DBGC_HDA ( ctx, 0, phash->expand, sizeof ( phash->expand ) );
}

/**
 * Calculate MS-CHAPv2 challenge response
 *
 * @v ctx		Context block
 * @v chash		Challenge hash
 * @v phash		Password hash (after expansion)
 * @v nt		NT response to fill in
 *
 * This is the ChallengeResponse() function as documented in RFC 2759
 * section 8.5.
 */
static void
mschapv2_challenge_response ( union mschapv2_context *ctx,
			      const union mschapv2_challenge_hash *chash,
			      const union mschapv2_password_hash *phash,
			      struct mschapv2_nt_response *nt ) {
	struct cipher_algorithm *des = &des_algorithm;
	unsigned int i;
	int rc;

	/* Construct response.  The design of the algorithm here is
	 * interesting, suggesting that an intern at Microsoft had
	 * heard the phrase "Triple DES" and hazarded a blind guess at
	 * what it might mean.
	 */
	for ( i = 0 ; i < ( sizeof ( phash->des ) /
			    sizeof ( phash->des[0] ) ) ; i++ ) {
		rc = cipher_setkey ( des, ctx->des, phash->des[i],
				     sizeof ( phash->des[i] ) );
		assert ( rc == 0 ); /* no failure mode exists */
		cipher_encrypt ( des, ctx->des, chash->des, nt->block[i],
				 sizeof ( chash->des ) );
	}
	DBGC ( ctx, "MSCHAPv2 NT response:\n" );
	DBGC_HDA ( ctx, 0, nt, sizeof ( *nt ) );
}

/**
 * Calculate MS-CHAPv2 challenge response
 *
 * @v username		User name (or NULL to use empty string)
 * @v password		Password (or NULL to use empty string)
 * @v challenge		Authenticator challenge
 * @v peer		Peer challenge
 * @v response		Challenge response to fill in
 *
 * This is essentially the GenerateNTResponse() function as documented
 * in RFC 2759 section 8.1.
 */
void mschapv2_response ( const char *username, const char *password,
			 const struct mschapv2_challenge *challenge,
			 const struct mschapv2_challenge *peer,
			 struct mschapv2_response *response ) {
	union mschapv2_context ctx;
	union mschapv2_challenge_hash chash;
	union mschapv2_password_hash phash;

	/* Zero reserved fields */
	memset ( response, 0, sizeof ( *response ) );

	/* Copy peer challenge to response */
	memcpy ( &response->peer, peer, sizeof ( response->peer ) );

	/* Construct challenge hash */
	mschapv2_challenge_hash ( &ctx, challenge, peer, username, &chash );

	/* Construct expanded password hash */
	mschapv2_password_hash ( &ctx, password, &phash );
	mschapv2_expand_hash ( &ctx, &phash );

	/* Construct NT response */
	mschapv2_challenge_response ( &ctx, &chash, &phash, &response->nt );
	DBGC ( &ctx, "MSCHAPv2 challenge response:\n" );
	DBGC_HDA ( &ctx, 0, response, sizeof ( *response ) );
}

/**
 * Calculate MS-CHAPv2 authenticator response
 *
 * @v username		User name (or NULL to use empty string)
 * @v password		Password (or NULL to use empty string)
 * @v challenge		Authenticator challenge
 * @v response		Challenge response
 * @v auth		Authenticator response to fill in
 *
 * This is essentially the GenerateAuthenticatorResponse() function as
 * documented in RFC 2759 section 8.7.
 */
void mschapv2_auth ( const char *username, const char *password,
		     const struct mschapv2_challenge *challenge,
		     const struct mschapv2_response *response,
		     struct mschapv2_auth *auth ) {
	struct digest_algorithm *sha1 = &sha1_algorithm;
	union mschapv2_context ctx;
	union mschapv2_challenge_hash chash;
	union mschapv2_password_hash phash;
	char tmp[3];
	char *wtf;
	unsigned int i;

	/* Construct hash of password hash */
	mschapv2_password_hash ( &ctx, password, &phash );
	mschapv2_hash_hash ( &ctx, &phash );

	/* Construct unnamed intermediate hash */
	digest_init ( sha1, ctx.sha1 );
	digest_update ( sha1, ctx.sha1, phash.md4, sizeof ( phash.md4 ) );
	digest_update ( sha1, ctx.sha1, &response->nt,
			sizeof ( response->nt ) );
	digest_update ( sha1, ctx.sha1, mschapv2_magic1,
			sizeof ( mschapv2_magic1 ) );
	digest_final ( sha1, ctx.sha1, phash.sha1 );
	DBGC ( &ctx, "MSCHAPv2 NT response:\n" );
	DBGC_HDA ( &ctx, 0, &response->nt, sizeof ( response->nt ) );
	DBGC ( &ctx, "MSCHAPv2 unnamed intermediate hash:\n" );
	DBGC_HDA ( &ctx, 0, phash.sha1, sizeof ( phash.sha1 ) );

	/* Construct challenge hash */
	mschapv2_challenge_hash ( &ctx, challenge, &response->peer,
				  username, &chash );

	/* Construct authenticator response hash */
	digest_init ( sha1, ctx.sha1 );
	digest_update ( sha1, ctx.sha1, phash.sha1, sizeof ( phash.sha1 ) );
	digest_update ( sha1, ctx.sha1, chash.des, sizeof ( chash.des ) );
	digest_update ( sha1, ctx.sha1, mschapv2_magic2,
			sizeof ( mschapv2_magic2 ) );
	digest_final ( sha1, ctx.sha1, phash.sha1 );
	DBGC ( &ctx, "MSCHAPv2 authenticator response hash:\n" );
	DBGC_HDA ( &ctx, 0, phash.sha1, sizeof ( phash.sha1 ) );

	/* Encode authenticator response hash */
	wtf = auth->wtf;
	*(wtf++) = 'S';
	*(wtf++) = '=';
	DBGC ( &ctx, "MSCHAPv2 authenticator response: S=" );
	for ( i = 0 ; i < sizeof ( phash.sha1 ) ; i++ ) {
		snprintf ( tmp, sizeof ( tmp ), "%02X", phash.sha1[i] );
		*(wtf++) = tmp[0];
		*(wtf++) = tmp[1];
		DBGC ( &ctx, "%s", tmp );
	}
	DBGC ( &ctx, "\n" );
}
