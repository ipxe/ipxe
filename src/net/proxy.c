#include <string.h>
#include <ipxe/proxy.h>
#include <ipxe/uri.h>
#include <ipxe/settings.h>

/** @file
 *
 * HTTP Proxy
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

struct uri *proxy_uri = NULL;

/** HTTP proxy address setting */
struct setting http_proxy_setting __setting ( SETTING_MISC ) = {
	.name = "http-proxy",
	.description = "Address and port of the HTTP (not HTTPS) proxy to use, as a http scheme URI",
	.type = &setting_type_string,
};

int is_proxy_set ( ) {
	/* Later, this may be expanded to encompass other settings */
	if ( ! proxy_uri ) {
		proxy_uri = get_proxy();
	}
	return ! ! proxy_uri;
}

struct uri *get_proxy ( ) {
	char *http_proxy_unexpanded, *http_proxy;

	if ( setting_exists ( NULL, &http_proxy_setting ) && ! proxy_uri ) {
		/* Later, this may select from multiple settings*/
		fetch_string_setting_copy ( NULL, &http_proxy_setting, &http_proxy_unexpanded );
		http_proxy = expand_settings ( http_proxy_unexpanded );
		proxy_uri = parse_uri ( http_proxy );
		free ( http_proxy_unexpanded );
		free ( http_proxy );
		/* Only the http scheme is currently supported */
		if ( strcmp ( proxy_uri->scheme, "http" ) != 0 ) {
			uri_put ( proxy_uri );
			DBG ( "http-proxy must begin with \"http://\"" );
			return NULL;
		}
	}

	return proxy_uri;
}

const char *proxied_uri_host ( struct uri *uri ) {
	/* Later, this could select from multiple proxies,
	based on hostname patterns matched against the uri */
	if ( is_proxy_set ( ) ) {
		return proxy_uri->host;
	} else {
		return uri->host;
	}
}

unsigned int proxied_uri_port ( struct uri *uri, unsigned int default_port ) {
	/* Later, this could select from multiple proxies,
	based on hostname patterns matched against the uri */
	if ( is_proxy_set ( ) ) {
		return uri_port ( proxy_uri, default_port);
	} else {
		return uri_port ( uri, default_port);
	}
}
