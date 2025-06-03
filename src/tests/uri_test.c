/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
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
 * URI self-tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <string.h>
#include <byteswap.h>
#include <ipxe/uri.h>
#include <ipxe/tcpip.h>
#include <ipxe/params.h>
#include <ipxe/test.h>

/** A URI parsing/formatting test */
struct uri_test {
	/** URI string */
	const char *string;
	/** URI */
	struct uri uri;
};

/** A URI port number test */
struct uri_port_test {
	/** URI string */
	const char *string;
	/** Default port number */
	unsigned int default_port;
	/** Expected port number */
	unsigned int port;
};

/** A URI or path resolution test */
struct uri_resolve_test {
	/** Base path or URI */
	const char *base;
	/** Relative path or URI */
	const char *relative;
	/** Expected resolved path or URI */
	const char *resolved;
};

/** A PXE URI test */
struct uri_pxe_test {
	/** Server address */
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
		struct sockaddr_tcpip st;
	} server;
	/** Filename */
	const char *filename;
	/** URI */
	struct uri uri;
	/** URI string (for display only; cannot be reparsed) */
	const char *string;
};

/** A current working URI test */
struct uri_churi_test {
	/** Relative URI */
	const char *relative;
	/** Expected new working URI */
	const char *expected;
};

/** A request parameter URI test list */
struct uri_params_test_list {
	/** Key */
	const char *key;
	/** Value */
	const char *value;
	/** Flags */
	unsigned int flags;
};

/** A request parameter URI test */
struct uri_params_test {
	/** URI string */
	const char *string;
	/** URI */
	struct uri uri;
	/** Parameter list name */
	const char *name;
	/** Parameter list */
	struct uri_params_test_list *list;
};

/**
 * Compare two URI component strings
 *
 * @v first		First string, or NULL
 * @v second		Second string, or NULL
 * @v difference	Difference
 */
static int uristrcmp ( const char *first, const char *second ) {

	/* Compare strings, allowing for either to be NULL */
	if ( first == second ) {
		return 0;
	} else if ( ( first == NULL ) || ( second == NULL ) ) {
		return -1;
	} else {
		return strcmp ( first, second );
	}
}

/**
 * Report URI equality test result
 *
 * @v uri		URI
 * @v expected		Expected URI
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_okx ( struct uri *uri, struct uri *expected, const char *file,
		      unsigned int line ) {

	okx ( uristrcmp ( uri->scheme, expected->scheme ) == 0, file, line );
	okx ( uristrcmp ( uri->opaque, expected->opaque ) == 0, file, line );
	okx ( uristrcmp ( uri->user, expected->user ) == 0, file, line );
	okx ( uristrcmp ( uri->password, expected->password ) == 0, file, line);
	okx ( uristrcmp ( uri->host, expected->host ) == 0, file, line );
	okx ( uristrcmp ( uri->port, expected->port ) == 0, file, line );
	okx ( uristrcmp ( uri->path, expected->path ) == 0, file, line );
	okx ( uristrcmp ( uri->epath, expected->epath ) == 0, file, line );
	okx ( uristrcmp ( uri->equery, expected->equery ) == 0, file, line );
	okx ( uristrcmp ( uri->efragment, expected->efragment ) == 0,
	      file, line);
	okx ( uri->params == expected->params, file, line );
}
#define uri_ok( uri, expected ) uri_okx ( uri, expected, __FILE__, __LINE__ )

/**
 * Report URI parsing test result
 *
 * @v test		URI test
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_parse_okx ( struct uri_test *test, const char *file,
			    unsigned int line ) {
	struct uri *uri;

	/* Parse URI */
	uri = parse_uri ( test->string );
	okx ( uri != NULL, file, line );
	if ( uri )
		uri_okx ( uri, &test->uri, file, line );
	uri_put ( uri );
}
#define uri_parse_ok( test ) uri_parse_okx ( test, __FILE__, __LINE__ )

