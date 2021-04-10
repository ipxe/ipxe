/* Enable IPv6 and HTTPS */
#define NET_PROTO_IPV6
#define DOWNLOAD_PROTO_HTTPS

/* Allow retrieval of metadata (such as an iPXE boot script) from
 * Google Compute Engine metadata server.
 */
#define HTTP_HACK_GCE

/* Allow scripts to handle errors by powering down the VM to avoid
 * incurring unnecessary costs.
 */
#define POWEROFF_CMD
