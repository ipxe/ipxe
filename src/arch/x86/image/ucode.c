/*
 * Copyright (C) 2024 Michael Brown <mbrown@fensystems.co.uk>.
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

/** @file
 *
 * Microcode updates
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ipxe/uaccess.h>
#include <ipxe/umalloc.h>
#include <ipxe/image.h>
#include <ipxe/cpuid.h>
#include <ipxe/msr.h>
#include <ipxe/mp.h>
#include <ipxe/timer.h>
#include <ipxe/ucode.h>

/**
 * Maximum number of hyperthread siblings
 *
 * Microcode updates must not be performed on hyperthread siblings at
 * the same time, since they share microcode storage.
 *
 * Hyperthread siblings are always the lowest level of the CPU
 * topology and correspond to the least significant bits of the APIC
 * ID.  We may therefore avoid collisions by performing the microcode
 * updates in batches, with each batch targeting just one value for
 * the least significant N bits of the APIC ID.
 *
 * We assume that no CPUs exist with more than this number of
 * hyperthread siblings.  (This must be a power of two.)
 */
#define UCODE_MAX_HT 8

/** Time to wait for a microcode update to complete */
#define UCODE_WAIT_MS 10

/** A CPU vendor string */
union ucode_vendor_id {
	/** CPUID registers */
	uint32_t dword[3];
	/** Human-readable string */
	uint8_t string[12];
};

/** A CPU vendor */
struct ucode_vendor {
	/** Vendor string */
	union ucode_vendor_id id;
	/** Microcode load trigger MSR */
	uint32_t trigger_msr;
	/** Microcode version requires manual clear */
	uint8_t ver_clear;
	/** Microcode version is reported via high dword */
	uint8_t ver_high;
};

/** A microcode update */
struct ucode_update {
	/** CPU vendor, if known */
	struct ucode_vendor *vendor;
	/** Boot processor CPU signature */
	uint32_t signature;
	/** Platform ID */
	uint32_t platform;
	/** Number of potentially relevant signatures found */
	unsigned int count;
	/** Update descriptors (if being populated) */
	struct ucode_descriptor *desc;
};

/** A microcode update summary */
struct ucode_summary {
	/** Number of CPUs processed */
	unsigned int count;
	/** Lowest observed microcode version */
	int32_t low;
	/** Highest observed microcode version */
	int32_t high;
};

/** Intel CPU vendor */
static struct ucode_vendor ucode_intel = {
	.id = { .string = "GenuineIntel" },
	.ver_clear = 1,
	.ver_high = 1,
	.trigger_msr = MSR_UCODE_TRIGGER_INTEL,
};

/** AMD CPU vendor */
static struct ucode_vendor ucode_amd = {
	.id = { .string = "AuthenticAMD" },
	.trigger_msr = MSR_UCODE_TRIGGER_AMD,
};

/** List of known CPU vendors */
static struct ucode_vendor *ucode_vendors[] = {
	&ucode_intel,
	&ucode_amd,
};

/**
 * Get CPU vendor name (for debugging)
 *
 * @v vendor		CPU vendor
 * @ret name		Name
 */
static const char * ucode_vendor_name ( const union ucode_vendor_id *vendor ) {
	static union {
		union ucode_vendor_id vendor;
		char text[ sizeof ( *vendor ) + 1 /* NUL */ ];
	} u;

	/* Construct name */
	memcpy ( &u.vendor, vendor, sizeof ( u.vendor ) );
	u.text[ sizeof ( u.text ) - 1 ] = '\0';
	return u.text;
}

/**
 * Check status report
 *
 * @v update		Microcode update
 * @v control		Microcode update control
 * @v status		Microcode update status
 * @v summary		Microcode update summary
 * @v id		APIC ID
 * @v optional		Status report is optional
 * @ret rc		Return status code
 */
