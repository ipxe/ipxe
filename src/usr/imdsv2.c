#include <errno.h>
#include <ipxe/image.h>
#include <ipxe/malloc.h>
#include <ipxe/uri.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usr/imdsv2.h>
#include <usr/imgmgmt.h>
#include <usr/userdata.h>

/**
 * Concatenates two URL parts, handling potential slash issues.
 *
 * @v base_url      The base URL string.
 * @v path          The path URL string to append.
 * @v url			Pointer to a char pointer that will receive the allocated concatenated URL string
 * @ret rc			Return status code
 */
int url_concat ( const char *base_url, const char *path, char **url ) {
	if ( ! base_url || ! path ) {
		return -EINVAL;
	}

	size_t base_len = strlen ( base_url );
	size_t path_len = strlen ( path );
	/* total_len has + 2 for the null terminator plus a potential '/' */
	size_t total_len = base_len + path_len + 2;

	char *result = malloc ( total_len );
	if ( ! result ) {
		return -ENOMEM;
	}

	strcpy ( result, base_url );

	bool base_ends_slash = base_len > 0 && base_url[base_len - 1] == '/';
	bool path_starts_slash = path_len > 0 && path[0] == '/';

	if ( path_len > 0 ) {
		if ( ! base_ends_slash && ! path_starts_slash ) {
			/* Add a '/' inbetween the base url and the path */
			strcat ( result, "/" );
		} else if ( base_ends_slash && path_starts_slash ) {
			/* Remove a '/' fromn the base url */
			result[base_len - 1] = '\0';
		}
		strcat ( result, path );
	}

	*url = result;

	return 0;
}

/**
 * Parses a specific credential value from an IMDSv2 credentials response.
 *
 * This function extracts the value associated with a given key from a JSON-formatted
 * credentials response obtained from IMDSv2. It is NOT a general-purpose JSON parser.
 *
 * @v credentials	The JSON-formatted credentials response string.
 * @v key			The key for the credential value to extract.
 * @v parsed_val	Pointer to the extracted value (newly allocated). Unmodified on error.
 * @ret rc			Return status code
 *
 * The caller is responsible for freeing the memory allocated for the returned string.
 */
int parse_imdsv2_credentials_response ( char *credentials, char *key, char **parsed_val ) {
	/* +3 for quotes and null terminator */
	char key_with_quotes[strlen ( key ) + 3];
	snprintf ( key_with_quotes, sizeof ( key_with_quotes ), "\"%s\"", key );

	/* Find the key */
	const char *key_start = strstr ( credentials, key_with_quotes );
	if ( ! key_start ) {
		return -1;
	}

	/* Find the colon, skipping whitespace */
	const char *colon = strchr ( key_start + strlen ( key_with_quotes ), ':' );
	if ( ! colon ) {
		return -1;
	}

	/* Find the opening quote, skipping whitespace */
	const char *quote = strchr ( ++colon, '"' );
	if ( ! quote ) {
		return -1;
	}

	/* Value starts immediately after the opening quote */
	const char *value_start = ++quote;

	const char *value_end = strchr ( value_start, '"' );

	const size_t value_length = value_end - value_start;
	const size_t value_buffer_size = value_length + 1;

	char *value = ( char * ) malloc ( value_buffer_size );
	if ( ! value ) {
		return -ENOMEM;
	}

	/* Fill the memory location with /0's */
	memset ( value, 0, sizeof ( char ) * value_buffer_size );

	strncpy ( value, value_start, value_length );

	*parsed_val = value;

	return 0;
}

/**
 * Copy image data to a buffer
 *
 * @v image		Image to read
 * @v buffer 	Buffer to fill in
 * @ret rc      Return status code
 */