/**
 * Report URI formatting test result
 *
 * @v test		URI test
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_format_okx ( struct uri_test *test, const char *file,
			     unsigned int line ) {
	char buf[ strlen ( test->string ) + 1 /* NUL */ ];
	char *tmp;
	size_t len;

	/* Format into fixed-size buffer */
	len = format_uri ( &test->uri, buf, sizeof ( buf ) );
	okx ( len == ( sizeof ( buf ) - 1 /* NUL */ ), file, line );
	okx ( strcmp ( buf, test->string ) == 0, file, line );

	/* Format into temporarily allocated buffer */
	tmp = format_uri_alloc ( &test->uri );
	okx ( tmp != NULL, file, line );
	if ( tmp )
		okx ( strcmp ( tmp, test->string ) == 0, file, line );
	free ( tmp );
}
#define uri_format_ok( test ) uri_format_okx ( test, __FILE__, __LINE__ )

/**
 * Report URI duplication test result
 *
 * @v test		URI
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_dup_okx ( struct uri *uri, const char *file,
			  unsigned int line ) {
	struct uri *dup;

	dup = uri_dup ( uri );
	okx ( dup != NULL, file, line );
	if ( dup )
		uri_okx ( dup, uri, file, line );
	uri_put ( dup );
}
#define uri_dup_ok( test ) uri_dup_okx ( test, __FILE__, __LINE__ )

/**
 * Report URI combined parsing and formatting test result
 *
 * @v test		URI test
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_parse_format_dup_okx ( struct uri_test *test, const char *file,
				       unsigned int line ) {

	uri_parse_okx ( test, file, line );
	uri_format_okx ( test, file, line );
	uri_dup_okx ( &test->uri, file, line );
}
#define uri_parse_format_dup_ok( test ) \
	uri_parse_format_dup_okx ( test, __FILE__, __LINE__ )

/**
 * Report URI port number test result
 *
 * @v test		URI port number test
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_port_okx ( struct uri_port_test *test, const char *file,
			   unsigned int line ) {
	struct uri *uri;
	unsigned int port;

	/* Parse URI */
	uri = parse_uri ( test->string );
	okx ( uri != NULL, file, line );
	if ( uri ) {
		port = uri_port ( uri, test->default_port );
		okx ( port == test->port, file, line );
	}
	uri_put ( uri );
}
#define uri_port_ok( test ) uri_port_okx ( test, __FILE__, __LINE__ )

/**
 * Report URI resolution test result
 *
 * @v test		Path resolution test
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_resolve_okx ( struct uri_resolve_test *test,
			      const char *file, unsigned int line ) {
	struct uri *base;
	struct uri *relative;
	struct uri *resolved = NULL;
	char *formatted;

	/* Parse URIs */
	base = parse_uri ( test->base );
	okx ( base != NULL, file, line );
	relative = parse_uri ( test->relative );
	okx ( relative != NULL, file, line );

	/* Resolve URI  */
	if ( base && relative ) {
		resolved = resolve_uri ( base, relative );
		okx ( resolved != NULL, file, line );
	}

	/* Format resolved URI */
	formatted = format_uri_alloc ( resolved );
	okx ( formatted != NULL, file, line );

	/* Check resolved URI */
	if ( formatted )
		okx ( strcmp ( formatted, test->resolved ) == 0, file, line );

	free ( formatted );
	uri_put ( resolved );
	uri_put ( relative );
	uri_put ( base );
}
#define uri_resolve_ok( test ) uri_resolve_okx ( test, __FILE__, __LINE__ )

/**
 * Report path resolution test result
 *
 * @v test		Path resolution test
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_resolve_path_okx ( struct uri_resolve_test *test,
				   const char *file, unsigned int line ) {
	char *resolved;

	/* Resolve paths using resolve_path() directly */
	resolved = resolve_path ( test->base, test->relative );
	okx ( resolved != NULL, file, line );
	if ( resolved )
		okx ( strcmp ( resolved, test->resolved ) == 0, file, line );
	free ( resolved );

	/* Resolve paths as URIs (since all paths are valid URIs) */
	uri_resolve_okx ( test, file, line );
}
#define uri_resolve_path_ok( test ) \
	uri_resolve_path_okx ( test, __FILE__, __LINE__ )