static int ucode_status ( const struct ucode_update *update,
			  const struct ucode_control *control,
			  const struct ucode_status *status,
			  struct ucode_summary *summary,
			  unsigned int id, int optional ) {
	struct ucode_descriptor *desc;

	/* Sanity check */
	assert ( id <= control->apic_max );

	/* Ignore empty optional status reports */
	if ( optional && ( ! status->signature ) )
		return 0;
	DBGC ( update, "UCODE %#08x signature %#08x ucode %#08x->%#08x\n",
	       id, status->signature, status->before, status->after );

	/* Check CPU signature */
	if ( ! status->signature ) {
		DBGC2 ( update, "UCODE %#08x has no signature\n", id );
		return -ENOENT;
	}

	/* Check APIC ID is correct */
	if ( status->id != id ) {
		DBGC ( update, "UCODE %#08x wrong APIC ID %#08x\n",
		       id, status->id );
		return -EINVAL;
	}

	/* Check that maximum APIC ID was not exceeded */
	if ( control->apic_unexpected ) {
		DBGC ( update, "UCODE %#08x saw unexpected APIC ID %#08x\n",
		       id, control->apic_unexpected );
		return -ERANGE;
	}

	/* Check microcode was not downgraded */
	if ( status->after < status->before ) {
		DBGC ( update, "UCODE %#08x was downgraded %#08x->%#08x\n",
		       id, status->before, status->after );
		return -ENOTTY;
	}

	/* Check that expected updates (if any) were applied */
	for ( desc = update->desc ; desc->signature ; desc++ ) {
		if ( ( desc->signature == status->signature ) &&
		     ( status->after < desc->version ) ) {
			DBGC ( update, "UCODE %#08x failed update %#08x->%#08x "
			       "(wanted %#08x)\n", id, status->before,
			       status->after, desc->version );
			return -EIO;
		}
	}

	/* Update summary */
	summary->count++;
	if ( status->before < summary->low )
		summary->low = status->before;
	if ( status->after > summary->high )
		summary->high = status->after;

	return 0;
}

/**
 * Update microcode on all CPUs
 *
 * @v image		Microcode image
 * @v update		Microcode update
 * @v summary		Microcode update summary to fill in
 * @ret rc		Return status code
 */
static int ucode_update_all ( struct image *image,
			      const struct ucode_update *update,
			      struct ucode_summary *summary ) {
	struct ucode_control control;
	struct ucode_vendor *vendor;
	struct ucode_status *status;
	unsigned int max;
	unsigned int id;
	size_t len;
	int rc;

	/* Initialise summary */
	summary->count = 0;
	summary->low = UCODE_VERSION_MAX;
	summary->high = UCODE_VERSION_MIN;

	/* Allocate status reports */
	max = mp_max_cpuid();
	len = ( ( max + 1 ) * sizeof ( *status ) );
	status = umalloc ( len );
	if ( ! status ) {
		DBGC ( image, "UCODE %s could not allocate %d status reports\n",
		       image->name, ( max + 1 ) );
		rc = -ENOMEM;
		goto err_alloc;
	}
	memset ( status, 0, len );

	/* Construct control structure */
	memset ( &control, 0, sizeof ( control ) );
	control.desc = virt_to_phys ( update->desc );
	control.status = virt_to_phys ( status );
	vendor = update->vendor;
	if ( vendor ) {
		control.ver_clear = vendor->ver_clear;
		control.ver_high = vendor->ver_high;
		control.trigger_msr = vendor->trigger_msr;
	} else {
		assert ( update->count == 0 );
	}
	control.apic_max = max;

	/* Update microcode on boot processor */
	mp_exec_boot ( ucode_update, &control );
	id = mp_boot_cpuid();
	if ( ( rc = ucode_status ( update, &control, &status[id],
				   summary, id, 0 ) ) != 0 ) {
		DBGC ( image, "UCODE %s failed on boot processor: %s\n",
		       image->name, strerror ( rc ) );
		goto err_boot;
	}

	/* Update microcode on application processors, avoiding
	 * simultaneous updates on hyperthread siblings.
	 */
	build_assert ( ( UCODE_MAX_HT & ( UCODE_MAX_HT - 1 ) ) == 0 );
	control.apic_mask = ( UCODE_MAX_HT - 1 );
	for ( ; control.apic_test <= control.apic_mask ; control.apic_test++ ) {
		mp_start_all ( ucode_update, &control );
		mdelay ( UCODE_WAIT_MS );
	}

	/* Check status reports */
	summary->count = 0;
	for ( id = 0 ; id <= max ; id++ ) {
		if ( ( rc = ucode_status ( update, &control, &status[id],
					   summary, id, 1 ) ) != 0 ) {
			goto err_status;
		}
	}

	/* Success */
	rc = 0;

 err_status:
 err_boot:
	ufree ( status );
 err_alloc:
	return rc;
}

