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

/** Maximum length allowed for the iPXE script */
/** User data has a limit of 16K characters */
/** Use a slightly larger number as the threshold */
#define MAX_IPXE_SCRIPT_LEN 20000
/** Maximum length allowed for the boundary string in the MIME multi-part format */
#define MAX_MULTIPART_BOUNDARY_LEN 500
/** String constants in the user data */
#define MIME_HEADER_CONTENT_TYPE_MULTIPART "Content-Type: multipart/mixed"
#define MIME_HEADER_CONTENT_TYPE_IPXE      "Content-Type: text/ipxe"
#define IPXE_SHEBANG                       "#!ipxe\n"

/**
 * Gets the substring of the specified source string and store the result in the specified destination. Memory for the destination string must be allocated before calling this function.
 *
 * @v source		The source string
 * @v destination	The destination string
 * @v start_index	Starting index of the substring
 * @v length		Length of the substring
 * @rt rc			Return status code
 */
int get_substring ( const char *source, char *destination, int start_index, int length ) {
	if ( ! source || ! destination ) {
		DBG ( "Memory for the source and the destination must be allocated before calling this function\n" );
		return -ENOMEM;
	}

	memcpy ( destination, source + start_index, length * sizeof ( char ) );
	destination[length] = '\0'; // Null-terminate the destination string
	return 0;
}

/**
 * Split a buffer into parts based on a boundary.
 *
 * @v buffer	Buffer to split
 * @v parts		Pointer to store the array of parts, each of which is a pointer to a string
 * @v boundary	Boundary string
 * @ret rc		Return status code
 */
int split_parts ( char *buffer, char ***parts, char *boundary ) {
	int rc = 0;
	char *part_start = NULL, *part_end = NULL;

	/* Create a boundary string with the -- prefix */
	size_t boundary_length = strnlen ( boundary, MAX_MULTIPART_BOUNDARY_LEN );
	char *boundary_with_prefix = malloc ( ( boundary_length + 3 ) * sizeof ( char ) ); /* 2 for -- and 1 for null terminator */
	if ( ! boundary_with_prefix ) {
		rc = -ENOMEM;
		goto err_script;
	}
	sprintf ( boundary_with_prefix, "--%s", boundary );

	/* Count the number of parts */
	int part_count = -1; /* Start at -1 to account for the preamble (content before the first boundary) */
	part_start = buffer;
	part_end = strstr ( part_start, boundary_with_prefix );
	while ( part_end ) {
		part_count++;
		part_start = part_end + strnlen ( boundary_with_prefix, MAX_MULTIPART_BOUNDARY_LEN );
		part_end = strstr ( part_start, boundary_with_prefix );
	}

	if ( part_count < 1 ) {
		printf ( "Malformed MIME multi-part data: no parts found\n" );
		rc = -ENOEXEC;
		goto err_script;
	}

	/* Allocate memory for the parts array */
	*parts = malloc ( ( part_count + 1 ) * sizeof ( char * ) );
	if ( ! *parts ) {
		return -ENOMEM;
	}
	( *parts )[part_count] = NULL;

	/* Split the buffer into parts */
	part_end = strstr ( buffer, boundary_with_prefix ); /* The start of the first boundary */
	for ( int i = 0; i < part_count; i++ ) {
		part_start = part_end + strnlen ( boundary_with_prefix, MAX_MULTIPART_BOUNDARY_LEN );
		part_end = strstr ( part_start, boundary_with_prefix );
		( *parts )[i] = malloc ( ( part_end - part_start + 1 ) * sizeof ( char ) );
		if ( ! ( *parts )[i] ) {
			rc = -ENOMEM;
			goto err_script;
		}
		rc = get_substring ( part_start, ( *parts )[i], 0, part_end - part_start );
		if ( rc ) {
			goto err_script;
		}
	}

err_script:
	free ( boundary_with_prefix );
	return rc;
}

/** Extract the MIME multi-part boundary string.
 *
 * @v input_string	The string in MIME-multi-part format
 * @v boundary		Pointer to store the boundary string
 * @ret rc			Return status code
 */
int get_multipart_boundary ( char *input_string, char **boundary ) {
	char *boundary_start = NULL, *boundary_end = NULL;

	/* Look for the first equal sign after content type */
	boundary_start = strchr ( input_string, '=' );
	if ( ! boundary_start ) {
		printf ( "Malformed MIME multi-part data: no boundary found\n" );
		return -ENOEXEC;
	}
	boundary_start++; /* Skip the '=' character */

	/* Check for optional double quotation marks around the boundary */
	if ( *boundary_start == '"' ) {
		boundary_start++; /* Skip the opening double quote */
		boundary_end = strchr ( boundary_start, '"' );
	} else {
		/* Look for the first newline character after the boundary */
		boundary_end = strchr ( boundary_start, '\n' );
	}

	if ( ! boundary_end ) {
		printf ( "Malformed MIME multi-part data: no closing double quote found\n" );
		return -ENOEXEC;
	}

	/* Extract the boundary string */
	size_t boundary_length = boundary_end - boundary_start;
	*boundary = malloc ( boundary_length + 1 * sizeof ( char ) ); /* 1 for null terminator */
	if ( ! *boundary ) {
		return -ENOMEM;
	}
	return get_substring ( boundary_start, *boundary, 0, boundary_length );
}

/**
 * Parse user data to extract parts.
 *
 * The result stored in parts are copied from the original buffer.
 *
 * @v buffer	User data buffer
 * @v parts		Pointer to store the array of parts, each of which is a pointer to a string
 * @ret rc		Return status code
 */
