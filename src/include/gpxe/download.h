#ifndef _GPXE_DOWNLOAD_H
#define _GPXE_DOWNLOAD_H

/** @file
 *
 * Download protocols
 *
 */

#include <stdint.h>
#include <gpxe/uaccess.h>
#include <gpxe/async.h>
#include <gpxe/buffer.h>
#include <gpxe/uri.h>
#include <gpxe/tables.h>

/** A download protocol */
struct download_protocol {
	/** Protocol name (e.g. "http") */ 
	const char *name;
	/** Start a download via this protocol
	 *
	 * @v uri		Uniform Resource Identifier
	 * @v buffer		Buffer into which to download file
	 * @v parent		Parent asynchronous operation
	 * @ret rc		Return status code
	 *
	 * The @c uri and @c buffer will remain persistent for the
	 * duration of the asynchronous operation.
	 */
	int ( * start_download ) ( struct uri *uri, struct buffer *buffer,
				   struct async *parent );
};

/** Register a download protocol */
#define __download_protocol __table ( struct download_protocol, \
				      download_protocols, 01 )

/** A download in progress */
struct download {
	/** User buffer allocated for the download */
	userptr_t *data;
	/** Size of the download */
	size_t *len;

	/** URI being downloaded */
	struct uri *uri;
	/** Expandable buffer for this download */
	struct buffer buffer;
	/** Download protocol */
	struct download_protocol *protocol;
	/** Asynchronous operation for this download */
	struct async async;
};

extern int start_download ( const char *uri_string, struct async *parent,
			    userptr_t *data, size_t *len );

#endif /* _GPXE_DOWNLOAD_H */
