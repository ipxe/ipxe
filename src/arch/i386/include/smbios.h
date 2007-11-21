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
	uint8_t length;
	/** Handle */
	uint16_t handle;
} __attribute__ (( packed ));

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

struct smbios_strings;
extern int find_smbios_structure ( unsigned int type,
				   void *structure, size_t length,
				   struct smbios_strings *strings );
extern int find_smbios_string ( struct smbios_strings *strings,
				unsigned int index,
				char *buffer, size_t length );

#endif /* _SMBIOS_H */
