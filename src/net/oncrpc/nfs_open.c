/*
 * Copyright (C) 2013 Marin Hannache <ipxe@mareo.fr>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <byteswap.h>
#include <ipxe/time.h>
#include <ipxe/socket.h>
#include <ipxe/tcpip.h>
#include <ipxe/in.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/uri.h>
#include <ipxe/features.h>
#include <ipxe/nfs.h>
#include <ipxe/nfs_open.h>
#include <ipxe/oncrpc.h>
#include <ipxe/oncrpc_iob.h>
#include <ipxe/portmap.h>
#include <ipxe/mount.h>
#include <ipxe/nfs_uri.h>
#include <ipxe/blockdev.h>
#include <ipxe/blocktrans.h>
#include <ipxe/acpi.h>

/** @file
 *
 * Network File System protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "NFS", DHCP_EB_FEATURE_NFS, 1 );

#define NFS_RSIZE 100000
/** Block size used for nfs block device requests */
#define NFS_BLKSIZE 512

enum nfs_pm_state {
	NFS_PORTMAP_NONE = 0,
	NFS_PORTMAP_MOUNTPORT,
	NFS_PORTMAP_NFSPORT,
	NFS_PORTMAP_CLOSED,
};

enum nfs_mount_state {
	NFS_MOUNT_NONE = 0,
	NFS_MOUNT_MNT,
	NFS_MOUNT_UMNT,
	NFS_MOUNT_CLOSED,
};

enum nfs_state {
	NFS_NONE = 0,
	NFS_LOOKUP,
	NFS_LOOKUP_SENT,
	NFS_READLINK,
	NFS_READLINK_SENT,
	NFS_READ,
	NFS_READ_SENT,
	NFS_CLOSED,
};

enum nfs_mode {
	NFS_READ_ALL = 0,
	NFS_BLOCK_MODE,
};

/**
 * A NFS request
 *
 */
struct nfs_request {
	/** Reference counter */
	struct refcnt           refcnt;
	/** Data transfer interface */
	struct interface        xfer;

	struct interface        pm_intf;
	struct interface        mount_intf;
	struct interface        nfs_intf;

	enum nfs_pm_state       pm_state;
	enum nfs_mount_state    mount_state;
	enum nfs_state          nfs_state;

	struct oncrpc_session   pm_session;
	struct oncrpc_session   mount_session;
	struct oncrpc_session   nfs_session;

	struct oncrpc_cred_sys  auth_sys;

	char *                  hostname;
	struct nfs_uri          uri;

	struct nfs_fh           readlink_fh;
	struct nfs_fh           current_fh;
	struct nfs_fh		dir_fh;		// the handle for the opened dir
	uint64_t                file_offset;
	size_t			bytes_to_read;

	size_t                  remaining;
	int                     eof;
	enum nfs_mode		mode;
};
struct nfs_range_request {
	uint64_t                file_offset;
	size_t			bytes_to_read;
};

static void nfs_step ( struct nfs_request *nfs );
static int nfs_open(struct interface *data, struct nfs_uri *uri, char *hostname, enum nfs_mode mode, struct nfs_range_request *range);

/**
 * Free NFS request
 *
 * @v refcnt		Reference counter
 */
static void nfs_free ( struct refcnt *refcnt ) {
	struct nfs_request      *nfs;

	nfs = container_of ( refcnt, struct nfs_request, refcnt );
	DBGC ( nfs, "NFS_OPEN %p freed\n", nfs );

	nfs_uri_free ( &nfs->uri );

	free ( nfs->hostname );
	free ( nfs->auth_sys.hostname );
	free ( nfs );
}

/**
 * Mark NFS operation as complete
 *
 * @v nfs		NFS request
 * @v rc		Return status code
 */