/**
 * Report URI PXE test result
 *
 * @v test		URI PXE test
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_pxe_okx ( struct uri_pxe_test *test, const char *file,
			  unsigned int line ) {
	char buf[ strlen ( test->string ) + 1 /* NUL */ ];
	struct uri *uri;
	size_t len;

	/* Construct URI */
	uri = pxe_uri ( &test->server.sa, test->filename );
	okx ( uri != NULL, file, line );
	if ( uri ) {
		uri_okx ( uri, &test->uri, file, line );
		len = format_uri ( uri, buf, sizeof ( buf ) );
		okx ( len == ( sizeof ( buf ) - 1 /* NUL */ ), file, line );
		okx ( strcmp ( buf, test->string ) == 0, file, line );
	}
	uri_put ( uri );
}
#define uri_pxe_ok( test ) uri_pxe_okx ( test, __FILE__, __LINE__ )

/**
 * Report current working URI test result
 *
 * @v tests		List of current working URI tests
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_churi_okx ( struct uri_churi_test *test, const char *file,
			    unsigned int line ) {
	struct uri *old_cwuri;
	struct uri *uri;
	char *formatted;

	/* Preserve original current working URI */
	old_cwuri = uri_get ( cwuri );

	/* Perform sequence of current working URI changes */
	do {
		/* Parse relative URI */
		uri = parse_uri ( test->relative );
		okx ( uri != NULL, file, line );

		/* Move to this URI */
		churi ( uri );

		/* Format new current working URI */
		formatted = format_uri_alloc ( cwuri );
		okx ( formatted != NULL, file, line );
		if ( formatted ) {
			okx ( strcmp ( formatted, test->expected ) == 0,
			      file, line );
		}

		/* Free temporary storage */
		free ( formatted );
		uri_put ( uri );

		/* Move to next current working URI test */
		test++;

	} while ( test->relative != NULL );

	/* Restore original current working URI */
	churi ( old_cwuri );
	uri_put ( old_cwuri );
}
#define uri_churi_ok( test ) uri_churi_okx ( test, __FILE__, __LINE__ )

/**
 * Report request parameter URI test list result
 *
 * @v test		Request parameter URI test
 * @v uri		URI
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_params_list_okx ( struct uri_params_test *test,
				  struct uri *uri, const char *file,
				  unsigned int line ) {
	struct uri_params_test_list *list;
	struct parameter *param;

	/* Check URI */
	uri_okx ( uri, &test->uri, file, line );

	/* Check URI parameters */
	okx ( uri->params != NULL, file, line );
	if ( uri->params ) {
		list = test->list;
		for_each_param ( param, uri->params ) {
			okx ( strcmp ( param->key, list->key ) == 0,
			      file, line );
			okx ( strcmp ( param->value, list->value ) == 0,
			      file, line );
			okx ( param->flags == list->flags, file, line );
			list++;
		}
		okx ( list->key == NULL, file, line );
	}
}
#define uri_params_list_ok( test ) \
	uri_params_list_okx ( test, __FILE__, __LINE__ )

/**
 * Report request parameter URI test result
 *
 * @v test		Request parameter URI test
 * @v file		Test code file
 * @v line		Test code line
 */
static void uri_params_okx ( struct uri_params_test *test, const char *file,
			     unsigned int line ) {
	struct uri_params_test_list *list;
	struct parameters *params;
	struct parameter *param;
	struct uri *uri;
	struct uri *dup;

	/* Create parameter list */
	params = create_parameters ( test->name );
	okx ( params != NULL, file, line );
	if ( params ) {
		for ( list = test->list ; list->key ; list++ ) {
			param = add_parameter ( params, list->key, list->value,
						list->flags );
			okx ( param != NULL, file, line );
		}
	}

	/* Record parameter list as part of expected URI */
	test->uri.params = params;

	/* Parse URI */
	uri = parse_uri ( test->string );
	okx ( uri != NULL, file, line );
	if ( uri )
		uri_params_list_okx ( test, uri, file, line );

	/* Duplicate URI */
	dup = uri_dup ( uri );
	okx ( dup != NULL, file, line );
	if ( dup )
		uri_params_list_okx ( test, dup, file, line );

	/* Clear parameter list in expected URI */
	test->uri.params = NULL;

	uri_put ( uri );
	uri_put ( dup );
}
#define uri_params_ok( test ) uri_params_okx ( test, __FILE__, __LINE__ )

