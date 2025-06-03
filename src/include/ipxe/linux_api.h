#ifndef _IPXE_LINUX_API_H
#define _IPXE_LINUX_API_H

/*
 * Copyright (C) 2010 Piotr Jaroszyński <p.jaroszynski@gmail.com>.
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** @file
 *
 * Linux host API
 *
 * This file is included from both the iPXE build environment and the
 * host build environment.
 *
 */

#if __STDC_HOSTED__
#define __asmcall
#define FILE_LICENCE(x)
#endif

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>

#if ! __STDC_HOSTED__
#define __KERNEL_STRICT_NAMES
#include <linux/time.h>
#include <linux/mman.h>
#include <linux/fcntl.h>
#include <linux/ioctl.h>
#include <linux/poll.h>
#include <linux/fs.h>
#define MAP_FAILED ( ( void * ) -1 )
#endif

struct sockaddr;
struct slirp_config;
struct slirp_callbacks;
struct Slirp;

extern int linux_errno;
extern int linux_argc;
extern char **linux_argv;

extern int __asmcall linux_open ( const char *pathname, int flags, ... );
extern int __asmcall linux_close ( int fd );
extern off_t __asmcall linux_lseek ( int fd, off_t offset, int whence );
extern ssize_t __asmcall linux_read ( int fd, void *buf, size_t count );
extern ssize_t __asmcall linux_write ( int fd, const void *buf, size_t count );
extern int __asmcall linux_fcntl ( int fd, int cmd, ... );
extern int __asmcall linux_ioctl ( int fd, unsigned long request, ... );
extern int __asmcall linux_fstat_size ( int fd, size_t *size );
extern int __asmcall linux_poll ( struct pollfd *fds, unsigned int nfds,
				  int timeout );
extern int __asmcall linux_nanosleep ( const struct timespec *req,
				       struct timespec *rem );
extern int __asmcall linux_usleep ( unsigned int usec );
extern int __asmcall linux_gettimeofday ( struct timeval *tv,
					  struct timezone *tz );
extern void * __asmcall linux_mmap ( void *addr, size_t length, int prot,
				     int flags, int fd, off_t offset );
extern void * __asmcall linux_mremap ( void *old_address, size_t old_size,
				       size_t new_size, int flags, ... );
extern int __asmcall linux_munmap ( void *addr, size_t length );
extern int __asmcall linux_socket ( int domain, int type, int protocol );
extern int __asmcall linux_bind ( int sockfd, const struct sockaddr *addr,
				  size_t addrlen );
extern ssize_t __asmcall linux_sendto ( int sockfd, const void *buf,
					size_t len, int flags,
					const struct sockaddr *dest_addr,
					size_t addrlen );
extern const char * __asmcall linux_strerror ( int linux_errno );
extern struct Slirp * __asmcall
linux_slirp_new ( const struct slirp_config *config,
		  const struct slirp_callbacks *callbacks, void *opaque );
extern void __asmcall linux_slirp_cleanup ( struct Slirp *slirp );
extern void __asmcall linux_slirp_input ( struct Slirp *slirp,
					  const uint8_t *pkt, int pkt_len );
extern void __asmcall
linux_slirp_pollfds_fill ( struct Slirp *slirp, uint32_t *timeout,
			   int ( __asmcall * add_poll ) ( int fd, int events,
							  void *opaque ),
			   void *opaque );
extern void __asmcall
linux_slirp_pollfds_poll ( struct Slirp *slirp, int select_error,
			   int ( __asmcall * get_revents ) ( int idx,
							     void *opaque ),
			   void *opaque );

#endif /* _IPXE_LINUX_API_H */