int get_image_data ( struct image *image, char **buffer ) {
	size_t offset = 0;

	/* Allocate a buffer to hold the data */
	*buffer = malloc ( image->len + 1 );
	if ( ! *buffer ) {
		return -ENOMEM;
	}

	/* Copy data from userptr_t to our local buffer */
	memcpy ( *buffer, ( image->data + offset ), image->len );

	/* Null terminate the buffer */
	( *buffer )[image->len] = '\0';

	return 0;
}

/**
 * Get IMDSv2 session token
 *
 * @v token           Pointer to store the token string
 * @v base_url   The AWS IMDS ipv4 or ipv6 base url
 * @ret rc            Return status code
 */
int get_imdsv2_token ( char **token, const char *base_url ) {
	char *uri_string = NULL;
	struct uri *uri = NULL;
	;
	struct image *image = NULL;
	int rc;

	/* Build IMDSv2 api token URI */
	rc = url_concat ( base_url, "api/token", &uri_string );
	if ( rc != 0 ) {
		goto err_url_concat;
	}

	/* Initialize token to NULL */
	*token = NULL;

	/* Parse the URI string */
	uri = parse_uri ( uri_string );
	if ( ! uri ) {
		rc = -ENOMEM;
		goto err_uri;
	}

	/* Set the HTTP method */
	uri->method = &http_put;
	/* Set aws token ttl */
	uri->aws_token_ttl = AWS_TOKEN_TTL;

	/* Get token and store it in an image */
	rc = imgdownload ( uri, 0, &image );
	if ( rc != 0 )
		goto err_token_download;

	/* Get the image data as string */
	rc = get_image_data ( image, token );
	if ( rc != 0 )
		goto err_token_read;

	free ( uri );
	free ( image );
	free ( uri_string );
	return 0;
err_token_read:
	image_put ( image );
err_token_download:
	uri_put ( uri );
err_uri:
err_url_concat:
	free ( uri_string );
	return rc;
}

/**
 * Get metadata associated with an EC2 Instance using IMDSv2.
 *
 * @v token         The AWS IMDSv2 session token to include in the request header.
 * @v base_url   	The AWS IMDS ipv4 or ipv6 base url
 * @v metadata_path The specific metadata path to retrieve (e.g., "instance-id").
 * @v response      A pointer to a character pointer that will store the retrieved metadata string.
 */
int get_imdsv2_metadata ( char *token, const char *base_url, char *metadata_path, char **response ) {
	char *uri_string = NULL;
	struct uri *uri = NULL;
	struct image *image = NULL;
	int rc;

	/* Build IMDSv2 metadata URI */
	rc = url_concat ( base_url, metadata_path, &uri_string );
	if ( rc != 0 ) {
		goto err_url_concat;
	}

	/* Initialize reponse to NULL */
	*response = NULL;

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

	/* Get response and store it in an image */
	rc = imgdownload ( uri, 0, &image );
	if ( rc != 0 )
		goto err_metadata_download;

	/* Get the image data as string */
	rc = get_image_data ( image, response );
	if ( rc != 0 )
		goto err_metadata_read;

	free ( uri );
	free ( image );
	free ( uri_string );
	return 0;
err_metadata_read:
	image_put ( image );
err_metadata_download:
	uri_put ( uri );
err_uri:
err_url_concat:
	free ( uri_string );
	return rc;
}

/**
 * Sets the appropriate IMDS base URL based on IP version preference.
 *
 * @v use_ipv6   Boolean flag to determine whether to use IPv6 (true) or IPv4 (false)
 * @v base_url   Pointer to a char pointer that will store the selected base URL
 *              Will be set to either IMDSV2_IPV6_METADATA_BASE_URL or IMDSV2_IPV4_METADATA_BASE_URL
 *
 * @ret rc      Return status code
 */
int get_imds_metadata_base_url ( int use_ipv6, const char **base_url ) {
	if ( use_ipv6 ) {
		*base_url = IMDSV2_IPV6_METADATA_BASE_URL;
	} else {
		*base_url = IMDSV2_IPV4_METADATA_BASE_URL;
	}
	return 0;
}