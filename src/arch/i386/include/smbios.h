#ifndef _SMBIOS_H
#define _SMBIOS_H

/** @file
 *
 * System Management BIOS
 */

#include <stdint.h>

/** An SMBIOS structure header */
struct smbios_header {
	/** Type */
	uint8_t type;
	/** Length */
	uint8_t len;
	/** Handle */
	uint16_t handle;
} __attribute__ (( packed ));

/** SMBIOS structure descriptor */
struct smbios_structure {
	/** Copy of SMBIOS structure header */
	struct smbios_header header;
	/** Offset of structure within SMBIOS */
	size_t offset;
	/** Length of strings section */
	size_t strings_len;
};

/** SMBIOS system information structure */
struct smbios_system_information {
	/** SMBIOS structure header */
	struct smbios_header header;
	/** Manufacturer string */
	uint8_t manufacturer;
	/** Product string */
	uint8_t product;
	/** Version string */
	uint8_t version;
	/** Serial number string */
	uint8_t serial;
	/** UUID */
	uint8_t uuid[16];
	/** Wake-up type */
	uint8_t wakeup;
} __attribute__ (( packed ));

/** SMBIOS system information structure type */
#define SMBIOS_TYPE_SYSTEM_INFORMATION 1

extern int find_smbios_structure ( unsigned int type,
				   struct smbios_structure *structure );
extern int read_smbios_structure ( struct smbios_structure *structure,
				   void *data, size_t len );
extern int read_smbios_string ( struct smbios_structure *structure,
				unsigned int index,
				void *data, size_t len );

#endif /* _SMBIOS_H */