static void nfs_done ( struct nfs_request *nfs, int rc ) {
	if ( rc == 0 && nfs->nfs_state != NFS_CLOSED )
		rc = -ECONNRESET;

	DBGC ( nfs, "NFS_OPEN %p completed (%s)\n", nfs, strerror ( rc ) );

	intf_shutdown ( &nfs->xfer, rc );
	intf_shutdown ( &nfs->pm_intf, rc );
	intf_shutdown ( &nfs->mount_intf, rc );
	intf_shutdown ( &nfs->nfs_intf, rc );
}

int nfs_acpi_describe ( struct nfs_request *nfs,
			 struct acpi_description_header *acpi, size_t len ) {

	DBGC ( nfs, "NFS %p cannot yet describe device in an ACPI table\n",
	       nfs );
	( void ) acpi;
	( void ) len;
	return 0;
}

/**
 * Read from block device
 *
 * @v nfs		NFS transaction
 * @v data		Data interface
 * @v lba		Starting logical block address
 * @v count		Number of logical blocks
 * @v buffer		Data buffer
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
int nfs_block_read ( struct nfs_request *nfs, struct interface *data,
		      uint64_t lba, unsigned int count, userptr_t buffer,
		      size_t len ) {
	int rc;
	struct nfs_range_request range;

	DBGC(nfs,"NFS_OPEN %p block %d+%d read\n",nfs,(int)lba,count);
	/* Sanity check */
	assert ( len == ( count * NFS_BLKSIZE ) );

	range.file_offset = lba * NFS_BLKSIZE;
	range.bytes_to_read = len;

	/* Start a range request to retrieve the block(s) */
	if ( ( rc = nfs_open ( data, &nfs->uri, nfs->hostname, NFS_BLOCK_MODE, &range) ) != 0 )
		goto err_open;

	/* Insert block device translator */
	if ( ( rc = block_translate ( data, buffer, len ) ) != 0 ) {
		DBGC ( nfs, "NFS %p could not insert block translator: %s\n",
		       nfs, strerror ( rc ) );
		goto err_translate;
	}

	return 0;

 err_translate:
	intf_restart ( data, rc );
 err_open:
	return rc;
}

/**
 * Read block device capacity
 *
 * @v control		Control interface
 * @v data		Data interface
 * @ret rc		Return status code
 */
int nfs_block_read_capacity ( struct nfs_request *nfs,
			       struct interface *data ) {
	int rc;

	DBGC(nfs,"block read capacity\n");
	/* Start a HEAD request to retrieve the capacity */
	if ( ( rc = nfs_open ( data, &nfs->uri, nfs->hostname, NFS_BLOCK_MODE, NULL) ) != 0 )
		goto err_open;

	/* Insert block device translator */
	if ( ( rc = block_translate ( data, UNULL, NFS_BLKSIZE ) ) != 0 ) {
		DBGC ( nfs, "NFS %p could not insert block translator: %s\n",
		       nfs, strerror ( rc ) );
		goto err_translate;
	}

	return 0;

 err_translate:
	intf_restart ( data, rc );
 err_open:
	return rc;
}

static int nfs_connect ( struct interface *intf, uint16_t port,
                         const char *hostname ) {
	struct sockaddr_tcpip   peer;
	struct sockaddr_tcpip   local;

	if ( ! intf || ! hostname || ! port )
		return -EINVAL;

	memset ( &peer, 0, sizeof ( peer ) );
	memset ( &local, 0, sizeof ( local ) );
	peer.st_port = htons ( port );

	/* Use a local port < 1024 to avoid using the 'insecure' option in
	 * /etc/exports file. */
	local.st_flags = TCPIP_BIND_PRIVILEGED;

	return xfer_open_named_socket ( intf, SOCK_STREAM,
	                                ( struct sockaddr * ) &peer, hostname,
                                        ( struct sockaddr * ) &local );
}

