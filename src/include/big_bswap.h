#ifndef ETHERBOOT_BIG_BSWAP_H
#define ETHERBOOT_BIG_BSWAP_H

#define ntohl(x) 	(x)
#define htonl(x) 	(x)
#define ntohs(x) 	(x)
#define htons(x) 	(x)
#define cpu_to_le32(x)	__bswap_32(x)
#define cpu_to_le16(x)	__bswap_16(x)
#define cpu_to_be32(x)	(x)
#define cpu_to_be16(x)	(x)
#define le32_to_cpu(x)	__bswap_32(x)
#define le16_to_cpu(x)	__bswap_16(x)
#define be32_to_cpu(x)	(x)
#define be16_to_cpu(x)	(x)

#endif /* ETHERBOOT_BIG_BSWAP_H */
