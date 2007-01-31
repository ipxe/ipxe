#ifndef _GPXE_TLS_H
#define _GPXE_TLS_H

#include <errno.h>

struct stream_application;

static inline int add_tls ( struct stream_application *app __unused ) {
	return -ENOTSUP;
}

#endif /* _GPXE_TLS_H */
