#ifndef _IPXE_UCODE_H
#define _IPXE_UCODE_H

/** @file
 *
 * Microcode updates
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/mp.h>

/** Platform ID MSR */
#define MSR_PLATFORM_ID 0x00000017UL

/** Extract platform ID from MSR value */
#define MSR_PLATFORM_ID_VALUE( value ) ( ( (value) >> 50 ) & 0x7 )

/** Intel microcode load trigger MSR */
#define MSR_UCODE_TRIGGER_INTEL 0x00000079UL

/** AMD microcode load trigger MSR */
#define MSR_UCODE_TRIGGER_AMD 0xc0010020UL

/** CPUID signature applicability mask
 *
 * We assume that only steppings may vary between the boot CPU and any
 * application processors.
 */
#define UCODE_SIGNATURE_MASK 0xfffffff0UL

/** Minimum possible microcode version */
#define UCODE_VERSION_MIN -0x80000000L

/** Maximum possible microcode version */
#define UCODE_VERSION_MAX 0x7fffffffL

/** A microcode update control
 *
 * This must match the layout as used by the assembly code in
 * ucode_mp.S.
 */
struct ucode_control {
	/** Microcode descriptor list physical address */
	uint64_t desc;
	/** Microcode status array physical address */
	uint64_t status;
	/** Microcode load trigger MSR */
	uint32_t trigger_msr;
	/** Maximum expected APIC ID */
	uint32_t apic_max;
	/** Unexpected APIC ID
	 *
	 * Any application processor may set this to indicate that its
	 * APIC ID was higher than the maximum expected APIC ID.
	 */
	uint32_t apic_unexpected;
	/** APIC ID eligibility mask bits */
	uint32_t apic_mask;
	/** APIC ID eligibility test bits */
	uint32_t apic_test;
	/** Microcode version requires manual clear */
	uint8_t ver_clear;
	/** Microcode version is reported via high dword */
	uint8_t ver_high;
} __attribute__ (( packed ));

/** A microcode update descriptor
 *
 * This must match the layout as used by the assembly code in
 * ucode_mp.S.
 */
struct ucode_descriptor {
	/** CPUID signature (or 0 to terminate list) */
	uint32_t signature;
	/** Microcode version */
	int32_t version;
	/** Microcode physical address */
	uint64_t address;
} __attribute__ (( packed ));

/** A microcode update status report
 *
 * This must match the layout as used by the assembly code in
 * ucode_mp.S.
 */
struct ucode_status {
	/** CPU signature */
	uint32_t signature;
	/** APIC ID (for sanity checking) */
	uint32_t id;
	/** Initial microcode version */
	int32_t before;
	/** Final microcode version */
	int32_t after;
} __attribute__ (( packed ));

/** A microcode date */
struct ucode_date {
	/** Year (BCD) */
	uint8_t year;
	/** Century (BCD) */
	uint8_t century;
	/** Day (BCD) */
	uint8_t day;
	/** Month (BCD) */
	uint8_t month;
} __attribute__ (( packed ));

/** An Intel microcode update file header */
struct intel_ucode_header {
	/** Header version number */
	uint32_t hver;
	/** Microcode version */
	int32_t version;
	/** Date */
	struct ucode_date date;
	/** CPUID signature */
	uint32_t signature;
	/** Checksum */
	uint32_t checksum;
	/** Loader version */
	uint32_t lver;
	/** Supported platforms */
	uint32_t platforms;
	/** Microcode data size (or 0 to indicate 2000 bytes) */
	uint32_t data_len;
	/** Total size (or 0 to indicate 2048 bytes) */
	uint32_t len;
	/** Reserved */
	uint8_t reserved[12];
} __attribute__ (( packed ));

/** Intel microcode header version number */
#define INTEL_UCODE_HVER 0x00000001UL

/** Intel microcode loader version number */
#define INTEL_UCODE_LVER 0x00000001UL

/** Intel microcode default data length */
#define INTEL_UCODE_DATA_LEN 2000

/** Intel microcode file alignment */
#define INTEL_UCODE_ALIGN 1024

/** An Intel microcode update file extended header */
struct intel_ucode_ext_header {
	/** Extended signature count */
	uint32_t count;
	/** Extended checksum */
	uint32_t checksum;
	/** Reserved */
	uint8_t reserved[12];
} __attribute__ (( packed ));

/** An Intel microcode extended signature */
struct intel_ucode_ext {
	/** CPUID signature */
	uint32_t signature;
	/** Supported platforms */
	uint32_t platforms;
	/** Checksum */
	uint32_t checksum;
} __attribute__ (( packed ));

/** An AMD microcode update file header */
struct amd_ucode_header {
	/** Magic signature */
	uint32_t magic;
	/** Equivalence table type */
	uint32_t type;
	/** Equivalence table length */
	uint32_t len;
} __attribute__ (( packed ));

/** AMD microcode magic signature */
#define AMD_UCODE_MAGIC ( ( 'A' << 16 ) | ( 'M' << 8 ) | ( 'D' << 0 ) )

/** AMD microcode equivalence table type */
#define AMD_UCODE_EQUIV_TYPE 0x00000000UL

/** An AMD microcode equivalence table entry */
struct amd_ucode_equivalence {
	/** CPU signature */
	uint32_t signature;
	/** Reserved */
	uint8_t reserved_a[8];
	/** Equivalence ID */
	uint16_t id;
	/** Reserved */
	uint8_t reserved_b[2];
} __attribute__ (( packed ));

/** An AMD microcode patch header */
struct amd_ucode_patch_header {
	/** Patch type */
	uint32_t type;
	/** Patch length */
	uint32_t len;
} __attribute__ (( packed ));

/** An AMD microcode patch */
struct amd_ucode_patch {
	/** Date */
	struct ucode_date date;
	/** Microcode version */
	int32_t version;
	/** Reserved */
	uint8_t reserved_a[16];
	/** Equivalence ID */
	uint16_t id;
	/** Reserved */
	uint8_t reserved_b[14];
} __attribute__ (( packed ));

/** AMD patch type */
#define AMD_UCODE_PATCH_TYPE 0x00000001UL

extern mp_func_t ucode_update;

#endif /* _IPXE_UCODE_H */