static void nfs_pm_step ( struct nfs_request *nfs ) {
	int     rc;

	if ( ! xfer_window ( &nfs->pm_intf ) )
		return;

	if ( nfs->pm_state == NFS_PORTMAP_NONE ) {
		DBGC ( nfs, "NFS_OPEN %p GETPORT call (mount)\n", nfs );

		rc = portmap_getport ( &nfs->pm_intf, &nfs->pm_session,
		                       ONCRPC_MOUNT, MOUNT_VERS,
		                       PORTMAP_PROTO_TCP );
		if ( rc != 0 )
			goto err;

		nfs->pm_state = NFS_PORTMAP_MOUNTPORT;
		return;
	}

	if ( nfs->pm_state == NFS_PORTMAP_NFSPORT ) {
		DBGC ( nfs, "NFS_OPEN %p GETPORT call (nfs)\n", nfs );

		rc = portmap_getport ( &nfs->pm_intf, &nfs->pm_session,
		                       ONCRPC_NFS, NFS_VERS,
		                       PORTMAP_PROTO_TCP );
		if ( rc != 0 )
			goto err;

		return;
	}

	return;
err:
	nfs_done ( nfs, rc );
}

static int nfs_pm_deliver ( struct nfs_request *nfs,
                            struct io_buffer *io_buf,
                            struct xfer_metadata *meta __unused ) {
	int                             rc;
	struct oncrpc_reply             reply;
	struct portmap_getport_reply    getport_reply;

	oncrpc_get_reply ( &nfs->pm_session, &reply, io_buf );
	if ( reply.accept_state != 0 )
	{
		rc = -EPROTO;
		goto err;
	}

	if ( nfs->pm_state == NFS_PORTMAP_MOUNTPORT ) {
		DBGC ( nfs, "NFS_OPEN %p got GETPORT reply (mount)\n", nfs );

		rc = portmap_get_getport_reply ( &getport_reply, &reply );
		if ( rc != 0 )
			goto err;

		rc = nfs_connect ( &nfs->mount_intf, getport_reply.port,
	                           nfs->hostname );
		if ( rc != 0 )
			goto err;

		nfs->pm_state = NFS_PORTMAP_NFSPORT;
		nfs_pm_step ( nfs );

		goto done;
	}

	if ( nfs->pm_state == NFS_PORTMAP_NFSPORT ) {
		DBGC ( nfs, "NFS_OPEN %p got GETPORT reply (nfs)\n", nfs );

		rc = portmap_get_getport_reply ( &getport_reply, &reply );
		if ( rc != 0 )
			goto err;

		rc = nfs_connect ( &nfs->nfs_intf, getport_reply.port,
	                           nfs->hostname );
		if ( rc != 0 )
			goto err;

		intf_shutdown ( &nfs->pm_intf, 0 );
		nfs->pm_state = NFS_PORTMAP_CLOSED;

		goto done;
	}

	rc = -EPROTO;
err:
	nfs_done ( nfs, rc );
done:
	free_iob ( io_buf );
	return 0;
}

static void nfs_mount_step ( struct nfs_request *nfs ) {
	int     rc;

	if ( ! xfer_window ( &nfs->mount_intf ) )
		return;

	if ( nfs->mount_state == NFS_MOUNT_NONE ) {
		DBGC ( nfs, "NFS_OPEN %p MNT call (%s)\n", nfs,
		       nfs_uri_mountpoint ( &nfs->uri ) );

		rc = mount_mnt ( &nfs->mount_intf, &nfs->mount_session,
		                 nfs_uri_mountpoint ( &nfs->uri ) );
		if ( rc != 0 )
			goto err;

		nfs->mount_state = NFS_MOUNT_MNT;
		return;
	}

	if ( nfs->mount_state == NFS_MOUNT_UMNT ) {
		DBGC ( nfs, "NFS_OPEN %p UMNT call\n", nfs );

		rc = mount_umnt ( &nfs->mount_intf, &nfs->mount_session,
		                 nfs_uri_mountpoint ( &nfs->uri ) );
		if ( rc != 0 )
			goto err;
	}

	return;
err:
	nfs_done ( nfs, rc );
}