/**
 * Add descriptor to list (if applicable)
 *
 * @v image		Microcode image
 * @v start		Starting offset within image
 * @v vendor		CPU vendor
 * @v desc		Microcode descriptor
 * @v platforms		Supported platforms, or 0 for all platforms
 * @v update		Microcode update
 */
static void ucode_describe ( struct image *image, size_t start,
			     const struct ucode_vendor *vendor,
			     const struct ucode_descriptor *desc,
			     uint32_t platforms, struct ucode_update *update ) {

	/* Dump descriptor information */
	DBGC2 ( image, "UCODE %s+%#04zx %s %#08x", image->name, start,
		ucode_vendor_name ( &vendor->id ), desc->signature );
	if ( platforms )
		DBGC2 ( image, " (%#02x)", platforms );
	DBGC2 ( image, " version %#08x\n", desc->version );

	/* Check applicability */
	if ( vendor != update->vendor )
		return;
	if ( ( desc->signature ^ update->signature ) & UCODE_SIGNATURE_MASK )
		return;
	if ( platforms && ( ! ( platforms & update->platform ) ) )
		return;

	/* Add descriptor, if applicable */
	if ( update->desc ) {
		memcpy ( &update->desc[update->count], desc, sizeof ( *desc ) );
		DBGC ( image, "UCODE %s+%#04zx found %s %#08x version %#08x\n",
		       image->name, start, ucode_vendor_name ( &vendor->id ),
		       desc->signature, desc->version );
	}
	update->count++;
}

/**
 * Verify checksum
 *
 * @v image		Microcode image
 * @v start		Starting offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int ucode_verify ( struct image *image, size_t start, size_t len ) {
	const uint32_t *dword;
	uint32_t checksum;
	unsigned int count;

	/* Check length is a multiple of dwords */
	if ( ( len % sizeof ( *dword ) ) != 0 ) {
		DBGC ( image, "UCODE %s+%#04zx invalid length %#zx\n",
		       image->name, start, len );
		return -EINVAL;
	}
	dword = ( image->data + start );

	/* Calculate checksum */
	count = ( len / sizeof ( *dword ) );
	for ( checksum = 0 ; count ; count-- )
		checksum += *(dword++);
	if ( checksum != 0 ) {
		DBGC ( image, "UCODE %s+%#04zx bad checksum %#08x\n",
		       image->name, start, checksum );
		return -EINVAL;
	}

	return 0;
}

/**
 * Parse Intel microcode image
 *
 * @v image		Microcode image
 * @v start		Starting offset within image
 * @v update		Microcode update
 * @ret len		Length consumed, or negative error
 */