/** Empty URI */
static struct uri_test uri_empty = {
	.string = "",
};

/** Basic HTTP URI */
static struct uri_test uri_boot_ipxe_org = {
	"http://boot.ipxe.org/demo/boot.php",
	{ .scheme = "http", .host = "boot.ipxe.org",
	  .path = "/demo/boot.php", .epath = "/demo/boot.php" },
};

/** Basic opaque URI */
static struct uri_test uri_mailto = {
	"mailto:ipxe-devel@lists.ipxe.org",
	{ .scheme = "mailto", .opaque = "ipxe-devel@lists.ipxe.org" },
};

/** Basic host-only URI */
static struct uri_test uri_host = {
	"http://boot.ipxe.org",
	{ .scheme = "http", .host = "boot.ipxe.org" },
};

/** Basic path-only URI */
static struct uri_test uri_path = {
	"/var/lib/tftpboot/pxelinux.0",
	{ .path = "/var/lib/tftpboot/pxelinux.0",
	  .epath ="/var/lib/tftpboot/pxelinux.0" },
};

/** Path-only URI with escaped characters */
static struct uri_test uri_path_escaped = {
	"/hello%20world%3F",
	{ .path = "/hello world?", .epath = "/hello%20world%3F" },
};

/** HTTP URI with all the trimmings */
static struct uri_test uri_http_all = {
	"http://anon:password@example.com:3001/~foo/cgi-bin/foo.pl?a=b&c=d#bit",
	{
		.scheme = "http",
		.user = "anon",
		.password = "password",
		.host = "example.com",
		.port = "3001",
		.path = "/~foo/cgi-bin/foo.pl",
		.epath = "/~foo/cgi-bin/foo.pl",
		.equery = "a=b&c=d",
		.efragment = "bit",
	},
};

/** HTTP URI with escaped characters */
static struct uri_test uri_http_escaped = {
	"https://test.ipxe.org/wtf%3F%0A?kind%23of/uri%20is#this%3F",
	{
		.scheme = "https",
		.host = "test.ipxe.org",
		.path = "/wtf?\n",
		.epath = "/wtf%3F%0A",
		.equery = "kind%23of/uri%20is",
		.efragment = "this%3F",
	},
};

/** HTTP URI with improperly escaped characters */
static struct uri_test uri_http_escaped_improper = {
	/* We accept for parsing improperly escaped characters.
	 * (Formatting the parsed URI would produce the properly
	 * encoded form, and so would not exactly match the original
	 * URI string.)
	 */
	"https://test%2eipxe.org/wt%66%3f\n?kind%23of/uri is#this?",
	{
		.scheme = "https",
		.host = "test.ipxe.org",
		.path = "/wtf?\n",
		.epath = "/wt%66%3f\n",
		.equery = "kind%23of/uri is",
		.efragment = "this?",
	},
};

/** IPv6 URI */
static struct uri_test uri_ipv6 = {
	"http://[2001:ba8:0:1d4::6950:5845]/",
	{
		.scheme = "http",
		.host = "[2001:ba8:0:1d4::6950:5845]",
		.path = "/",
		.epath = "/",
	},
};

/** IPv6 URI with port */
static struct uri_test uri_ipv6_port = {
	"http://[2001:ba8:0:1d4::6950:5845]:8001/boot",
	{
		.scheme = "http",
		.host = "[2001:ba8:0:1d4::6950:5845]",
		.port = "8001",
		.path = "/boot",
		.epath = "/boot",
	},
};

/** IPv6 URI with link-local address */
static struct uri_test uri_ipv6_local = {
	"http://[fe80::69ff:fe50:5845%25net0]/ipxe",
	{
		.scheme = "http",
		.host = "[fe80::69ff:fe50:5845%net0]",
		.path = "/ipxe",
		.epath = "/ipxe",
	},
};

/** IPv6 URI with link-local address not conforming to RFC 6874 */
static struct uri_test uri_ipv6_local_non_conforming = {
	/* We accept for parsing a single "%" in "%net0" (rather than
	 * the properly encoded form "%25net0").  (Formatting the
	 * parsed URI would produce the properly encoded form, and so
	 * would not exactly match the original URI string.)
	 */
	"http://[fe80::69ff:fe50:5845%net0]/ipxe",
	{
		.scheme = "http",
		.host = "[fe80::69ff:fe50:5845%net0]",
		.path = "/ipxe",
		.epath = "/ipxe",
	},
};