static int nfs_mount_deliver ( struct nfs_request *nfs,
                               struct io_buffer *io_buf,
                               struct xfer_metadata *meta __unused ) {
	int                     rc;
	struct oncrpc_reply     reply;
	struct mount_mnt_reply  mnt_reply;

	oncrpc_get_reply ( &nfs->mount_session, &reply, io_buf );
	if ( reply.accept_state != 0 )
	{
		rc = -EPROTO;
		goto err;
	}

	if ( nfs->mount_state == NFS_MOUNT_MNT ) {
		DBGC ( nfs, "NFS_OPEN %p got MNT reply\n", nfs );
		rc = mount_get_mnt_reply ( &mnt_reply, &reply );
		if ( rc != 0 ) {
			switch ( mnt_reply.status ) {
				case MNT3ERR_NOTDIR:
				case MNT3ERR_NOENT:
				case MNT3ERR_ACCES:
					break;

				default:
					goto err;
			}

			if ( ! strcmp ( nfs_uri_mountpoint ( &nfs->uri ),
				        "/" ) )
				goto err;

			if ( ( rc = nfs_uri_next_mountpoint ( &nfs->uri ) ) )
				goto err;

			DBGC ( nfs, "NFS_OPEN %p MNT failed retrying with " \
			       "%s\n", nfs, nfs_uri_mountpoint ( &nfs->uri ) );

			nfs->mount_state = NFS_MOUNT_NONE;
			nfs_mount_step ( nfs );

			goto done;
		}

		nfs->dir_fh = mnt_reply.fh;
		nfs->nfs_state = NFS_LOOKUP;
		nfs_step ( nfs );

		goto done;
	}

	if ( nfs->mount_state == NFS_MOUNT_UMNT ) {
		DBGC ( nfs, "NFS_OPEN %p got UMNT reply\n", nfs );
		nfs_done ( nfs, 0 );

		goto done;
	}

	rc = -EPROTO;
err:
	nfs_done ( nfs, rc );
done:
	free_iob ( io_buf );
	return 0;
}

static void nfs_step ( struct nfs_request *nfs ) {
	int     rc;
	char    *path_component;

	if ( ! xfer_window ( &nfs->nfs_intf ) )
		return;

	if ( nfs->nfs_state == NFS_LOOKUP ) {
		path_component = nfs_uri_next_path_component ( &nfs->uri );

		DBGC ( nfs, "NFS_OPEN %p LOOKUP call (%s)\n", nfs,
                       path_component );

		rc = nfs_lookup ( &nfs->nfs_intf, &nfs->nfs_session,
		                  &nfs->dir_fh, path_component );
		if ( rc != 0 )
			goto err;

		nfs->nfs_state = NFS_LOOKUP_SENT;
		return;
	}


	if ( nfs->nfs_state == NFS_READLINK ) {
		DBGC ( nfs, "NFS_OPEN %p READLINK call\n", nfs );

		rc = nfs_readlink ( &nfs->nfs_intf, &nfs->nfs_session,
		                    &nfs->readlink_fh );
		if ( rc != 0 )
			goto err;

		nfs->nfs_state = NFS_READLINK_SENT;
		return;
	}

	if ( nfs->nfs_state == NFS_READ ) {
		size_t len = NFS_RSIZE;
		if (len > nfs->bytes_to_read) len = nfs->bytes_to_read;

		DBGC ( nfs, "NFS_OPEN %p READ(%d,%d) call\n", nfs, (int)nfs->file_offset, len);

		rc = nfs_read ( &nfs->nfs_intf, &nfs->nfs_session,
		                &nfs->current_fh, nfs->file_offset,
		                len );
		if ( rc != 0 )
			goto err;

		nfs->nfs_state = NFS_READ_SENT;
		return;
	}

	return;
err:
	nfs_done ( nfs, rc );
}

