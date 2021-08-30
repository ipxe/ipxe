/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/**
 * @file
 *
 * EFI file protocols
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <wchar.h>
#include <ipxe/image.h>
#include <ipxe/cpio.h>
#include <ipxe/efi/efi.h>
#include <ipxe/efi/Protocol/SimpleFileSystem.h>
#include <ipxe/efi/Protocol/BlockIo.h>
#include <ipxe/efi/Protocol/DiskIo.h>
#include <ipxe/efi/Guid/FileInfo.h>
#include <ipxe/efi/Guid/FileSystemInfo.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/efi/efi_file.h>

/** EFI media ID */
#define EFI_MEDIA_ID_MAGIC 0x69505845

/** An EFI virtual file reader */
struct efi_file_reader {
	/** EFI file */
	struct efi_file *file;
	/** Position within virtual file */
	size_t pos;
	/** Output data buffer */
	void *data;
	/** Length of output data buffer */
	size_t len;
};

/** An EFI file */
struct efi_file {
	/** Reference count */
	struct refcnt refcnt;
	/** EFI file protocol */
	EFI_FILE_PROTOCOL file;
	/** Image (if any) */
	struct image *image;
	/** Filename */
	const char *name;
	/** Current file position */
	size_t pos;
	/**
	 * Read from file
	 *
	 * @v reader		File reader
	 * @ret len		Length read
	 */
	size_t ( * read ) ( struct efi_file_reader *reader );
};

static struct efi_file efi_file_root;
static struct efi_file efi_file_initrd;

/**
 * Free EFI file
 *
 * @v refcnt		Reference count
 */
static void efi_file_free ( struct refcnt *refcnt ) {
	struct efi_file *file =
		container_of ( refcnt, struct efi_file, refcnt );

	image_put ( file->image );
	free ( file );
}

/**
 * Get EFI file name (for debugging)
 *
 * @v file		EFI file
 * @ret name		Name
 */
static const char * efi_file_name ( struct efi_file *file ) {

	return ( file == &efi_file_root ? "<root>" : file->name );
}

/**
 * Find EFI file image
 *
 * @v name		Filename
 * @ret image		Image, or NULL
 */
static struct image * efi_file_find ( const char *name ) {
	struct image *image;

	/* Find image */
	list_for_each_entry ( image, &images, list ) {
		if ( strcasecmp ( image->name, name ) == 0 )
			return image;
	}

	return NULL;
}

/**
 * Get length of EFI file
 *
 * @v file		EFI file
 * @ret len		Length of file
 */
static size_t efi_file_len ( struct efi_file *file ) {
	struct efi_file_reader reader;

	/* If this is the root directory, then treat as length zero */
	if ( ! file->read )
		return 0;

	/* Initialise reader */
	reader.file = file;
	reader.pos = 0;
	reader.data = NULL;
	reader.len = 0;

	/* Perform dummy read to determine file length */
	file->read ( &reader );

	return reader.pos;
}

/**
 * Read chunk of EFI file
 *
 * @v reader		EFI file reader
 * @v data		Input data, or UNULL to zero-fill
 * @v len		Length of input data
 * @ret len		Length of output data
 */
static size_t efi_file_read_chunk ( struct efi_file_reader *reader,
				    userptr_t data, size_t len ) {
	struct efi_file *file = reader->file;
	size_t offset;

	/* Calculate offset into input data */
	offset = ( file->pos - reader->pos );

	/* Consume input data range */
	reader->pos += len;

	/* Calculate output length */
	if ( offset < len ) {
		len -= offset;
	} else {
		len = 0;
	}
	if ( len > reader->len )
		len = reader->len;

	/* Copy or zero output data */
	if ( data ) {
		copy_from_user ( reader->data, data, offset, len );
	} else {
		memset ( reader->data, 0, len );
	}

	/* Consume output buffer */
	file->pos += len;
	reader->data += len;
	reader->len -= len;

	return len;
}