int get_parts ( char *buffer, char ***parts ) {
	int rc = 0;

	char *boundary = NULL;

	/* Check if this is a MIME multi-part by looking for content type */
	char *content_type_start = strstr ( buffer, MIME_HEADER_CONTENT_TYPE_MULTIPART );

	if ( content_type_start ) {
		/* Multi-part data */
		/* Get the boundary string */
		rc = get_multipart_boundary ( content_type_start, &boundary );
		if ( rc != 0 ) {
			goto err_script;
		}

		/* Split into parts */
		rc = split_parts ( buffer, parts, boundary );
	} else {
		/* Single-part data */
		*parts = malloc ( 2 * sizeof ( char * ) );
		if ( ! *parts ) {
			rc = -ENOMEM;
			goto err_script;
		}
		/* Copy the entire buffer */
		( *parts )[0] = malloc ( ( strnlen ( buffer, MAX_IPXE_SCRIPT_LEN ) + 1 ) * sizeof ( char ) );
		if ( ! ( *parts )[0] ) {
			rc = -ENOMEM;
			goto err_script;
		}
		rc = get_substring ( buffer, ( *parts )[0], 0, strnlen ( buffer, MAX_IPXE_SCRIPT_LEN ) );
		if ( rc ) {
			goto err_script;
		}

		( *parts )[1] = NULL;
	}

err_script:
	free ( boundary );
	return rc;
}

/**
 * Search for a string in a string and truncate the content before the matching string. The result starts from the beginning of the matching string to the end of the original string.
 *
 * The result is a pointer to the original string, not a copy.
 *
 * @v original_string	The original string to search in
 * @v search_string		String to search for
 * @v result			Pointer to store the result
 * @ret rc				Return status code
 */
int truncate_string_before ( char *original_string, char *search_string, char **result ) {
	/* Check if the original string is NULL */
	if ( ! original_string || ! search_string ) {
		DBG ( "The original string or the search string is NULL\n" );
		return -ENOMEM;
	}
	/* Find the position of the search string in the original string */
	char *position = strstr ( original_string, search_string );

	/* If the search string is found, update the result pointer */
	if ( position != NULL ) {
		*result = position;
		return 0; // Success
	} else {
		return -ENOEXEC; // Search string not found
	}
}

/**
 * Search for the iPXE script in an array of parts.
 *
 * @v parts		Array of parts
 * @v result	Pointer to store the result
 * @ret rc		Return status code
 */
int search_ipxe_script_in_parts ( char **parts, char **result ) {
	int part_count = 0;
	/* Count the number of parts */
	while ( parts[part_count] != NULL ) {
		part_count++;
	}

	if ( part_count < 1 ) {
		printf ( "Malformed MIME multi-part data: no parts found\n" );
		return -ENOEXEC;
	} else if ( part_count == 1 ) {
		/* Search for the search string in the only part */
		return truncate_string_before ( parts[0], IPXE_SHEBANG, result );
	} else {
		/* Loop through each part */
		for ( int i = 0; parts[i] != NULL; i++ ) {
			/* Search for the search string in the current part */
			/* Search for the content type first, then the iPXE shebang */
			if ( truncate_string_before ( parts[i], MIME_HEADER_CONTENT_TYPE_IPXE, result ) == 0 && truncate_string_before ( *result, IPXE_SHEBANG, result ) == 0 ) {
				return 0; // Search string found
			}
		}
		return -ENOEXEC; // Search string not found in any part
	}
}

/**
 * Parse user data to extract the iPXE section.
 *
 * @v image     Image to update
 * @ret rc      Return status code
 */
int extract_ipxe_script ( struct image *image ) {
	char *buffer = NULL;
	char *ipxe_script = NULL;
	int rc;

	/* Get the image data as a string */
	rc = get_image_data ( image, &buffer );
	if ( rc != 0 || ! buffer ) {
		printf ( "Could not get image data\n" );
		goto err_buffer;
	}

	/* Check the length of the buffer */
	if ( strnlen ( buffer, MAX_IPXE_SCRIPT_LEN ) >= MAX_IPXE_SCRIPT_LEN ) {
		printf ( "User data is too long\n" );
		goto err_buffer_size;
	}

	char **parts = NULL;
	rc = get_parts ( buffer, &parts );
	/* Check if parsing was successful */
	if ( rc != 0 ) {
		printf ( "Could not parse user data\n" );
		goto err_script;
	}

	/* Extract the iPXE script */
	rc = search_ipxe_script_in_parts ( parts, &ipxe_script );
	/* Check if extraction was successful */
	if ( rc != 0 ) {
		printf ( "Could not extract iPXE script\n" );
		goto err_script;
	}

	/* Clean the old data in image and update with the extracted iPXE script */
	free ( ( void * ) image->data );
	size_t script_size = strnlen ( ipxe_script, MAX_IPXE_SCRIPT_LEN ) + 1;
	memcpy ( ( void * ) image->data, ipxe_script, script_size );
	image->len = script_size;
	image->type = NULL;

	/* Register the updated image */
	rc = register_image ( image );
	if ( rc != 0 ) {
		printf ( "Could not register image\n" );
		goto err_script;
	}

err_script:
	for ( int i = 0; parts[i] != NULL; i++ ) {
		free ( parts[i] );
	}
	free ( parts );
err_buffer_size:
	free ( buffer );
err_buffer:
	return rc;
}

/**
 * Get user data and store it in an image
 *
 * @v use_ipv6  Boolean flag to determine whether to use IPv6 (true) or IPv4 (false)
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

	rc = extract_ipxe_script ( *image );
	if ( rc != 0 ) {
		printf ( "failed to get iPXE script\n" );
		goto err_userdata;
	}

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
	return image_exec ( image );
}