static int nfs_deliver ( struct nfs_request *nfs,
                         struct io_buffer *io_buf,
                         struct xfer_metadata *meta __unused ) {
	int                     rc;
	struct oncrpc_reply     reply;

	if ( nfs->remaining == 0 ) {
		oncrpc_get_reply ( &nfs->nfs_session, &reply, io_buf );
		if ( reply.accept_state != 0 ) {
			rc = -EPROTO;
			goto err;
		}
	}

	if ( nfs->nfs_state == NFS_LOOKUP_SENT ) {
		struct nfs_lookup_reply lookup_reply;

		DBGC ( nfs, "NFS_OPEN %p got LOOKUP reply\n", nfs );

		rc = nfs_get_lookup_reply ( &lookup_reply, &reply );
		if ( rc != 0 )
			goto err;

		if ( lookup_reply.ent_type == NFS_ATTR_SYMLINK ) {
			nfs->readlink_fh = lookup_reply.fh;
			nfs->nfs_state   = NFS_READLINK;
		} else {
			nfs->current_fh = lookup_reply.fh;

			if ( nfs->uri.lookup_pos[0] == '\0' ) {
				if (nfs->mode == NFS_READ_ALL) {
					nfs->bytes_to_read = lookup_reply.size;
					nfs->nfs_state = NFS_READ;
				} else {
					if (nfs->bytes_to_read) {
						nfs->nfs_state = NFS_READ;
						nfs_step(nfs);
					} else {
						xfer_seek(&nfs->xfer, lookup_reply.size);
						xfer_seek(&nfs->xfer, 0);
						intf_shutdown ( &nfs->nfs_intf, 0 );
						nfs->nfs_state = NFS_CLOSED;
						nfs->mount_state = NFS_MOUNT_UMNT;
						nfs_mount_step ( nfs );
					}
				}
			} else
				nfs->nfs_state = NFS_LOOKUP;
		}

		nfs_step ( nfs );
		goto done;
	}

	if ( nfs->nfs_state == NFS_READLINK_SENT ) {
		char                      *path;
		struct nfs_readlink_reply readlink_reply;

		DBGC ( nfs, "NFS_OPEN %p got READLINK reply\n", nfs );

		rc = nfs_get_readlink_reply ( &readlink_reply, &reply );
		if ( rc != 0 )
			goto err;

		if ( readlink_reply.path_len == 0 )
		{
			rc = -EINVAL;
			goto err;
		}

		if ( ! ( path = strndup ( readlink_reply.path,
		                          readlink_reply.path_len ) ) )
		{
			rc = -ENOMEM;
			goto err;
		}

		nfs_uri_symlink ( &nfs->uri, path );
		free ( path );

		DBGC ( nfs, "NFS_OPEN %p new path: %s\n", nfs,
		       nfs->uri.path );

		nfs->nfs_state = NFS_LOOKUP;
		nfs_step ( nfs );
		goto done;
	}

	if ( nfs->nfs_state == NFS_READ_SENT ) {
		if ( nfs->remaining == 0 ) {
			DBGC ( nfs, "NFS_OPEN %p got READ reply\n", nfs );

			struct nfs_read_reply read_reply;

			rc = nfs_get_read_reply ( &read_reply, &reply );
			if ( rc != 0 )
				goto err;

			if ( nfs->file_offset == 0 ) {
				DBGC2 ( nfs, "NFS_OPEN %p size: %llu bytes\n",
				        nfs, read_reply.filesize );

				xfer_seek ( &nfs->xfer, read_reply.filesize );
				xfer_seek ( &nfs->xfer, 0 );
			}

			nfs->file_offset += read_reply.count;
			nfs->bytes_to_read -= read_reply.count;
			nfs->remaining    = read_reply.count;
			nfs->eof          = read_reply.eof;
		}

		size_t len = iob_len ( io_buf );
		if ( len > nfs->remaining )
			iob_unput ( io_buf, len - nfs->remaining );

		nfs->remaining -= iob_len ( io_buf );

		DBGC ( nfs, "NFS_OPEN %p got %zd bytes\n", nfs,
		       iob_len ( io_buf ) );

		rc = xfer_deliver_iob ( &nfs->xfer, iob_disown ( io_buf ) );
		if ( rc != 0 )
			goto err;

		if ( nfs->remaining == 0 ) {
			if (nfs->mode == NFS_BLOCK_MODE) {
				if (nfs->bytes_to_read) {
					nfs->nfs_state = NFS_READ;
					nfs_step(nfs);
				} else {
					intf_shutdown ( &nfs->nfs_intf, 0 );
					nfs->nfs_state = NFS_CLOSED;
					nfs->mount_state = NFS_MOUNT_UMNT;
					nfs_mount_step ( nfs );
				}
			} else if ( ! nfs->eof ) {
				DBGC(nfs,"NFS_OPEN condition1\n");
				nfs->nfs_state = NFS_READ;
				nfs_step ( nfs );
			} else {
				DBGC(nfs,"NFS_OPEN condition2\n");
				intf_shutdown ( &nfs->nfs_intf, 0 );
				nfs->nfs_state = NFS_CLOSED;
				nfs->mount_state = NFS_MOUNT_UMNT;
				nfs_mount_step ( nfs );
			}
		}

		return 0;
	}

	rc = -EPROTO;
err:
	nfs_done ( nfs, rc );
done:
	free_iob ( io_buf );
	return 0;
}