/** iSCSI URI */
static struct uri_test uri_iscsi = {
	"iscsi:10.253.253.1::::iqn.2010-04.org.ipxe:rabbit",
	{
		.scheme = "iscsi",
		.opaque = "10.253.253.1::::iqn.2010-04.org.ipxe:rabbit",
	},
};

/** File URI with relative (opaque) path */
static struct uri_test uri_file_relative = {
	"file:boot/script.ipxe",
	{
		.scheme = "file",
		.opaque = "boot/script.ipxe",
	},
};

/** File URI with absolute path */
static struct uri_test uri_file_absolute = {
	"file:/boot/script.ipxe",
	{
		.scheme = "file",
		.path = "/boot/script.ipxe",
		.epath = "/boot/script.ipxe",
	},
};

/** File URI with volume name */
static struct uri_test uri_file_volume = {
	"file://hpilo/boot/script.ipxe",
	{
		.scheme = "file",
		.host = "hpilo",
		.path = "/boot/script.ipxe",
		.epath = "/boot/script.ipxe",
	},
};

/** Relative URI with colons in path */
static struct uri_test uri_colons = {
	"/boot/52:54:00:12:34:56/boot.ipxe",
	{
		.path = "/boot/52:54:00:12:34:56/boot.ipxe",
		.epath = "/boot/52:54:00:12:34:56/boot.ipxe",
	},
};

/** URI with port number */
static struct uri_port_test uri_explicit_port = {
	"http://192.168.0.1:8080/boot.php",
	80,
	8080,
};

/** URI without port number */
static struct uri_port_test uri_default_port = {
	"http://192.168.0.1/boot.php",
	80,
	80,
};

/** Simple path resolution test */
static struct uri_resolve_test uri_simple_path = {
	"/etc/passwd",
	"group",
	"/etc/group",
};

/** Path resolution test with "." and ".." elements */
static struct uri_resolve_test uri_relative_path = {
	"/var/lib/tftpboot/pxe/pxelinux.0",
	"./../ipxe/undionly.kpxe",
	"/var/lib/tftpboot/ipxe/undionly.kpxe",
};

/** Path resolution test terminating with directory */
static struct uri_resolve_test uri_directory_path = {
	"/test/cgi-bin.pl/boot.ipxe",
	"..",
	"/test/",
};

/** Path resolution test with excessive ".." elements */
static struct uri_resolve_test uri_excessive_path = {
	"/var/lib/tftpboot/ipxe.pxe",
	"../../../../../../../foo",
	"/foo",
};

/** Path resolution test with absolute path */
static struct uri_resolve_test uri_absolute_path = {
	"/var/lib/tftpboot",
	"/etc/hostname",
	"/etc/hostname",
};

/** Relative URI resolution test */
static struct uri_resolve_test uri_relative = {
	"http://boot.ipxe.org/demo/boot.php?vendor=10ec&device=8139",
	"initrd.img",
	"http://boot.ipxe.org/demo/initrd.img",
};

/** Absolute URI resolution test */
static struct uri_resolve_test uri_absolute = {
	"http://boot.ipxe.org/demo/boot.php",
	"ftp://192.168.0.1/boot.ipxe",
	"ftp://192.168.0.1/boot.ipxe",
};

/** Absolute path URI resolution test */
static struct uri_resolve_test uri_absolute_uri_path = {
	"http://boot.ipxe.org/demo/boot.php#test",
	"/demo/vmlinuz",
	"http://boot.ipxe.org/demo/vmlinuz",
};

/** Query URI resolution test */
static struct uri_resolve_test uri_query = {
	"http://10.253.253.1/test.pl?mac=02-00-69-50-58-45",
	"?mac=00-1f-16-bc-fe-2f",
	"http://10.253.253.1/test.pl?mac=00-1f-16-bc-fe-2f",
};

/** Fragment URI resolution test */
static struct uri_resolve_test uri_fragment = {
	"http://192.168.0.254/test#foo",
	"#bar",
	"http://192.168.0.254/test#bar",
};