/**
 * Read from image-backed file
 *
 * @v reader		EFI file reader
 * @ret len		Length read
 */
static size_t efi_file_read_image ( struct efi_file_reader *reader ) {
	struct efi_file *file = reader->file;
	struct image *image = file->image;

	/* Read from file */
	return efi_file_read_chunk ( reader, image->data, image->len );
}

/**
 * Read from magic initrd file
 *
 * @v reader		EFI file reader
 * @ret len		Length read
 */
static size_t efi_file_read_initrd ( struct efi_file_reader *reader ) {
	struct efi_file *file = reader->file;
	struct cpio_header cpio;
	struct image *image;
	const char *name;
	size_t pad_len;
	size_t cpio_len;
	size_t name_len;
	size_t len;

	/* Read from file */
	len = 0;
	for_each_image ( image ) {

		/* Ignore currently executing image */
		if ( image == current_image )
			continue;

		/* Pad to alignment boundary */
		pad_len = ( ( -reader->pos ) & ( INITRD_ALIGN - 1 ) );
		if ( pad_len ) {
			DBGC ( file, "EFIFILE %s [%#08zx,%#08zx) pad\n",
			       efi_file_name ( file ), reader->pos,
			       ( reader->pos + pad_len ) );
		}
		len += efi_file_read_chunk ( reader, UNULL, pad_len );

		/* Read CPIO header, if applicable */
		cpio_len = cpio_header ( image, &cpio );
		if ( cpio_len ) {
			name = cpio_name ( image );
			name_len = cpio_name_len ( image );
			pad_len = ( cpio_len - sizeof ( cpio ) - name_len );
			DBGC ( file, "EFIFILE %s [%#08zx,%#08zx) %s header\n",
			       efi_file_name ( file ), reader->pos,
			       ( reader->pos + cpio_len ), image->name );
			len += efi_file_read_chunk ( reader,
						     virt_to_user ( &cpio ),
						     sizeof ( cpio ) );
			len += efi_file_read_chunk ( reader,
						     virt_to_user ( name ),
						     name_len );
			len += efi_file_read_chunk ( reader, UNULL, pad_len );
		}

		/* Read file data */
		DBGC ( file, "EFIFILE %s [%#08zx,%#08zx) %s\n",
		       efi_file_name ( file ), reader->pos,
		       ( reader->pos + image->len ), image->name );
		len += efi_file_read_chunk ( reader, image->data, image->len );
	}

	return len;
}

/**
 * Open fixed file
 *
 * @v file		EFI file
 * @v new		New EFI file
 * @ret efirc		EFI status code
 */
static EFI_STATUS efi_file_open_fixed ( struct efi_file *file,
					EFI_FILE_PROTOCOL **new ) {

	/* Increment reference count */
	ref_get ( &file->refcnt );

	/* Return opened file */
	*new = &file->file;

	DBGC ( file, "EFIFILE %s opened\n", efi_file_name ( file ) );
	return 0;
}

/**
 * Associate file with image
 *
 * @v file		EFI file
 * @v image		Image
 */
static void efi_file_image ( struct efi_file *file, struct image *image ) {

	file->image = image;
	file->name = image->name;
	file->read = efi_file_read_image;
}

