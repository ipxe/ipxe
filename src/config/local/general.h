#define	NET_PROTO_IPV6		/* IPv6 protocol */
#undef	NET_PROTO_STP		/* Spanning Tree protocol */

#undef	SANBOOT_PROTO_ISCSI	/* iSCSI protocol */
#undef	SANBOOT_PROTO_AOE	/* AoE protocol */
#undef	SANBOOT_PROTO_IB_SRP	/* Infiniband SCSI RDMA protocol */
#undef	SANBOOT_PROTO_FCP	/* Fibre Channel protocol */
#undef	SANBOOT_PROTO_HTTP	/* HTTP SAN protocol */

#undef HTTP_AUTH_BASIC		/* Basic authentication */
#undef HTTP_AUTH_DIGEST	/* Digest authentication */

#undef	CRYPTO_80211_WEP	/* WEP encryption (deprecated and insecure!) */
#undef	CRYPTO_80211_WPA	/* WPA Personal, authenticating with passphrase */
#undef	CRYPTO_80211_WPA2	/* Add support for stronger WPA cryptography */

#define VLAN_CMD		/* VLAN commands */

#define PING_CMD		/* Ping command */
#define NEIGHBOUR_CMD		/* Neighbour management commands */
#define IPSTAT_CMD		/* IP statistics commands */
#define NSLOOKUP_CMD		/* DNS resolving command */
#define REBOOT_CMD		/* Reboot command */
#define POWEROFF_CMD		/* Power off command */

#undef VNIC_IPOIB		/* Infiniband IPoIB virtual NICs */
