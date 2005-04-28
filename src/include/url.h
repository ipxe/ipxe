#ifndef URL_H
#define URL_H

/*
 * Information parsed from a URL string.  "char *" pointers will point
 * to the start of the relevant portion of the original URL string,
 * which will have been modified by inserting terminating NULs at the
 * appropriate points.  Use unparse_url() if you want to get back the
 * original string.
 *
 */
struct url_info {
	char *protocol;
	char *host;
	char *port;
	char *file;
};

extern void parse_url ( struct url_info *info, char *url );
extern char * unparse_url ( struct url_info *info );

#endif /* URL_H */