/** Empty relative URI resolution test */
static struct uri_resolve_test uri_self = {
	"http://192.168.0.1/path/to/me",
	"",
	"http://192.168.0.1/path/to/me",
};

/** Current directory URI resolution test */
static struct uri_resolve_test uri_cwd = {
	"http://192.168.0.1/path/to/me",
	".",
	"http://192.168.0.1/path/to/",
};

/** PXE URI with absolute URI */
static struct uri_pxe_test uri_pxe_absolute = {
	{
		/* 192.168.0.3 */
		.sin = {
			.sin_family = AF_INET,
			.sin_addr = { .s_addr = htonl ( 0xc0a80003 ) },
		},
	},
	"http://not.a.tftp/uri",
	{
		.scheme = "http",
		.host = "not.a.tftp",
		.path = "/uri",
		.epath = "/uri",
	},
	"http://not.a.tftp/uri",
};

/** PXE URI with absolute path */
static struct uri_pxe_test uri_pxe_absolute_path = {
	{
		/* 192.168.0.2 */
		.sin = {
			.sin_family = AF_INET,
			.sin_addr = { .s_addr = htonl ( 0xc0a80002 ) },
		},
	},
	"/absolute/path",
	{
		.scheme = "tftp",
		.host = "192.168.0.2",
		.path = "//absolute/path",
		.epath = "//absolute/path",
	},
	"tftp://192.168.0.2//absolute/path",
};

/** PXE URI with relative path */
static struct uri_pxe_test uri_pxe_relative_path = {
	{
		/* 192.168.0.3 */
		.sin = {
			.sin_family = AF_INET,
			.sin_addr = { .s_addr = htonl ( 0xc0a80003 ) },
		},
	},
	"relative/path",
	{
		.scheme = "tftp",
		.host = "192.168.0.3",
		.path = "/relative/path",
		.epath = "/relative/path",
	},
	"tftp://192.168.0.3/relative/path",
};

/** PXE URI with path containing special characters */
static struct uri_pxe_test uri_pxe_icky = {
	{
		/* 10.0.0.6 */
		.sin = {
			.sin_family = AF_INET,
			.sin_addr = { .s_addr = htonl ( 0x0a000006 ) },
		},
	},
	"C:\\tftpboot\\icky#path",
	{
		.scheme = "tftp",
		.host = "10.0.0.6",
		.path = "/C:\\tftpboot\\icky#path",
		.epath = "/C:\\tftpboot\\icky#path",
	},
	"tftp://10.0.0.6/C:\\tftpboot\\icky#path",
};

/** PXE URI with custom port */
static struct uri_pxe_test uri_pxe_port = {
	{
		/* 192.168.0.1:4069 */
		.sin = {
			.sin_family = AF_INET,
			.sin_addr = { .s_addr = htonl ( 0xc0a80001 ) },
			.sin_port = htons ( 4069 ),
		},
	},
	"/another/path",
	{
		.scheme = "tftp",
		.host = "192.168.0.1",
		.port = "4069",
		.path = "//another/path",
		.epath = "//another/path",
	},
	"tftp://192.168.0.1:4069//another/path",
};

/** Current working URI test */
static struct uri_churi_test uri_churi[] = {
	{
		"http://boot.ipxe.org/demo/boot.php",
		"http://boot.ipxe.org/demo/boot.php",
	},
	{
		"?vendor=10ec&device=8139",
		"http://boot.ipxe.org/demo/boot.php?vendor=10ec&device=8139",
	},
	{
		"fedora/fedora.ipxe",
		"http://boot.ipxe.org/demo/fedora/fedora.ipxe",
	},
	{
		"vmlinuz",
		"http://boot.ipxe.org/demo/fedora/vmlinuz",
	},
	{
		"http://local/boot/initrd.img",
		"http://local/boot/initrd.img",
	},
	{
		"modules/8139too.ko",
		"http://local/boot/modules/8139too.ko",
	},
	{
		NULL,
		NULL,
	}
};

