/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <ipxe/clientcert.h>

/** @file
 *
 * Client certificate store
 *
 * Life would in theory be easier if we could use a single file to
 * hold both the certificate and corresponding private key.
 * Unfortunately, the only common format which supports this is
 * PKCS#12 (aka PFX), which is too ugly to be allowed anywhere near my
 * codebase.  See, for reference and amusement:
 *
 *    http://www.cs.auckland.ac.nz/~pgut001/pubs/pfx.html
 *
 */

/* Sanity checks */
#if defined(CERTIFICATE) && ! defined(PRIVATE_KEY)
#warning "Attempting to embed certificate with no corresponding private key"
#endif
#if defined(PRIVATE_KEY) && ! defined(CERTIFICATE)
#warning "Attempting to embed private key with no corresponding certificate"
#endif

/* Raw client certificate data */
extern char client_certificate_data[];
extern char client_certificate_len[];
__asm__ ( ".section \".rodata\", \"a\", @progbits\n\t"
	  "\nclient_certificate_data:\n\t"
#ifdef CERTIFICATE
	  ".incbin \"" CERTIFICATE "\"\n\t"
#endif /* CERTIFICATE */
	  ".size client_certificate_data, ( . - client_certificate_data )\n\t"
	  ".equ client_certificate_len, ( . - client_certificate_data )\n\t"
	  ".previous\n\t" );

/** Client certificate */
struct client_certificate client_certificate = {
	.data = client_certificate_data,
	.len = ( ( size_t ) client_certificate_len ),
};

/* Raw client private key data */
extern char client_private_key_data[];
extern char client_private_key_len[];
__asm__ ( ".section \".rodata\", \"a\", @progbits\n\t"
	  "\nclient_private_key_data:\n\t"
#ifdef PRIVATE_KEY
	  ".incbin \"" PRIVATE_KEY "\"\n\t"
#endif /* PRIVATE_KEY */
	  ".size client_private_key_data, ( . - client_private_key_data )\n\t"
	  ".equ client_private_key_len, ( . - client_private_key_data )\n\t"
	  ".previous\n\t" );

/** Client private key */
struct client_private_key client_private_key = {
	.data = client_private_key_data,
	.len = ( ( size_t ) client_private_key_len ),
};
