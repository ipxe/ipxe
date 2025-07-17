#ifndef _USR_IMDSV2_H
#define _USR_IMDSV2_H

/** @file
 *
 * AWS Instance Metadata Service (IMDSv2) helper commands
 *
 */

extern int url_concat ( const char *base_url, const char *path, char **url );
extern int get_imdsv2_token ( char **token, const char *base_url );
extern int get_imdsv2_metadata ( char *token, const char *base_url, char *metadata_path, char **response );
extern int get_imds_metadata_base_url ( int use_ipv6, const char **base_url );
extern int parse_imdsv2_credentials_response ( char *credentials, char *key, char **parsed_val );

#define IMDSV2_IPV4_METADATA_BASE_URL "http://169.254.169.254/latest/"
#define IMDSV2_IPV6_METADATA_BASE_URL "http://[fd00:ec2::254]/latest/"

#endif /* _USR_IMDSV2_H */