/** Request parameter URI test list */
static struct uri_params_test_list uri_params_list[] = {
	{
		"vendor",
		"10ec",
		PARAMETER_FORM,
	},
	{
		"device",
		"8139",
		PARAMETER_FORM,
	},
	{
		"uuid",
		"f59fac00-758f-498f-9fe5-87d790045d94",
		PARAMETER_HEADER,
	},
	{
		NULL,
		NULL,
		0,
	}
};

/** Request parameter URI test */
static struct uri_params_test uri_params = {
	"http://boot.ipxe.org/demo/boot.php##params",
	{
		.scheme = "http",
		.host = "boot.ipxe.org",
		.path = "/demo/boot.php",
		.epath = "/demo/boot.php",
	},
	NULL,
	uri_params_list,
};

/** Named request parameter URI test list */
static struct uri_params_test_list uri_named_params_list[] = {
	{
		"mac",
		"00:1e:65:80:d3:b6",
		PARAMETER_FORM,
	},
	{
		"serial",
		"LXTQ20Z1139322762F2000",
		PARAMETER_FORM,
	},
	{
		NULL,
		NULL,
		0,
	}
};

/** Named request parameter URI test */
static struct uri_params_test uri_named_params = {
	"http://192.168.100.4:3001/register##params=foo",
	{
		.scheme = "http",
		.host = "192.168.100.4",
		.port = "3001",
		.path = "/register",
		.epath = "/register",
	},
	"foo",
	uri_named_params_list,
};

/**
 * Perform URI self-test
 *
 */
static void uri_test_exec ( void ) {

	/* URI parsing, formatting, and duplication tests */
	uri_parse_format_dup_ok ( &uri_empty );
	uri_parse_format_dup_ok ( &uri_boot_ipxe_org );
	uri_parse_format_dup_ok ( &uri_mailto );
	uri_parse_format_dup_ok ( &uri_host );
	uri_parse_format_dup_ok ( &uri_path );
	uri_parse_format_dup_ok ( &uri_path_escaped );
	uri_parse_format_dup_ok ( &uri_http_all );
	uri_parse_format_dup_ok ( &uri_http_escaped );
	uri_parse_ok ( &uri_http_escaped_improper ); /* Parse only */
	uri_parse_format_dup_ok ( &uri_ipv6 );
	uri_parse_format_dup_ok ( &uri_ipv6_port );
	uri_parse_format_dup_ok ( &uri_ipv6_local );
	uri_parse_ok ( &uri_ipv6_local_non_conforming ); /* Parse only */
	uri_parse_format_dup_ok ( &uri_iscsi );
	uri_parse_format_dup_ok ( &uri_file_relative );
	uri_parse_format_dup_ok ( &uri_file_absolute );
	uri_parse_format_dup_ok ( &uri_file_volume );
	uri_parse_format_dup_ok ( &uri_colons );

	/** URI port number tests */
	uri_port_ok ( &uri_explicit_port );
	uri_port_ok ( &uri_default_port );

	/** Path resolution tests */
	uri_resolve_path_ok ( &uri_simple_path );
	uri_resolve_path_ok ( &uri_relative_path );
	uri_resolve_path_ok ( &uri_directory_path );
	uri_resolve_path_ok ( &uri_excessive_path );
	uri_resolve_path_ok ( &uri_absolute_path );

	/** URI resolution tests */
	uri_resolve_ok ( &uri_relative );
	uri_resolve_ok ( &uri_absolute );
	uri_resolve_ok ( &uri_absolute_uri_path );
	uri_resolve_ok ( &uri_query );
	uri_resolve_ok ( &uri_fragment );
	uri_resolve_ok ( &uri_self );
	uri_resolve_ok ( &uri_cwd );

	/* PXE URI construction tests */
	uri_pxe_ok ( &uri_pxe_absolute );
	uri_pxe_ok ( &uri_pxe_absolute_path );
	uri_pxe_ok ( &uri_pxe_relative_path );
	uri_pxe_ok ( &uri_pxe_icky );
	uri_pxe_ok ( &uri_pxe_port );

	/* Current working URI tests */
	uri_churi_ok ( uri_churi );

	/* Request parameter URI tests */
	uri_params_ok ( &uri_params );
	uri_params_ok ( &uri_named_params );
}

/** URI self-test */
struct self_test uri_test __self_test = {
	.name = "uri",
	.exec = uri_test_exec,
};
