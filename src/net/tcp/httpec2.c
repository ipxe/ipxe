FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * @file
 *
 * Amazon Elastic Compute Cloud (EC2) Instance Metadata Service (IMDSv2) retrieval
 *
 * IMDSv2 enhances IMDSv1 security by requiring a session token for metadata requests.
 * This token is obtained via a PUT request to the IMDS endpoint. Subsequent metadata
 * requests must include this token in the non-standard HTTP header
 * "X-aws-ec2-metadata-token". Additionally, the "X-aws-ec2-metadata-token-ttl-seconds"
 * header is required to specify the token's time-to-live.
 */

#include <ipxe/http.h>
#include <ipxe/uri.h>
#include <ipxe/vsprintf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Construct HTTP "X-aws-ec2-metadata-token-ttl-seconds" header
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int http_format_aws_token_ttl ( struct http_transaction *http,
									   char *buf, size_t len ) {
	/* Return zero length if no token ttl available */
	if ( ! http->uri->aws_token_ttl )
		return 0;

	/* Return required length if no buffer provided */
	if ( ! buf )
		return strnlen ( http->uri->aws_token_ttl, AWS_TOKEN_TTL_LEN );

	/* Format the header value */
	return ssnprintf ( buf, len, "%s", http->uri->aws_token_ttl );
}

/**
 * Construct HTTP "X-aws-ec2-metadata-token" header
 *
 * @v http		HTTP transaction
 * @v buf		Buffer
 * @v len		Length of buffer
 * @ret len		Length of header value, or negative error
 */
static int http_format_aws_token ( struct http_transaction *http,
								   char *buf, size_t len ) {
	/* Return zero length if no token available */
	if ( ! http->uri->aws_token )
		return 0;

	/* Return required length if no buffer provided */
	if ( ! buf )
		return strnlen ( http->uri->aws_token, MAX_AWS_TOKEN_LEN );

	/* Format the header value */
	return ssnprintf ( buf, len, "%s", http->uri->aws_token );
}

/** HTTP "X-aws-ec2-metadata-token-ttl-seconds" header */
struct http_request_header http_request_aws_token_ttl __http_request_header = {
	.name = "X-aws-ec2-metadata-token-ttl-seconds",
	.format = http_format_aws_token_ttl,
};

/** HTTP "X-aws-ec2-metadata-token" header */
struct http_request_header http_request_aws_token __http_request_header = {
	.name = "X-aws-ec2-metadata-token",
	.format = http_format_aws_token,
};