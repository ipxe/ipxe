#ifndef _GPXE_SHA1_H
#define _GPXE_SHA1_H

#include "crypto/axtls/crypto.h"

struct crypto_algorithm;

#define SHA1_CTX_SIZE sizeof ( SHA1_CTX )
#define SHA1_DIGEST_SIZE SHA1_SIZE

extern struct crypto_algorithm sha1_algorithm;

#endif /* _GPXE_SHA1_H */
