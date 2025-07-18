#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Unescapes a JSON-escaped string by converting known escape sequences
 * (`\\` and `\"`) to their literal characters. Unknown escape sequences
 * are preserved as-is.
 *
 * @v escaped_string     Input JSON-escaped string
 * @v unescaped_string   Pointer to receive a heap-allocated unescaped string
 * @ret rc     0 on success, negative on error
 *
 * The caller is responsible for freeing the allocated memory.
 */

static int unescape_string ( const char *escaped_string, char **unescaped_string ) {
	*unescaped_string = NULL;

	size_t escaped_len = strlen ( escaped_string );
	char *unescaped_str = calloc ( escaped_len + 1, sizeof ( char ) );
	if ( unescaped_str == NULL ) {
		return -ENOMEM;
	}

	size_t j = 0;
	for ( size_t i = 0; i < escaped_len; i++ ) {
		if ( escaped_string[i] == '\\' && i + 1 < escaped_len ) {
			char next_char = escaped_string[i + 1];
			if ( next_char == '\\' || next_char == '"' ) {
				/* Unescape: add the actual character and skip both */
				unescaped_str[j++] = next_char;
				/* Skip the next character since we processed it */
				i++;
			} else {
				/* Unknown escape: keep both characters */
				unescaped_str[j++] = escaped_string[i];
			}
		} else {
			/* Regular character or backslash at end of string */
			unescaped_str[j++] = escaped_string[i];
		}
	}

	*unescaped_string = unescaped_str;

	return 0;
}

/**
 * Extracts a string value associated with a key from a JSON-encoded string.
 *
 * This function locates a key within a flat, well-formed JSON object string, extracts
 * the string value associated with that key, and performs a single level of unescaping
 * on the value (e.g., turning `\\` into `\`, `\"` into `"`, etc).
 *
 * Limitations:
 * - Only works on flat JSON objects with simple `"key":"value"` pairs.
 * - Only supports extracting values of type string (enclosed in double quotes).
 * - Does not handle arrays, nested objects, or complex escape sequences like unicode.
 * - Escaped quote detection is naive and only handles a single backslash escape (`\"`),
 *   not sequences like `\\"`.
 *
 * @v json     JSON input string with one layer of escaping
 * @v key      Key to find
 * @v output   Pointer to receive a heap-allocated copy of the unescaped value string
 * @ret rc     0 on success, negative on error
 *
 * The caller is responsible for freeing the allocated memory.
 */
int json_extract_string ( char *json, char *key, char **output ) {
	char *search_string = NULL;
	char *escaped_value = NULL;
	char *unescaped_value = NULL;
	int rc = 0;
	*output = NULL;

	/* Build search string: quoted key */
	if ( asprintf ( &search_string, "\"%s\"", key ) < 0 ) {
		rc = -ENOMEM;
		goto cleanup;
	}

	/* Find key start */
	const char *key_start = strstr ( json, search_string );
	if ( ! key_start ) {
		rc = -ENOENT;
		goto cleanup;
	}

	/* Find the colon after the key */
	const char *colon = strchr ( key_start + strlen ( search_string ), ':' );
	if ( ! colon ) {
		rc = -EINVAL;
		goto cleanup;
	}

	/* Find the opening quote after the colon */
	const char *quote = strchr ( ++colon, '"' );
	if ( ! quote ) {
		rc = -EINVAL;
		goto cleanup;
	}

	/* The value starts one character after the opening quote */
	const char *value_start = ++quote;

	/* Find the closing quote */
	char *value_end = strchr ( value_start, '"' );
	if ( ! value_end ) {
		rc = -EINVAL;
		goto cleanup;
	}

	/* If the quote is escaped (preceded by '\'), keep searching for the real string end */
	while ( *( value_end - 1 ) == '\\' ) {
		value_end = strchr ( value_end + 1, '"' );
		if ( ! value_end ) {
			rc = -EINVAL;
			goto cleanup;
		}
	}

	size_t value_len = value_end - value_start;

	/* Allocate and copy escaped substring */
	escaped_value = calloc ( value_len + 1, sizeof ( char ) );
	if ( ! escaped_value ) {
		rc = -ENOMEM;
		goto cleanup;
	}
	memcpy ( escaped_value, value_start, value_len );

	/* Unescape the extracted value to decode escaped characters */
	rc = unescape_string ( escaped_value, &unescaped_value );
	if ( rc < 0 ) {
		goto cleanup;
	}

	*output = unescaped_value;

cleanup:
	free ( search_string );
	free ( escaped_value );
	return rc;
}
