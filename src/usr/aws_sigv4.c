#include <errno.h>
#include <ipxe/hmac.h>
#include <ipxe/http.h>
#include <ipxe/params.h>
#include <ipxe/parseopt.h>
#include <ipxe/settings.h>
#include <ipxe/sha256.h>
#include <ipxe/uri.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usr/aws_sigv4.h>
#include <usr/json.h>

#define AWS_ALGORITHM      "AWS4-HMAC-SHA256"
#define AWS_REQUEST_TYPE   "aws4_request"
#define AWS_CONTENT_TYPE   "application/x-amz-json-1.1"
#define AWS_SIGNED_HEADERS "content-type;host;x-amz-date;x-amz-security-token"

/**
 * Macro for handling asprintf with error checking.
 */
#define ASPRINTF_OR_GOTO_CLEANUP( ptr, fmt, ... )          \
	if ( asprintf ( &( ptr ), fmt, ##__VA_ARGS__ ) < 0 ) { \
		rc = -ENOMEM;                                      \
		goto cleanup;                                      \
	}

/**
 * Computes the SHA-256 hash of a given data block, storing the result as raw bytes.
 *
 * @v data       Pointer to the data block to be hashed.
 * @v data_len   The length of the data block in bytes.
 * @v out        Pointer to the output buffer where the 32-byte hash will be stored.
 */
static void sha256 ( const void *data, size_t data_len, uint8_t *out ) {
	struct digest_algorithm *digest = &sha256_algorithm;

	/* Allocate memory for SHA-256 context */
	uint8_t ctx[digest->ctxsize];

	digest_init ( digest, ctx );
	digest_update ( digest, ctx, data, data_len );
	digest_final ( digest, ctx, out );
}

/** Computes HMAC-SHA-256 hash of a given data block, storing the result as raw bytes.
 *
 * @v data       Pointer to the data block to be hashed.
 * @v data_len   The length of the data block in bytes.
 * @v out        Pointer to the output buffer where the 32-byte hash will be stored.
 */
static void hmac_sha256 ( const void *key, size_t key_len, const void *data, size_t data_len, uint8_t *out ) {
	struct digest_algorithm *digest = &sha256_algorithm;

	/* Allocate memory for HMAC context */
	uint8_t ctx[hmac_ctxsize ( digest )];

	hmac_init ( digest, ctx, key, key_len );
	hmac_update ( digest, ctx, data, data_len );
	hmac_final ( digest, ctx, out );
}

/**
 * Converts a byte array to its hexadecimal string representation.
 *
 * @v bytes    Pointer to the byte array to convert.
 * @v len      The length of the byte array in bytes.
 * @v out_str  Pointer to a character pointer where the newly allocated hex string
 * will be stored
 * @ret rc			Return status code
 *
 * The caller is responsible for freeing the memory allocated for the returned string.
 */
static int bytes_to_hex_string ( const uint8_t *bytes, size_t len, char **out_str ) {
	/* Initialize output parameter */
	*out_str = NULL;

	/* Each byte will be represented by 2 hexadecimal characters, plus the null terminator */
	*out_str = malloc ( ( len * 2 + 1 ) * sizeof ( char ) );
	if ( *out_str == NULL ) {
		return -ENOMEM;
	}

	for ( size_t i = 0; i < len; ++i ) {
		sprintf ( ( *out_str ) + ( i * 2 ), "%02x", bytes[i] );
	}

	/* Null terminate the string */
	( *out_str )[len * 2] = '\0';

	return 0;
}

/**
 * Computes SHA-256 hash of data and converts it to hexadecimal string.
 *
 * @v data       Pointer to the data block to be hashed.
 * @v data_len   The length of the data block in bytes.
 * @v hex_out    Pointer to store the allocated hex string
 * @ret rc       Return status code
 *
 * The caller is responsible for freeing the allocated hex string memory.
 */
static int sha256_to_hex ( const void *data, size_t data_len, char **hex_out ) {
	uint8_t raw_hash[SHA256_DIGEST_SIZE];

	sha256 ( data, data_len, raw_hash );
	return bytes_to_hex_string ( raw_hash, SHA256_DIGEST_SIZE, hex_out );
}

/**
 * Creates a JSON payload for the AWS Secrets Manager GetSecretValue API.
 * https://docs.aws.amazon.com/secretsmanager/latest/apireference/API_GetSecretValue.html
 *
 * @v secret_id   The SecretId (name or ARN) to be included in the payload
 * @v payload     Pointer to store the allocated JSON string. Will be set to
 *                NULL on error
 * @ret rc        Return status code
 *
 * The caller is responsible for freeing the allocated payload memory.
 */
int generate_get_secret_value_payload ( const char *secret_id, char **payload ) {
	int rc = 0;
	*payload = NULL;

	ASPRINTF_OR_GOTO_CLEANUP ( *payload, "{\"SecretId\": \"%s\"}", secret_id );
	return 0;

cleanup:
	return rc;
}

/**
 * Derives the signing key for AWS Signature Version 4.
 * https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_sigv-create-signed-request.html#derive-signing-key
 *
 * This function implements the steps to generate the final signing key by iteratively
 * applying HMAC-SHA256.
 *
 * @v params           Pointer to AWS SigV4 parameters
 * @v concatenated_key The result of prepending "AWS4" to the secret key.
 * @v k_signing        Pointer to a uint8_t array (of size SHA256_DIGEST_SIZE)
 * where the resulting signing key will be stored.
 * @ret rc            Return status code

 * The caller must ensure that the k_signing buffer has sufficient allocated memory.
 */
static int generate_signing_key ( const AwsSigv4Params *params, char *concatenated_key, uint8_t *k_signing ) {
	uint8_t k_date[SHA256_DIGEST_SIZE];
	uint8_t k_region[SHA256_DIGEST_SIZE];
	uint8_t k_service[SHA256_DIGEST_SIZE];

	/* k_date = hash("AWS4" + Key, Date) */
	hmac_sha256 ( concatenated_key, strlen ( concatenated_key ), params->date_stamp, strlen ( params->date_stamp ), k_date );

	/* k_region = hash(k_date, Region) */
	hmac_sha256 ( k_date, SHA256_DIGEST_SIZE, params->region, strlen ( params->region ), k_region );

	/* k_service = hash(k_region, Service) */
	hmac_sha256 ( k_region, SHA256_DIGEST_SIZE, params->service, strlen ( params->service ), k_service );

	/* k_signing = hash(k_service, "aws4_request") */
	hmac_sha256 ( k_service, SHA256_DIGEST_SIZE, AWS_REQUEST_TYPE, strlen ( AWS_REQUEST_TYPE ), k_signing );

	return 0;
}

/**
 * Generates AWS Signature Version 4 (SigV4) for API authentication.
 * https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
 *
 * This function implements the AWS SigV4 signing process in three main steps:
 * 1. Create a Canonical Request by normalizing HTTP method, URI, query params,
 *    headers, and computing a payload hash.
 * 2. Create a String To Sign containing the algorithm, date, credential scope,
 *    and canonical request hash.
 * 3. Calculate Signature by:
 *    - Deriving a signing key through a series of HMAC operations
 *    - Using the signing key to sign the string to sign
 *
 * @v params     AWS SigV4 parameters including credentials, service, region,
 *               timestamp information, and payload
 * @v sigv4      Output pointer to store the generated hex-encoded signature.
 *               The caller is responsible for freeing this memory.
 * @ret rc       Return status code
 *
 * The caller is responsible for freeing the memory allocated for the returned string.
 */
int aws_sigv4 ( const AwsSigv4Params *params, char **sigv4 ) {
	/* Initialize all pointers to NULL */
	char *payload_hash = NULL;
	char *canonical_headers = NULL;
	char *canonical_request = NULL;
	char *canonical_request_hash = NULL;
	char *credential_scope = NULL;
	char *string_to_sign = NULL;
	char *concatenated_key = NULL;
	char *sigv4_hash = NULL;
	uint8_t raw_signing_key_hash[SHA256_DIGEST_SIZE];
	uint8_t raw_sigv4_hash[SHA256_DIGEST_SIZE];
	int rc;

	/* Initialize output parameter */
	*sigv4 = NULL;

	/* Hash the payload (part of the Canonical Request) */
	if ( ( rc = sha256_to_hex ( params->payload, strlen ( params->payload ), &payload_hash ) ) != 0 )
		goto cleanup;

	/* Generate canonical headers inline */
	ASPRINTF_OR_GOTO_CLEANUP ( canonical_headers,
							   "content-type:%s\n"
							   "host:%s.%s.amazonaws.com\n"
							   "x-amz-date:%s\n"
							   "x-amz-security-token:%s\n",
							   AWS_CONTENT_TYPE, params->service, params->region,
							   params->amz_date, params->session_token );

	/* Generate canonical request inline */
	ASPRINTF_OR_GOTO_CLEANUP ( canonical_request,
							   "%s\n%s\n%s\n%s\n%s\n%s",
							   "POST", "/", "", canonical_headers,
							   AWS_SIGNED_HEADERS,
							   payload_hash );

	/* Hash the canonical request */
	if ( ( rc = sha256_to_hex ( canonical_request, strlen ( canonical_request ), &canonical_request_hash ) ) != 0 )
		goto cleanup;

	/* Generate credential scope inline */
	ASPRINTF_OR_GOTO_CLEANUP ( credential_scope, "%s/%s/%s/%s",
							   params->date_stamp, params->region, params->service, AWS_REQUEST_TYPE );

	/* Generate string to sign inline */
	ASPRINTF_OR_GOTO_CLEANUP ( string_to_sign,
							   "%s\n%s\n%s\n%s",
							   AWS_ALGORITHM, params->amz_date,
							   credential_scope, canonical_request_hash );

	/* Generate concatenated key inline */
	ASPRINTF_OR_GOTO_CLEANUP ( concatenated_key, "AWS4%s", params->secret_key );

	/* Generate signing key */
	generate_signing_key ( params, concatenated_key, raw_signing_key_hash );

	/* Generate signature */
	hmac_sha256 ( raw_signing_key_hash, SHA256_DIGEST_SIZE,
				  string_to_sign, strlen ( string_to_sign ), raw_sigv4_hash );

	if ( ( rc = bytes_to_hex_string ( raw_sigv4_hash, SHA256_DIGEST_SIZE, &sigv4_hash ) ) != 0 )
		goto cleanup;

	/* Return by reference */
	*sigv4 = sigv4_hash;
	sigv4_hash = NULL; /* Prevent double-free */

	/* Free all other allocated memory */
	free ( payload_hash );
	free ( canonical_headers );
	free ( canonical_request );
	free ( canonical_request_hash );
	free ( credential_scope );
	free ( string_to_sign );
	free ( concatenated_key );

	return 0;

cleanup:
	free ( payload_hash );
	free ( canonical_headers );
	free ( canonical_request );
	free ( canonical_request_hash );
	free ( credential_scope );
	free ( string_to_sign );
	free ( concatenated_key );
	free ( sigv4_hash );

	return rc;
}

/**
 * Constructs the Authorization header for AWS SigV4 authentication.
 * https://docs.aws.amazon.com/IAM/latest/UserGuide/create-signed-request.html#add-signature-to-request
 *
 * Combines the following components into the Authorization header format:
 * 1. Algorithm (AWS4-HMAC-SHA256)
 * 2. Credential (access_key/date/region/service/aws4_request)
 * 3. SignedHeaders (alphabetically sorted list of signed header names)
 * 4. Signature (hex-encoded signature calculated from string to sign)
 *
 * @v params               Pointer to AwsSigv4Params
 * @v sigv4                The calculated signature string
 * @v authorization_header Pointer to store the allocated header string
 *
 * @ret rc                 Return status code
 *
 * The caller is responsible for freeing the allocated memory.
 */
static int construct_authorization_header ( const AwsSigv4Params *params, const char *sigv4,
											char **authorization_header ) {
	int rc = 0;
	/* Initialize output parameter */
	*authorization_header = NULL;

	ASPRINTF_OR_GOTO_CLEANUP ( *authorization_header,
							   "%s Credential=%s/%s/%s/%s/%s,"
							   "SignedHeaders=%s,Signature=%s",
							   AWS_ALGORITHM, params->access_key, params->date_stamp,
							   params->region, params->service, AWS_REQUEST_TYPE,
							   AWS_SIGNED_HEADERS, sigv4 );

	return 0;

cleanup:
	return rc;
}

/**
 * Creates an AWS API request URI with authorization headers.
 *
 * Constructs a URI for AWS API requests with necessary authentication headers
 * including the AWS SigV4 Authorization header, service endpoint, and JSON payload.
 *
 * @v params    Pointer to AWS SigV4 parameters
 * @v sigv4     AWS SigV4 signature string
 * @v payload   JSON payload string for request body
 * @v uri       Output pointer to store the URI structure
 * @ret rc      Return status code
 *
 * The caller must free the URI structure with uri_put() after use.
 */
int generate_aws_request ( const AwsSigv4Params *params, char *sigv4, char *payload, struct uri **uri ) {
	char *uri_string = NULL;
	struct uri *aws_uri = NULL;
	char *authorization_header = NULL;
	int rc = 0;

	/* Initialize output parameter */
	*uri = NULL;

	/* Construct AWS endpoint string */
	ASPRINTF_OR_GOTO_CLEANUP ( uri_string, "https://%s.%s.amazonaws.com",
							   params->service, params->region );

	aws_uri = parse_uri ( uri_string );
	if ( ! aws_uri ) {
		rc = -ENOMEM;
		goto cleanup;
	}
	aws_uri->method = &http_post;
	aws_uri->params = create_parameters ( "AWS Parameter List" );

	/* Construct authorization header */
	if ( ( rc = construct_authorization_header ( params, sigv4, &authorization_header ) ) != 0 )
		goto cleanup;

	/* Add headers */
	/* A "Host" header will automatically be added by iPXE from the parsed URI String */
	add_parameter ( aws_uri->params, "X-Amz-Target", "secretsmanager.GetSecretValue", PARAMETER_HEADER );
	add_parameter ( aws_uri->params, "X-Amz-Date", params->amz_date, PARAMETER_HEADER );
	add_parameter ( aws_uri->params, "X-Amz-Security-Token", params->session_token, PARAMETER_HEADER );
	add_parameter ( aws_uri->params, "Authorization", authorization_header, PARAMETER_HEADER );
	add_parameter ( aws_uri->params, "Accept", "*/*", PARAMETER_HEADER );
	add_parameter ( aws_uri->params, "Content-Type", AWS_CONTENT_TYPE, PARAMETER_HEADER );
	add_parameter ( aws_uri->params, payload, "", PARAMETER_JSON );

	*uri = aws_uri;
	aws_uri = NULL;

	free ( uri_string );
	return 0;

cleanup:
	free ( uri_string );
	free ( authorization_header ); /* Only free on error path */
	if ( aws_uri )
		uri_put ( aws_uri );
	return rc;
}

/**
 * Extracts a credential from AWS Secrets Manager response and stores it in settings.
 *
 * Parses JSON response, extracts the specified key from SecretString, and
 * stores the value in iPXE settings under the provided setting name.
 *
 * @v response      JSON response from AWS Secrets Manager
 * @v kv            Storage for extracted key-value pair
 * @v json_key      Key to extract from SecretString
 * @v setting       Named setting structure for storage
 * @v setting_name  Name for the setting in iPXE
 * @ret rc          Return status code
 */
int parse_and_store_credential ( char *response, json_kv *kv, char *json_key,
								 struct named_setting *setting, char *setting_name ) {
	char *output = NULL;
	char *secret_string = NULL;
	int rc;

	/* Initialize output parameters */
	kv->key = NULL;
	kv->value = NULL;

	/* Extract the SecretString */
	rc = json_extract_string ( response, "SecretString", &secret_string );
	if ( rc != 0 ) {
		DBG ( "ERR: Could not extract SecretString.\n" );
		goto out;
	}

	/* Extract the value from the SecretString */
	rc = json_extract_string ( secret_string, json_key, &output );
	if ( rc != 0 ) {
		DBG ( "ERR: Failed to get \"%s\" from JSON. Verify it is configured correctly.\n", json_key );
		goto out;
	}

	kv->key = strdup ( json_key );
	kv->value = strdup ( output );
	if ( ! kv->key || ! kv->value ) {
		rc = -ENOMEM;
		goto err_strdup;
	}

	/* Set the value in settings */
	parse_autovivified_setting ( setting_name, setting );
	rc = storef_setting ( setting->settings, &setting->setting, kv->value );
	if ( rc != 0 ) {
		goto err_storef;
	}

	free ( output );
	free ( secret_string );
	return 0;

err_storef:
err_strdup:
	free ( kv->key );
	free ( kv->value );
	kv->key = NULL;
	kv->value = NULL;
out:
	free ( output );
	free ( secret_string );
	return rc;
}
