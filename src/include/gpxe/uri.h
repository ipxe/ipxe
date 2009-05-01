#ifndef _GPXE_URI_H
#define _GPXE_URI_H

/** @file
 *
 * Uniform Resource Identifiers
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <stdlib.h>
#include <gpxe/refcnt.h>

/** A Uniform Resource Identifier
 *
 * Terminology for this data structure is as per uri(7), except that
 * "path" is defined to include the leading '/' for an absolute path.
 *
 * Note that all fields within a URI are optional and may be NULL.
 *
 * Some examples are probably helpful:
 *
 *   http://www.etherboot.org/wiki :
 *
 *   scheme = "http", host = "www.etherboot.org", path = "/wiki"
 *
 *   /var/lib/tftpboot :
 *
 *   path = "/var/lib/tftpboot"
 *
 *   mailto:bob@nowhere.com :
 *
 *   scheme = "mailto", opaque = "bob@nowhere.com"
 *
 *   ftp://joe:secret@insecure.org:8081/hidden/path/to?what=is#this
 *
 *   scheme = "ftp", user = "joe", password = "secret",
 *   host = "insecure.org", port = "8081", path = "/hidden/path/to",
 *   query = "what=is", fragment = "this"
 */
struct uri {
	/** Reference count */
	struct refcnt refcnt;
	/** Scheme */
	const char *scheme;
	/** Opaque part */
	const char *opaque;
	/** User name */
	const char *user;
	/** Password */
	const char *password;
	/** Host name */
	const char *host;
	/** Port number */
	const char *port;
	/** Path */
	const char *path;
	/** Query */
	const char *query;
	/** Fragment */
	const char *fragment;
};

/**
 * URI is an absolute URI
 *
 * @v uri			URI
 * @ret is_absolute		URI is absolute
 *
 * An absolute URI begins with a scheme, e.g. "http:" or "mailto:".
 * Note that this is a separate concept from a URI with an absolute
 * path.
 */
static inline int uri_is_absolute ( struct uri *uri ) {
	return ( uri->scheme != NULL );
}

/**
 * URI has an absolute path
 *
 * @v uri			URI
 * @ret has_absolute_path	URI has an absolute path
 *
 * An absolute path begins with a '/'.  Note that this is a separate
 * concept from an absolute URI.  Note also that a URI may not have a
 * path at all.
 */
static inline int uri_has_absolute_path ( struct uri *uri ) {
	return ( uri->path && ( uri->path[0] == '/' ) );
}

/**
 * URI has a relative path
 *
 * @v uri			URI
 * @ret has_relative_path	URI has a relative path
 *
 * A relative path begins with something other than a '/'.  Note that
 * this is a separate concept from a relative URI.  Note also that a
 * URI may not have a path at all.
 */
static inline int uri_has_relative_path ( struct uri *uri ) {
	return ( uri->path && ( uri->path[0] != '/' ) );
}

/**
 * Increment URI reference count
 *
 * @v uri		URI, or NULL
 * @ret uri		URI as passed in
 */
static inline __attribute__ (( always_inline )) struct uri *
uri_get ( struct uri *uri ) {
	ref_get ( &uri->refcnt );
	return uri;
}

/**
 * Decrement URI reference count
 *
 * @v uri		URI, or NULL
 */
static inline __attribute__ (( always_inline )) void
uri_put ( struct uri *uri ) {
	ref_put ( &uri->refcnt );
}

extern struct uri *cwuri;

extern struct uri * parse_uri ( const char *uri_string );
extern unsigned int uri_port ( struct uri *uri, unsigned int default_port );
extern int unparse_uri ( char *buf, size_t size, struct uri *uri );
extern struct uri * uri_dup ( struct uri *uri );
extern char * resolve_path ( const char *base_path,
			     const char *relative_path );
extern struct uri * resolve_uri ( struct uri *base_uri,
				  struct uri *relative_uri );
extern void churi ( struct uri *uri );
extern size_t uri_encode ( const char *raw_string, char *buf, size_t len );
extern size_t uri_decode ( const char *encoded_string, char *buf, size_t len );

#endif /* _GPXE_URI_H */