/*****************************************************************************
 * Interfaces
 *
 */

// OP's exposed to ipxe
static struct interface_operation nfs_xfer_operations[] = {
	INTF_OP ( intf_close, struct nfs_request *, nfs_done ),
	INTF_OP ( block_read, struct nfs_request *, nfs_block_read ),
	INTF_OP ( block_read_capacity, struct nfs_request *,  nfs_block_read_capacity ),
	INTF_OP ( acpi_describe, struct nfs_request *,  nfs_acpi_describe ),
};

/** NFS data transfer interface descriptor */
static struct interface_descriptor nfs_xfer_desc =
	INTF_DESC ( struct nfs_request, xfer, nfs_xfer_operations );

// OP's for the portmap socket
static struct interface_operation nfs_pm_operations[] = {
	INTF_OP ( intf_close, struct nfs_request *, nfs_done ),
	INTF_OP ( xfer_deliver, struct nfs_request *, nfs_pm_deliver ),
	INTF_OP ( xfer_window_changed, struct nfs_request *, nfs_pm_step ),
};

static struct interface_descriptor nfs_pm_desc =
	INTF_DESC ( struct nfs_request, pm_intf, nfs_pm_operations );

// OP's for the mount socket
static struct interface_operation nfs_mount_operations[] = {
	INTF_OP ( intf_close, struct nfs_request *, nfs_done ),
	INTF_OP ( xfer_deliver, struct nfs_request *, nfs_mount_deliver ),
	INTF_OP ( xfer_window_changed, struct nfs_request *, nfs_mount_step ),
};

static struct interface_descriptor nfs_mount_desc =
	INTF_DESC ( struct nfs_request, mount_intf, nfs_mount_operations );

// OP's for the main nfs socket
static struct interface_operation nfs_operations[] = {
	INTF_OP ( intf_close, struct nfs_request *, nfs_done ),
	INTF_OP ( xfer_deliver, struct nfs_request *, nfs_deliver ),
	INTF_OP ( xfer_window_changed, struct nfs_request *, nfs_step ),
};

static struct interface_descriptor nfs_desc =
	INTF_DESC_PASSTHRU ( struct nfs_request, nfs_intf, nfs_operations,
	                     xfer );

/*****************************************************************************
 *
 * URI opener
 *
 */

static int nfs_parse_uri ( struct nfs_uri *uri_temp, char **hostname, const struct uri *uri ) {
	int     rc;

	if ( ! uri || ! uri->host || ! uri->path )
		return -EINVAL;

	if ( ( rc = nfs_uri_init ( uri_temp, uri ) ) != 0 )
		return rc;

	if ( ! ( *hostname = strdup ( uri->host ) ) ) {
		rc = -ENOMEM;
		goto err_hostname;
	}

	return 0;

err_hostname:
	nfs_uri_free ( uri_temp );
	return rc;
}

