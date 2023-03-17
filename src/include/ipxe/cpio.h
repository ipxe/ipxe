#pragma once

#ifndef _IPXE_CPIO_H
    #define _IPXE_CPIO_H

/** @file
 *
 * CPIO archives
 *
 */

FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

    #include <ipxe/image.h>

/** A CPIO archive header
 *
 * All field are hexadecimal ASCII numbers padded with '0' on the
 * left to the full width of the field.
 */
struct cpio_header {
    /** The string "070701" or "070702" */
    char c_magic[6];
    /** File inode number */
    char c_ino[8];
    /** File mode and permissions */
    char c_mode[8];
    /** File uid */
    char c_uid[8];
    /** File gid */
    char c_gid[8];
    /** Number of links */
    char c_nlink[8];
    /** Modification time */
    char c_mtime[8];
    /** Size of data field */
    char c_filesize[8];
    /** Major part of file device number */
    char c_maj[8];
    /** Minor part of file device number */
    char c_min[8];
    /** Major part of device node reference */
    char c_rmaj[8];
    /** Minor part of device node reference */
    char c_rmin[8];
    /** Length of filename, including final NUL */
    char c_namesize[8];
    /** Checksum of data field if c_magic is 070702, othersize zero */
    char c_chksum[8];
} __attribute__((packed));

    /** CPIO magic */
    #define CPIO_MAGIC "070701"

    /** CPIO header length alignment */
    #define CPIO_ALIGN 4

    /** Alignment for CPIO archives within an initrd */
    #define INITRD_ALIGN 4096

/**
 * Get CPIO image name
 *
 * @v image		Image
 * @ret name		Image name (not NUL terminated)
 */
static inline __attribute__((always_inline)) const char*
cpio_name(struct image* image) {
    return image->cmdline;
}

extern void cpio_set_field(char* field, unsigned long value);
extern size_t cpio_name_len(struct image* image);
extern size_t cpio_header(struct image* image, struct cpio_header* cpio);

#endif /* _IPXE_CPIO_H */