static int ucode_parse_intel ( struct image *image, size_t start,
			       struct ucode_update *update ) {
	const struct intel_ucode_header *hdr;
	const struct intel_ucode_ext_header *exthdr;
	const struct intel_ucode_ext *ext;
	struct ucode_descriptor desc;
	size_t remaining;
	size_t offset;
	size_t data_len;
	size_t len;
	unsigned int i;
	int rc;

	/* Read header */
	remaining = ( image->len - start );
	if ( remaining < sizeof ( *hdr ) ) {
		DBGC ( image, "UCODE %s+%#04zx too small for Intel header\n",
		       image->name, start );
		return -ENOEXEC;
	}
	hdr = ( image->data + start );

	/* Determine lengths */
	data_len = hdr->data_len;
	if ( ! data_len )
		data_len = INTEL_UCODE_DATA_LEN;
	len = hdr->len;
	if ( ! len )
		len = ( sizeof ( *hdr ) + data_len );

	/* Verify a selection of fields */
	if ( ( hdr->hver != INTEL_UCODE_HVER ) ||
	     ( hdr->lver != INTEL_UCODE_LVER ) ||
	     ( len < sizeof ( *hdr ) ) ||
	     ( len > remaining ) ||
	     ( data_len > ( len - sizeof ( *hdr ) ) ) ||
	     ( ( data_len % sizeof ( uint32_t ) ) != 0 ) ||
	     ( ( len % INTEL_UCODE_ALIGN ) != 0 ) ) {
		DBGC2 ( image, "UCODE %s+%#04zx is not an Intel update\n",
			image->name, start );
		return -EINVAL;
	}
	DBGC2 ( image, "UCODE %s+%#04zx is an Intel update\n",
		image->name, start );

	/* Verify checksum */
	if ( ( rc = ucode_verify ( image, start, len ) ) != 0 )
		return rc;

	/* Populate descriptor */
	desc.signature = hdr->signature;
	desc.version = hdr->version;
	desc.address = ( virt_to_phys ( image->data ) + start +
			 sizeof ( *hdr ) );

	/* Add non-extended descriptor, if applicable */
	ucode_describe ( image, start, &ucode_intel, &desc, hdr->platforms,
			 update );

	/* Construct extended descriptors, if applicable */
	offset = ( sizeof ( *hdr ) + data_len );
	if ( offset <= ( len - sizeof ( *exthdr ) ) ) {

		/* Read extended header */
		exthdr = ( image->data + start + offset );
		offset += sizeof ( *exthdr );

		/* Read extended signatures */
		for ( i = 0 ; i < exthdr->count ; i++ ) {

			/* Read extended signature */
			if ( offset > ( len - sizeof ( *ext ) ) ) {
				DBGC ( image, "UCODE %s+%#04zx extended "
				       "signature overrun\n",
				       image->name, start );
				return -EINVAL;
			}
			ext = ( image->data + start + offset );
			offset += sizeof ( *ext );

			/* Avoid duplicating non-extended descriptor */
			if ( ( ext->signature == hdr->signature ) &&
			     ( ext->platforms == hdr->platforms ) ) {
				continue;
			}

			/* Construct descriptor, if applicable */
			desc.signature = ext->signature;
			ucode_describe ( image, start, &ucode_intel, &desc,
					 ext->platforms, update );
		}
	}

	return len;
}

/**
 * Parse AMD microcode image
 *
 * @v image		Microcode image
 * @v start		Starting offset within image
 * @v update		Microcode update
 * @ret len		Length consumed, or negative error
 */