/**
 * Open file
 *
 * @v this		EFI file
 * @ret new		New EFI file
 * @v wname		Filename
 * @v mode		File mode
 * @v attributes	File attributes (for newly-created files)
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_file_open ( EFI_FILE_PROTOCOL *this, EFI_FILE_PROTOCOL **new,
		CHAR16 *wname, UINT64 mode, UINT64 attributes __unused ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );
	char buf[ wcslen ( wname ) + 1 /* NUL */ ];
	struct efi_file *new_file;
	struct image *image;
	char *name;

	/* Convert name to ASCII */
	snprintf ( buf, sizeof ( buf ), "%ls", wname );
	name = buf;

	/* Initial '\' indicates opening from the root directory */
	while ( *name == '\\' ) {
		file = &efi_file_root;
		name++;
	}

	/* Allow root directory itself to be opened */
	if ( ( name[0] == '\0' ) || ( name[0] == '.' ) )
		return efi_file_open_fixed ( &efi_file_root, new );

	/* Fail unless opening from the root */
	if ( file != &efi_file_root ) {
		DBGC ( file, "EFIFILE %s is not a directory\n",
		       efi_file_name ( file ) );
		return EFI_NOT_FOUND;
	}

	/* Fail unless opening read-only */
	if ( mode != EFI_FILE_MODE_READ ) {
		DBGC ( file, "EFIFILE %s cannot be opened in mode %#08llx\n",
		       name, mode );
		return EFI_WRITE_PROTECTED;
	}

	/* Allow magic initrd to be opened */
	if ( strcasecmp ( name, efi_file_initrd.name ) == 0 )
		return efi_file_open_fixed ( &efi_file_initrd, new );

	/* Identify image */
	image = efi_file_find ( name );
	if ( ! image ) {
		DBGC ( file, "EFIFILE %s does not exist\n", name );
		return EFI_NOT_FOUND;
	}

	/* Allocate and initialise file */
	new_file = zalloc ( sizeof ( *new_file ) );
	if ( ! new_file )
		return EFI_OUT_OF_RESOURCES;
	ref_init ( &file->refcnt, efi_file_free );
	memcpy ( &new_file->file, &efi_file_root.file,
		 sizeof ( new_file->file ) );
	efi_file_image ( new_file, image_get ( image ) );
	*new = &new_file->file;
	DBGC ( new_file, "EFIFILE %s opened\n", efi_file_name ( new_file ) );

	return 0;
}

/**
 * Close file
 *
 * @v this		EFI file
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_file_close ( EFI_FILE_PROTOCOL *this ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );

	/* Close file */
	DBGC ( file, "EFIFILE %s closed\n", efi_file_name ( file ) );
	ref_put ( &file->refcnt );

	return 0;
}

/**
 * Close and delete file
 *
 * @v this		EFI file
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_file_delete ( EFI_FILE_PROTOCOL *this ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );

	DBGC ( file, "EFIFILE %s cannot be deleted\n", efi_file_name ( file ) );

	/* Close file */
	efi_file_close ( this );

	/* Warn of failure to delete */
	return EFI_WARN_DELETE_FAILURE;
}

/**
 * Return variable-length data structure
 *
 * @v base		Base data structure (starting with UINT64)
 * @v base_len		Length of base data structure
 * @v name		Name to append to base data structure
 * @v len		Length of data buffer
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS efi_file_varlen ( UINT64 *base, size_t base_len,
				    const char *name, UINTN *len, VOID *data ) {
	size_t name_len;

	/* Calculate structure length */
	name_len = strlen ( name );
	*base = ( base_len + ( name_len + 1 /* NUL */ ) * sizeof ( wchar_t ) );
	if ( *len < *base ) {
		*len = *base;
		return EFI_BUFFER_TOO_SMALL;
	}

	/* Copy data to buffer */
	*len = *base;
	memcpy ( data, base, base_len );
	efi_snprintf ( ( data + base_len ), ( name_len + 1 /* NUL */ ),
		       "%s", name );

	return 0;
}

/**
 * Return file information structure
 *
 * @v file		EFI file
 * @v len		Length of data buffer
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS efi_file_info ( struct efi_file *file, UINTN *len,
				  VOID *data ) {
	EFI_FILE_INFO info;
	size_t file_len;

	/* Get file length */
	file_len = efi_file_len ( file );

	/* Populate file information */
	memset ( &info, 0, sizeof ( info ) );
	info.FileSize = file_len;
	info.PhysicalSize = file_len;
	info.Attribute = EFI_FILE_READ_ONLY;
	if ( file == &efi_file_root )
		info.Attribute |= EFI_FILE_DIRECTORY;

	return efi_file_varlen ( &info.Size, SIZE_OF_EFI_FILE_INFO,
				 file->name, len, data );
}

