#ifndef ETHERBOOT_BITS_ENDIAN_H
#define ETHERBOOT_BITS_ENDIAN_H

#define __BYTE_ORDER __LITTLE_ENDIAN

#define le32_to_cpup(x) (*(uint32_t *)(x))
#define cpu_to_le16p(x) (*(uint16_t*)(x))

#endif /* ETHERBOOT_BITS_ENDIAN_H */
