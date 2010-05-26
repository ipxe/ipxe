/*
 * Copyright (C) 2010 Piotr Jaroszyński <p.jaroszynski@gmail.com>
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

FILE_LICENCE(GPL2_OR_LATER);

/** @file
 *
 * iPXE user memory allocation API for linux
 *
 */

#include <assert.h>
#include <ipxe/umalloc.h>

#include <linux_api.h>

/** Special address returned for empty allocations */
#define NOWHERE ((void *)-1)

/** Poison to make the metadata more unique */
#define POISON 0xa5a5a5a5
#define min(a,b) (((a)<(b))?(a):(b))

/** Metadata stored at the beginning of all allocations */
struct metadata
{
	unsigned poison;
	size_t size;
};

#define SIZE_MD (sizeof(struct metadata))

/** Simple realloc which passes most of the work to mmap(), mremap() and munmap() */
static void * linux_realloc(void *ptr, size_t size)
{
	struct metadata md = {0, 0};
	struct metadata * mdptr = NULL;

	DBG2("linux_realloc(%p, %zd)\n", ptr, size);

	/* Check whether we have a valid pointer */
	if (ptr != NULL && ptr != NOWHERE) {
		mdptr = ptr - SIZE_MD;
		md = *mdptr;

		/* Check for poison in the metadata */
		if (md.poison != POISON) {
			DBG("linux_realloc bad poison: 0x%x (expected 0x%x)\n", md.poison, POISON);
			return NULL;
		}
	} else {
		/* Handle NOWHERE as NULL */
		ptr = NULL;
	}

	/*
	 * At this point, ptr is either NULL or pointing to a region allocated by us.
	 * In the latter case mdptr is pointing to a valid metadata, otherwise it is NULL.
	 */

	/* Handle deallocation or allocation of size 0 */
	if (size == 0) {
		if (mdptr) {
			if (linux_munmap(mdptr, md.size))
				DBG("linux_realloc munmap failed: %s\n", linux_strerror(linux_errno));
		}
		return NOWHERE;
	}

	if (ptr) {
		/* ptr is pointing to an already allocated memory, mremap() it with new size */
		mdptr = linux_mremap(mdptr, md.size + SIZE_MD, size + SIZE_MD, MREMAP_MAYMOVE);
		if (mdptr == MAP_FAILED) {
			DBG("linux_realloc mremap failed: %s\n", linux_strerror(linux_errno));
			return NULL;
		}

		ptr = ((void *)mdptr) + SIZE_MD;
	} else {
		/* allocate new memory with mmap() */
		mdptr = linux_mmap(NULL, size + SIZE_MD, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (mdptr == MAP_FAILED) {
			DBG("linux_realloc mmap failed: %s\n", linux_strerror(linux_errno));
			return NULL;
		}
		ptr = ((void *)mdptr) + SIZE_MD;
	}

	/* Update the metadata */
	mdptr->poison = POISON;
	mdptr->size = size;

	return ptr;
}

/**
 * Reallocate external memory
 *
 * @v old_ptr		Memory previously allocated by umalloc(), or UNULL
 * @v new_size		Requested size
 * @ret new_ptr		Allocated memory, or UNULL
 *
 * Calling realloc() with a new size of zero is a valid way to free a
 * memory block.
 */
static userptr_t linux_urealloc(userptr_t old_ptr, size_t new_size)
{
	return (userptr_t)linux_realloc((void *)old_ptr, new_size);
}

PROVIDE_UMALLOC(linux, urealloc, linux_urealloc);
