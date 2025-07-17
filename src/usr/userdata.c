#include <errno.h>
#include <ipxe/image.h>
#include <ipxe/malloc.h>
#include <ipxe/uri.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usr/imdsv2.h>
#include <usr/imgmgmt.h>
#include <usr/userdata.h>

/**
 * Get user data and store it in an image
 *
 * @v use_ipv6   Boolean flag to determine whether to use IPv6 (true) or IPv4 (false)
 * @v image		Image to fill in
 * @ret rc		Return status code
 */
int get_userdata ( int use_ipv6, struct image **image ) {
	char *uri_string = NULL;
	struct uri *uri;
	const char *base_url;
	char *token = NULL;
	int rc;

	rc = get_imds_metadata_base_url ( use_ipv6, &base_url );
	if ( rc != 0 )
		goto err_base_url;

	/* Get IMDSv2 session token */
	rc = get_imdsv2_token ( &token, base_url );
	if ( rc != 0 )
		goto err_token;

	/* Build IMDSv2 user data URI */
	rc = url_concat ( base_url, "user-data", &uri_string );
	if ( rc != 0 ) {
		goto err_url_concat;
	}

	/* Parse the URI string */
	uri = parse_uri ( uri_string );
	if ( ! uri ) {
		rc = -ENOMEM;
		goto err_uri;
	}

	/* Set the HTTP method */
	uri->method = &http_get;
	/* Set AWS IMDSv2 token */
	uri->aws_token = token;

	/* Get user data and store it in an image */
	rc = imgdownload ( uri, 0, image );
	if ( rc != 0 )
		goto err_userdata;

	free ( uri );
	free ( uri_string );
	return 0;
err_userdata:
	uri_put ( uri );
err_uri:
err_url_concat:
	free ( uri_string );
err_token:
	free ( token );
err_base_url:
	return rc;
}

/**
 * Execute user data stored in an image
 *
 * @v image		Executable image
 * @ret rc		Return status code
 */
int execute_userdata ( struct image *image ) {
	int rc;
	if ( ( rc = image_exec ( image ) ) != 0 )
		return rc;

	return 0;
}