static int ucode_parse_amd ( struct image *image, size_t start,
			     struct ucode_update *update ) {
	const struct amd_ucode_header *hdr;
	const struct amd_ucode_equivalence *equiv;
	const struct amd_ucode_patch_header *phdr;
	const struct amd_ucode_patch *patch;
	struct ucode_descriptor desc;
	size_t remaining;
	size_t offset;
	unsigned int count;
	unsigned int used;
	unsigned int i;

	/* Read header */
	remaining = ( image->len - start );
	if ( remaining < sizeof ( *hdr ) ) {
		DBGC ( image, "UCODE %s+%#04zx too small for AMD header\n",
		       image->name, start );
		return -ENOEXEC;
	}
	hdr = ( image->data + start );

	/* Check header */
	if ( hdr->magic != AMD_UCODE_MAGIC ) {
		DBGC2 ( image, "UCODE %s+%#04zx is not an AMD update\n",
			image->name, start );
		return -ENOEXEC;
	}
	DBGC2 ( image, "UCODE %s+%#04zx is an AMD update\n",
		image->name, start );
	if ( hdr->type != AMD_UCODE_EQUIV_TYPE ) {
		DBGC ( image, "UCODE %s+%#04zx unsupported equivalence table "
		       "type %d\n", image->name, start, hdr->type );
		return -ENOTSUP;
	}
	if ( hdr->len > ( remaining - sizeof ( *hdr ) ) ) {
		DBGC ( image, "UCODE %s+%#04zx truncated equivalence table\n",
		       image->name, start );
		return -EINVAL;
	}

	/* Count number of equivalence table entries */
	offset = sizeof ( *hdr );
	equiv = ( image->data + start + offset );
	for ( count = 0 ; offset < ( sizeof ( *hdr ) + hdr->len ) ;
	      count++, offset += sizeof ( *equiv ) ) {
		if ( ! equiv[count].signature )
			break;
	}
	DBGC2 ( image, "UCODE %s+%#04zx has %d equivalence table entries\n",
		image->name, start, count );

	/* Parse available updates */
	offset = ( sizeof ( *hdr ) + hdr->len );
	used = 0;
	while ( used < count ) {

		/* Read patch header */
		if ( ( offset + sizeof ( *phdr ) ) > remaining ) {
			DBGC ( image, "UCODE %s+%#04zx truncated patch "
			       "header\n", image->name, start );
			return -EINVAL;
		}
		phdr = ( image->data + start + offset );
		offset += sizeof ( *phdr );

		/* Validate patch header */
		if ( phdr->type != AMD_UCODE_PATCH_TYPE ) {
			DBGC ( image, "UCODE %s+%#04zx unsupported patch type "
			       "%d\n", image->name, start, phdr->type );
			return -ENOTSUP;
		}
		if ( phdr->len < sizeof ( *patch ) ) {
			DBGC ( image, "UCODE %s+%#04zx underlength patch\n",
			       image->name, start );
			return -EINVAL;
		}
		if ( phdr->len > ( remaining - offset ) ) {
			DBGC ( image, "UCODE %s+%#04zx truncated patch\n",
			       image->name, start );
			return -EINVAL;
		}

		/* Read patch and construct descriptor */
		patch = ( image->data + start + offset );
		desc.version = patch->version;
		desc.address = ( virt_to_phys ( image->data ) +
				 start + offset );
		offset += phdr->len;

		/* Parse equivalence table to find matching signatures */
		for ( i = 0 ; i < count ; i++ ) {
			if ( patch->id == equiv[i].id ) {
				desc.signature = equiv[i].signature;
				ucode_describe ( image, start, &ucode_amd,
						 &desc, 0, update );
				used++;
			}
		}
	}

	return offset;
}

/**
 * Parse microcode image
 *
 * @v image		Microcode image
 * @v update		Microcode update
 * @ret rc		Return status code
 */
static int ucode_parse ( struct image *image, struct ucode_update *update ) {
	size_t start;
	int len;

	/* Attempt to parse concatenated microcode updates */
	for ( start = 0 ; start < image->len ; start += len ) {

		/* Attempt to parse as Intel microcode */
		len = ucode_parse_intel ( image, start, update );
		if ( len > 0 )
			continue;

		/* Attempt to parse as AMD microcode */
		len = ucode_parse_amd ( image, start, update );
		if ( len > 0 )
			continue;

		/* Not a recognised microcode format */
		DBGC ( image, "UCODE %s+%zx not recognised\n",
		       image->name, start );
		return -ENOEXEC;
	}

	return 0;
}

/**
 * Execute microcode update
 *
 * @v image		Microcode image
 * @ret rc		Return status code
 */