static int nfs_uri_copy(struct nfs_uri *dest, struct nfs_uri *src) {
	dest->mountpoint = strdup(src->mountpoint);
	dest->filename = strdup(src->filename);
	dest->path = strdup(src->path);
	dest->lookup_pos = dest->path;

	return 0;
}
static int nfs_open(struct interface *xfer, struct nfs_uri *uri, char *hostname, enum nfs_mode mode, struct nfs_range_request *range) {
	int                     rc;
	struct nfs_request      *nfs;

	nfs = zalloc ( sizeof ( *nfs ) );
	if ( ! nfs )
		return -ENOMEM;

	nfs->hostname = strdup(hostname);

	rc = nfs_uri_copy(&nfs->uri,uri);
	if (rc != 0) goto err_uri;

	DBGC ( nfs, "NFS_OPEN %p URI parsed: (mountpoint=%s, path=%s)\n",
	       nfs, nfs_uri_mountpoint ( &nfs->uri), nfs->uri.path );


	rc = oncrpc_init_cred_sys ( &nfs->auth_sys );
	if ( rc != 0 )
		goto err_cred;

	ref_init ( &nfs->refcnt, nfs_free );
	intf_init ( &nfs->xfer, &nfs_xfer_desc, &nfs->refcnt );
	intf_init ( &nfs->pm_intf, &nfs_pm_desc, &nfs->refcnt );
	intf_init ( &nfs->mount_intf, &nfs_mount_desc, &nfs->refcnt );
	intf_init ( &nfs->nfs_intf, &nfs_desc, &nfs->refcnt );

	portmap_init_session ( &nfs->pm_session, &nfs->auth_sys.credential );
	mount_init_session ( &nfs->mount_session, &nfs->auth_sys.credential );
	nfs_init_session ( &nfs->nfs_session, &nfs->auth_sys.credential );

	DBGC ( nfs, "NFS_OPEN %p connecting to port mapper (%s:%d)...\n", nfs,
	       nfs->hostname, PORTMAP_PORT );

	rc = nfs_connect ( &nfs->pm_intf, PORTMAP_PORT, nfs->hostname );
	if ( rc != 0 )
		goto err_connect;

	/* Attach to parent interface, mortalise self, and return */
	intf_plug_plug ( &nfs->xfer, xfer );
	ref_put ( &nfs->refcnt );

	nfs->mode = mode;

	if (mode == NFS_BLOCK_MODE) {
		if (range) {
			nfs->file_offset = range->file_offset;
			nfs->bytes_to_read = range->bytes_to_read;
			//nfs->remaining = 0;
			//nfs->nfs_state = NFS_READ;
			//nfs_step(nfs);
		} else {
			nfs->file_offset = 0;
			nfs->bytes_to_read = 0;
			//nfs->nfs_state = NFS_LOOKUP;
			//nfs_step(nfs);
		}
	}

	return 0;
err_connect:
	free ( nfs->auth_sys.hostname );
err_cred:
	nfs_uri_free ( &nfs->uri );
	free ( nfs->hostname );
err_uri:
	free ( nfs );
	return rc;
}

/**
 * Initiate a NFS connection
 *
 * @v xfer		Data transfer interface
 * @v uri		Uniform Resource Identifier
 * @ret rc		Return status code
 */
static int nfs_open_uri ( struct interface *xfer, struct uri *uri ) {
	int                     rc;
	struct nfs_uri      uri_temp;
	char *hostname_temp;

	rc = nfs_parse_uri( &uri_temp, &hostname_temp,uri );
	if ( rc != 0 )
		goto err_uri;

	nfs_open(xfer,&uri_temp,hostname_temp,NFS_READ_ALL,NULL);

	return 0;
err_uri:
	return rc;
}

/** NFS URI opener */
struct uri_opener nfs_uri_opener __uri_opener = {
	.scheme	= "nfs",
	.open	= nfs_open_uri,
};