/**
 * Read directory entry
 *
 * @v file		EFI file
 * @v len		Length to read
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS efi_file_read_dir ( struct efi_file *file, UINTN *len,
				      VOID *data ) {
	EFI_STATUS efirc;
	struct efi_file entry;
	struct image *image;
	unsigned int index;

	/* Construct directory entries for image-backed files */
	index = file->pos;
	for_each_image ( image ) {
		if ( index-- == 0 ) {
			efi_file_image ( &entry, image );
			efirc = efi_file_info ( &entry, len, data );
			if ( efirc == 0 )
				file->pos++;
			return efirc;
		}
	}

	/* No more entries */
	*len = 0;
	return 0;
}

/**
 * Read from file
 *
 * @v this		EFI file
 * @v len		Length to read
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_file_read ( EFI_FILE_PROTOCOL *this,
					 UINTN *len, VOID *data ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );
	struct efi_file_reader reader;
	size_t pos = file->pos;

	/* If this is the root directory, then construct a directory entry */
	if ( ! file->read )
		return efi_file_read_dir ( file, len, data );

	/* Initialise reader */
	reader.file = file;
	reader.pos = 0;
	reader.data = data;
	reader.len = *len;

	/* Read from the file */
	DBGC ( file, "EFIFILE %s read [%#08zx,%#08zx)\n",
	       efi_file_name ( file ), pos, file->pos );
	*len = file->read ( &reader );
	assert ( ( pos + *len ) == file->pos );

	return 0;
}

/**
 * Write to file
 *
 * @v this		EFI file
 * @v len		Length to write
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_file_write ( EFI_FILE_PROTOCOL *this,
					  UINTN *len, VOID *data __unused ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );

	DBGC ( file, "EFIFILE %s cannot write [%#08zx, %#08zx)\n",
	       efi_file_name ( file ), file->pos,
	       ( ( size_t ) ( file->pos + *len ) ) );
	return EFI_WRITE_PROTECTED;
}

/**
 * Set file position
 *
 * @v this		EFI file
 * @v position		New file position
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_file_set_position ( EFI_FILE_PROTOCOL *this,
						 UINT64 position ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );
	size_t len;

	/* Get file length */
	len = efi_file_len ( file );

	/* Check for the magic end-of-file value */
	if ( position == 0xffffffffffffffffULL )
		position = len;

	/* Fail if we attempt to seek past the end of the file (since
	 * we do not support writes).
	 */
	if ( position > len ) {
		DBGC ( file, "EFIFILE %s cannot seek to %#08llx of %#08zx\n",
		       efi_file_name ( file ), position, len );
		return EFI_UNSUPPORTED;
	}

	/* Set position */
	file->pos = position;
	DBGC ( file, "EFIFILE %s position set to %#08zx\n",
	       efi_file_name ( file ), file->pos );

	return 0;
}

/**
 * Get file position
 *
 * @v this		EFI file
 * @ret position	New file position
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_file_get_position ( EFI_FILE_PROTOCOL *this,
						 UINT64 *position ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );

	*position = file->pos;
	return 0;
}

/**
 * Get file information
 *
 * @v this		EFI file
 * @v type		Type of information
 * @v len		Buffer size
 * @v data		Buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_file_get_info ( EFI_FILE_PROTOCOL *this,
					     EFI_GUID *type,
					     UINTN *len, VOID *data ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );
	EFI_FILE_SYSTEM_INFO fsinfo;
	struct image *image;

	/* Determine information to return */
	if ( memcmp ( type, &efi_file_info_id, sizeof ( *type ) ) == 0 ) {

		/* Get file information */
		DBGC ( file, "EFIFILE %s get file information\n",
		       efi_file_name ( file ) );
		return efi_file_info ( file, len, data );

	} else if ( memcmp ( type, &efi_file_system_info_id,
			     sizeof ( *type ) ) == 0 ) {

		/* Get file system information */
		DBGC ( file, "EFIFILE %s get file system information\n",
		       efi_file_name ( file ) );
		memset ( &fsinfo, 0, sizeof ( fsinfo ) );
		fsinfo.ReadOnly = 1;
		for_each_image ( image )
			fsinfo.VolumeSize += image->len;
		return efi_file_varlen ( &fsinfo.Size,
					 SIZE_OF_EFI_FILE_SYSTEM_INFO, "iPXE",
					 len, data );
	} else {

		DBGC ( file, "EFIFILE %s cannot get information of type %s\n",
		       efi_file_name ( file ), efi_guid_ntoa ( type ) );
		return EFI_UNSUPPORTED;
	}
}

