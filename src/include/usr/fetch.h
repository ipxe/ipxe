#ifndef _USR_FETCH_H
#define _USR_FETCH_H

/**
 * @file
 *
 * Fetch file
 *
 */

#include <stdint.h>
#include <gpxe/uaccess.h>

extern int fetch ( const char *filename, userptr_t *data, size_t *len );

#endif /* _USR_FETCH_H */
