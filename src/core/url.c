#include "string.h"
#include "url.h"

/*
 * Parse a URL string into its constituent parts.
 *
 * We accept URLs of the form
 *
 *   [protocol://[host][:port]/]path/to/file
 *
 * We return true for success, 0 for failure (e.g. unknown protocol).
 * The URL string will be modified by having NULs inserted after
 * "protocol", "host" and "port".  The original URL can be
 * reconstructed by calling unparse_url.
 *
 */
void parse_url ( struct url_info *info, char *url ) {
	char *p;

	/* Zero the structure */
	memset ( info, 0, sizeof ( *info ) );

	/* Search for a protocol delimiter */
	for ( p = url ; *p ; p++ ) {
		if ( memcmp ( p, "://", 3 ) != 0 )
			continue;

		/* URL has an explicit protocol */
		info->protocol = url;
		*p = '\0';
		p += 3;
		info->host = p;

		/* Search for port or file delimiter */
		for ( ; *p ; p++ ) {
			if ( *p == ':' ) {
				*p = '\0';
				info->port = p + 1;
				continue;
			}
			if ( *p == '/' ) {
				*(p++) = '\0';
				break;
			}
		}
		info->file = p;
		return;
	}

	/* URL has no explicit protocol; is just a filename */
	info->file = url;
}

/*
 * Restore a parsed URL to its original pristine form.
 *
 */
char * unparse_url ( struct url_info *info ) {
	if ( info->protocol ) {
		/* URL had a protocol: fill in the deleted separators */
		info->file[-1] = '/';
		if ( info->port ) {
			info->port[-1] = ':';
		}
		info->host[-3] = ':';
		return info->protocol;
	} else {
		/* URL had no protocol; was just a filename */
		return info->file;
	}
}