/**
 * Set file information
 *
 * @v this		EFI file
 * @v type		Type of information
 * @v len		Buffer size
 * @v data		Buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_file_set_info ( EFI_FILE_PROTOCOL *this, EFI_GUID *type,
		    UINTN len __unused, VOID *data __unused ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );

	DBGC ( file, "EFIFILE %s cannot set information of type %s\n",
	       efi_file_name ( file ), efi_guid_ntoa ( type ) );
	return EFI_WRITE_PROTECTED;
}

/**
 * Flush file modified data
 *
 * @v this		EFI file
 * @v type		Type of information
 * @v len		Buffer size
 * @v data		Buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI efi_file_flush ( EFI_FILE_PROTOCOL *this ) {
	struct efi_file *file = container_of ( this, struct efi_file, file );

	DBGC ( file, "EFIFILE %s flushed\n", efi_file_name ( file ) );
	return 0;
}

/** Root directory */
static struct efi_file efi_file_root = {
	.refcnt = REF_INIT ( ref_no_free ),
	.file = {
		.Revision = EFI_FILE_PROTOCOL_REVISION,
		.Open = efi_file_open,
		.Close = efi_file_close,
		.Delete = efi_file_delete,
		.Read = efi_file_read,
		.Write = efi_file_write,
		.GetPosition = efi_file_get_position,
		.SetPosition = efi_file_set_position,
		.GetInfo = efi_file_get_info,
		.SetInfo = efi_file_set_info,
		.Flush = efi_file_flush,
	},
	.image = NULL,
	.name = "",
};

/** Magic initrd file */
static struct efi_file efi_file_initrd = {
	.refcnt = REF_INIT ( ref_no_free ),
	.file = {
		.Revision = EFI_FILE_PROTOCOL_REVISION,
		.Open = efi_file_open,
		.Close = efi_file_close,
		.Delete = efi_file_delete,
		.Read = efi_file_read,
		.Write = efi_file_write,
		.GetPosition = efi_file_get_position,
		.SetPosition = efi_file_set_position,
		.GetInfo = efi_file_get_info,
		.SetInfo = efi_file_set_info,
		.Flush = efi_file_flush,
	},
	.image = NULL,
	.name = "initrd.magic",
	.read = efi_file_read_initrd,
};

/**
 * Open root directory
 *
 * @v filesystem	EFI simple file system
 * @ret file		EFI file handle
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_file_open_volume ( EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *filesystem __unused,
		       EFI_FILE_PROTOCOL **file ) {

	DBGC ( &efi_file_root, "EFIFILE open volume\n" );
	return efi_file_open_fixed ( &efi_file_root, file );
}

/** EFI simple file system protocol */
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL efi_simple_file_system_protocol = {
	.Revision = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION,
	.OpenVolume = efi_file_open_volume,
};

/** Dummy block I/O reset */
static EFI_STATUS EFIAPI
efi_block_io_reset ( EFI_BLOCK_IO_PROTOCOL *this __unused, BOOLEAN extended ) {

	DBGC ( &efi_file_root, "EFIFILE block %sreset\n",
	       ( extended ? "extended " : "" ) );
	return 0;
}

