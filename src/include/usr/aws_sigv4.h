#ifndef _USR_AWS_SIGV4_H
#define _USR_AWS_SIGV4_H

/** @file
 *
 * AWS Signature Version 4 Calculation
 * https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_sigv.html
 */

#include <ipxe/uri.h>

/* Elements of an AWS API request signature */
/* https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_sigv-signing-elements.html */
typedef struct {
	const char *payload;
	const char *service;
	const char *operation;
	const char *region;
	const char *amz_date;
	const char *date_stamp;
	const char *access_key;
	const char *secret_key;
	const char *session_token;
} AwsSigv4Params;

/* Parameters used in the Canonical Request */
typedef struct {
	char *http_method;
	char *canonical_uri;
	char *canonical_query_string;
	char *canonical_headers;
	char *signed_headers;
	char *payload_hash;
} CanonicalRequestParams;

/* Parameters used in the String To Sign */
typedef struct {
	const char *algorithm;
	const char *amz_date;
	char *credential_scope;
	char *canonical_request_hash;
} StringToSignParams;

typedef struct {
	char *key;
	char *value;
} json_kv;

extern int generate_get_secret_value_payload ( const char *secret_id, char **payload );
extern int aws_sigv4 ( const AwsSigv4Params *params, char **sigv4 );
extern int generate_aws_request ( const AwsSigv4Params *params, char *sigv4, char *payload, struct uri **uri );
extern int parse_and_store_credential ( char *response, json_kv *kv, char *json_key,
										struct named_setting *setting, char *setting_name );
extern int parse_secret_json ( char *json, char *key, json_kv *kv );

#endif /* _USR_AWS_SIGV4_H */
