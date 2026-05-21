/* Enable IPv6 and HTTPS */
#define NET_PROTO_IPV6
#define DOWNLOAD_PROTO_HTTPS

/* Allow retrieval of metadata (such as an iPXE boot script) from
 * Google Compute Engine metadata server.
 */
#define HTTP_HACK_GCE

/* Allow retrieval of metadata from Amazon EC2 Instance Metadata
 * Service using IMDSv2 session tokens.
 */
#define HTTP_HACK_EC2

/* Allow scripts to handle errors by powering down the VM to avoid
 * incurring unnecessary costs.
 */
#define POWEROFF_CMD

/* Allow scripts to fetch a URI and store the response body in a
 * named setting for use in subsequent commands.
 */
#define FETCHVAR_CMD