/** Dummy block I/O read */
static EFI_STATUS EFIAPI
efi_block_io_read_blocks ( EFI_BLOCK_IO_PROTOCOL *this __unused, UINT32 MediaId,
			   EFI_LBA lba, UINTN len, VOID *data ) {

	DBGC ( &efi_file_root, "EFIFILE block read ID %#08x LBA %#08llx -> "
	       "%p+%zx\n", MediaId, ( ( unsigned long long ) lba ),
	       data, ( ( size_t ) len ) );
	return EFI_NO_MEDIA;
}

/** Dummy block I/O write */
static EFI_STATUS EFIAPI
efi_block_io_write_blocks ( EFI_BLOCK_IO_PROTOCOL *this __unused,
			    UINT32 MediaId, EFI_LBA lba, UINTN len,
			    VOID *data ) {

	DBGC ( &efi_file_root, "EFIFILE block write ID %#08x LBA %#08llx <- "
	       "%p+%zx\n", MediaId, ( ( unsigned long long ) lba ),
	       data, ( ( size_t ) len ) );
	return EFI_NO_MEDIA;
}

/** Dummy block I/O flush */
static EFI_STATUS EFIAPI
efi_block_io_flush_blocks ( EFI_BLOCK_IO_PROTOCOL *this __unused ) {

	DBGC ( &efi_file_root, "EFIFILE block flush\n" );
	return 0;
}

/** Dummy block I/O media */
static EFI_BLOCK_IO_MEDIA efi_block_io_media = {
	.MediaId = EFI_MEDIA_ID_MAGIC,
	.MediaPresent = TRUE,
	.ReadOnly = TRUE,
	.BlockSize = 1,
};

/** Dummy EFI block I/O protocol */
static EFI_BLOCK_IO_PROTOCOL efi_block_io_protocol = {
	.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION,
	.Media = &efi_block_io_media,
	.Reset = efi_block_io_reset,
	.ReadBlocks = efi_block_io_read_blocks,
	.WriteBlocks = efi_block_io_write_blocks,
	.FlushBlocks = efi_block_io_flush_blocks,
};

/** Dummy disk I/O read */
static EFI_STATUS EFIAPI
efi_disk_io_read_disk ( EFI_DISK_IO_PROTOCOL *this __unused, UINT32 MediaId,
			UINT64 offset, UINTN len, VOID *data ) {

	DBGC ( &efi_file_root, "EFIFILE disk read ID %#08x offset %#08llx -> "
	       "%p+%zx\n", MediaId, ( ( unsigned long long ) offset ),
	       data, ( ( size_t ) len ) );
	return EFI_NO_MEDIA;
}

/** Dummy disk I/O write */
static EFI_STATUS EFIAPI
efi_disk_io_write_disk ( EFI_DISK_IO_PROTOCOL *this __unused, UINT32 MediaId,
			 UINT64 offset, UINTN len, VOID *data ) {

	DBGC ( &efi_file_root, "EFIFILE disk write ID %#08x offset %#08llx <- "
	       "%p+%zx\n", MediaId, ( ( unsigned long long ) offset ),
	       data, ( ( size_t ) len ) );
	return EFI_NO_MEDIA;
}

/** Dummy EFI disk I/O protocol */
static EFI_DISK_IO_PROTOCOL efi_disk_io_protocol = {
	.Revision = EFI_DISK_IO_PROTOCOL_REVISION,
	.ReadDisk = efi_disk_io_read_disk,
	.WriteDisk = efi_disk_io_write_disk,
};

/**
 * Install EFI simple file system protocol
 *
 * @v handle		EFI handle
 * @ret rc		Return status code
 */