static int ucode_exec ( struct image *image ) {
	struct ucode_update update;
	struct ucode_vendor *vendor;
	struct ucode_summary summary;
	union ucode_vendor_id id;
	uint64_t platform_id;
	uint32_t discard_a;
	uint32_t discard_b;
	uint32_t discard_c;
	uint32_t discard_d;
	unsigned int check;
	unsigned int i;
	size_t len;
	int rc;

	/* Initialise update */
	memset ( &update, 0, sizeof ( update ) );
	cpuid ( CPUID_VENDOR_ID, 0, &discard_a, &id.dword[0], &id.dword[2],
		&id.dword[1] );
	cpuid ( CPUID_FEATURES, 0, &update.signature, &discard_b,
		&discard_c, &discard_d );

	/* Identify CPU vendor, if recognised */
	for ( i = 0 ; i < ( sizeof ( ucode_vendors ) /
			    sizeof ( ucode_vendors[0] ) ) ; i++ ) {
		vendor = ucode_vendors[i];
		if ( memcmp ( &id, &vendor->id, sizeof ( id ) ) == 0 )
			update.vendor = vendor;
	}

	/* Identify platform, if applicable */
	if ( update.vendor == &ucode_intel ) {
		platform_id = rdmsr ( MSR_PLATFORM_ID );
		update.platform =
			( 1 << MSR_PLATFORM_ID_VALUE ( platform_id ) );
	}

	/* Count number of matching update descriptors */
	DBGC ( image, "UCODE %s applying to %s %#08x",
	       image->name, ucode_vendor_name ( &id ), update.signature );
	if ( update.platform )
		DBGC ( image, " (%#02x)", update.platform );
	DBGC ( image, "\n" );
	if ( ( rc = ucode_parse ( image, &update ) ) != 0 )
		goto err_count;
	DBGC ( image, "UCODE %s found %d matching update(s)\n",
	       image->name, update.count );

	/* Allocate descriptors */
	len = ( ( update.count + 1 /* terminator */ ) *
		sizeof ( update.desc[0] ) );
	update.desc = zalloc ( len );
	if ( ! update.desc ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Populate descriptors */
	check = update.count;
	update.count = 0;
	if ( ( rc = ucode_parse ( image, &update ) ) != 0 )
		goto err_parse;
	assert ( check == update.count );

	/* Perform update */
	if ( ( rc = ucode_update_all ( image, &update, &summary ) ) != 0 )
		goto err_update;

	/* Print summary if directed to do so */
	if ( image->cmdline && ( strstr ( image->cmdline, "-v" ) ) ) {
		printf ( "Microcode: " );
		if ( summary.low == summary.high ) {
			printf ( "already version %#x", summary.low );
		} else {
			printf ( "updated version %#x->%#x",
				 summary.low, summary.high );
		}
		printf ( " (x%d)\n", summary.count );
	}

 err_update:
 err_parse:
	free ( update.desc );
 err_alloc:
 err_count:
	return rc;
}

/**
 * Probe microcode update image
 *
 * @v image		Microcode image
 * @ret rc		Return status code
 */
static int ucode_probe ( struct image *image ) {
	const union {
		struct intel_ucode_header intel;
		struct amd_ucode_header amd;
	} *header;

	/* Sanity check */
	if ( image->len < sizeof ( *header )  ) {
		DBGC ( image, "UCODE %s too short\n", image->name );
		return -ENOEXEC;
	}

	/* Read first microcode image header */
	header = image->data;

	/* Check for something that looks like an Intel update
	 *
	 * Intel updates unfortunately have no magic signatures or
	 * other easily verifiable fields.  We check a small selection
	 * of header fields that can be easily verified.
	 *
	 * We do not attempt to fully parse the update, since we want
	 * errors to be reported at the point of attempting to execute
	 * the image, and do not want to have a microcode image
	 * erroneously treated as a PXE boot executable.
	 */
	if ( ( header->intel.hver == INTEL_UCODE_HVER ) &&
	     ( header->intel.lver == INTEL_UCODE_LVER ) &&
	     ( ( header->intel.date.century == 0x19 ) ||
	       ( ( header->intel.date.century >= 0x20 ) &&
		 ( header->intel.date.century <= 0x29 ) ) ) ) {
		DBGC ( image, "UCODE %s+%#04zx looks like an Intel update\n",
		       image->name, ( ( size_t ) 0 ) );
		return 0;
	}

	/* Check for AMD update signature */
	if ( ( header->amd.magic == AMD_UCODE_MAGIC ) &&
	     ( header->amd.type == AMD_UCODE_EQUIV_TYPE ) ) {
		DBGC ( image, "UCODE %s+%#04zx looks like an AMD update\n",
		       image->name, ( ( size_t ) 0 ) );
		return 0;
	}

	return -ENOEXEC;
}

/** Microcode update image type */
struct image_type ucode_image_type __image_type ( PROBE_NORMAL ) = {
	.name = "ucode",
	.probe = ucode_probe,
	.exec = ucode_exec,
};
