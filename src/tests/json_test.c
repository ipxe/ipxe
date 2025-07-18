FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * JSON string extraction tests
 *
 */

/* Forcibly enable assertions */
#undef NDEBUG

#include <ipxe/test.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <usr/json.h>

/** A JSON extraction test */
struct json_test {
	/** Input JSON string */
	const char *json;
	/** Key to extract */
	const char *key;
	/** Expected output */
	const char *expected;
	/** Expected return code */
	int expected_rc;
};

/** Define a JSON test */
#define JSON_TEST( _name, _json, _key, _expected, _rc ) \
	static struct json_test _name = {                   \
		.json = _json,                                  \
		.key = _key,                                    \
		.expected = _expected,                          \
		.expected_rc = _rc,                             \
	}

/** AWS credentials response test (sanitized) */
JSON_TEST ( aws_creds_test,
			"{\n"
			"  \"Code\" : \"Success\",\n"
			"  \"LastUpdated\" : \"2025-06-19T20:56:49Z\",\n"
			"  \"Type\" : \"AWS-HMAC\",\n"
			"  \"AccessKeyId\" : \"ASIAEXAMPLEACCESSKEY\",\n"
			"  \"SecretAccessKey\" : \"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY\",\n"
			"  \"Token\" : \"IQoJb3JpZ2luX2V4AMEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLE"
			"TOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXA"
			"MPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKE"
			"NEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLETOKENEXAMPLET"
			"OKENEXAMPLETOKENEXAMPLETOKENEXAMPLE==\",\n"
			"  \"Expiration\" : \"2025-06-20T03:31:27Z\"\n"
			"}",
			"AccessKeyId",
			"ASIAEXAMPLEACCESSKEY",
			0 );

/** AWS secrets manager test */
JSON_TEST ( aws_secret_test,
			"{\"SecretString\":\"{\\\"password\\\":\\\"my-secure-password\\\"}\"}",
			"SecretString",
			"{\"password\":\"my-secure-password\"}",
			0 );

/** Escaped characters test */
JSON_TEST ( escaped_chars_test,
			"{\"value\":\"escaped\\\"quote and \\\\backslash\"}",
			"value",
			"escaped\"quote and \\backslash",
			0 );

/**
 * Report a JSON extraction test result
 *
 * @v test      JSON test
 * @v file      Test code file
 * @v line      Test code line
 */
static void json_extract_okx ( struct json_test *test, const char *file,
							   unsigned int line ) {
	char *output = NULL;
	int rc;

	rc = json_extract_string ( ( char * ) test->json,
							   ( char * ) test->key, &output );

	okx ( rc == test->expected_rc, file, line );

	if ( rc == 0 ) {
		okx ( strcmp ( output, test->expected ) == 0, file, line );
	}

	free ( output );
}
#define json_extract_ok( test ) json_extract_okx ( test, __FILE__, __LINE__ )

/**
 * Perform JSON extraction self-tests
 *
 */
static void json_test_exec ( void ) {
	json_extract_ok ( &aws_creds_test );
	json_extract_ok ( &aws_secret_test );
	json_extract_ok ( &escaped_chars_test );
}

/** JSON extraction self-test */
struct self_test json_test __self_test = {
	.name = "json",
	.exec = json_test_exec,
};