int efi_file_install ( EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_DISK_IO_PROTOCOL *diskio;
		void *interface;
	} diskio;
	EFI_STATUS efirc;
	int rc;

	/* Reset root directory state */
	efi_file_root.pos = 0;

	/* Install the simple file system protocol, block I/O
	 * protocol, and disk I/O protocol.  We don't have a block
	 * device, but large parts of the EDK2 codebase make the
	 * assumption that file systems are normally attached to block
	 * devices, and so we create a dummy block device on the same
	 * handle just to keep things looking normal.
	 */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
			&handle,
			&efi_block_io_protocol_guid,
			&efi_block_io_protocol,
			&efi_disk_io_protocol_guid,
			&efi_disk_io_protocol,
			&efi_simple_file_system_protocol_guid,
			&efi_simple_file_system_protocol, NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( handle, "Could not install simple file system "
		       "protocols: %s\n", strerror ( rc ) );
		goto err_install;
	}

	/* The FAT filesystem driver has a bug: if a block device
	 * contains no FAT filesystem but does have an
	 * EFI_SIMPLE_FILE_SYSTEM_PROTOCOL instance, the FAT driver
	 * will assume that it must have previously installed the
	 * EFI_SIMPLE_FILE_SYSTEM_PROTOCOL.  This causes the FAT
	 * driver to claim control of our device, and to refuse to
	 * stop driving it, which prevents us from later uninstalling
	 * correctly.
	 *
	 * Work around this bug by opening the disk I/O protocol
	 * ourselves, thereby preventing the FAT driver from opening
	 * it.
	 *
	 * Note that the alternative approach of opening the block I/O
	 * protocol (and thereby in theory preventing DiskIo from
	 * attaching to the block I/O protocol) causes an endless loop
	 * of calls to our DRIVER_STOP method when starting the EFI
	 * shell.  I have no idea why this is.
	 */
	if ( ( efirc = bs->OpenProtocol ( handle, &efi_disk_io_protocol_guid,
					  &diskio.interface, efi_image_handle,
					  handle,
					  EFI_OPEN_PROTOCOL_BY_DRIVER ) ) != 0){
		rc = -EEFI ( efirc );
		DBGC ( handle, "Could not open disk I/O protocol: %s\n",
		       strerror ( rc ) );
		DBGC_EFI_OPENERS ( handle, handle, &efi_disk_io_protocol_guid );
		goto err_open;
	}
	assert ( diskio.diskio == &efi_disk_io_protocol );

	return 0;

	bs->CloseProtocol ( handle, &efi_disk_io_protocol_guid,
			    efi_image_handle, handle );
 err_open:
	bs->UninstallMultipleProtocolInterfaces (
			handle,
			&efi_simple_file_system_protocol_guid,
			&efi_simple_file_system_protocol,
			&efi_disk_io_protocol_guid,
			&efi_disk_io_protocol,
			&efi_block_io_protocol_guid,
			&efi_block_io_protocol, NULL );
 err_install:
	return rc;
}

/**
 * Uninstall EFI simple file system protocol
 *
 * @v handle		EFI handle
 */
void efi_file_uninstall ( EFI_HANDLE handle ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;
	int rc;

	/* Close our own disk I/O protocol */
	bs->CloseProtocol ( handle, &efi_disk_io_protocol_guid,
			    efi_image_handle, handle );

	/* We must install the file system protocol first, since
	 * otherwise the EDK2 code will attempt to helpfully uninstall
	 * it when the block I/O protocol is uninstalled, leading to a
	 * system lock-up.
	 */
	if ( ( efirc = bs->UninstallMultipleProtocolInterfaces (
			handle,
			&efi_simple_file_system_protocol_guid,
			&efi_simple_file_system_protocol,
			&efi_disk_io_protocol_guid,
			&efi_disk_io_protocol,
			&efi_block_io_protocol_guid,
			&efi_block_io_protocol, NULL ) ) != 0 ) {
		rc = -EEFI ( efirc );
		DBGC ( handle, "Could not uninstall simple file system "
		       "protocols: %s\n", strerror ( rc ) );
		/* Oh dear */
	}
}
