
#include <etherboot.h>
#include <fs.h>
#include <lib.h>

#define DEBUG_THIS DEBUG_ELTORITO
#include <debug.h>

#define ELTORITO_PLATFORM_X86 0
#define ELTORITO_PLATFORM_PPC 1
#define ELTORITO_PLATFORM_MAC 2
#include <bits/eltorito.h>

#ifndef ELTORITO_PLATFORM
#error "ELTORITO_PLATFORM is not defined for this arch"
#endif

/* El Torito boot record at sector 0x11 of bootable CD */
struct boot_record {
    uint8_t ind;
    uint8_t iso_id[5];
    uint8_t version;
    uint8_t boot_id[32];
    uint8_t reserved[32];
    uint8_t catalog_offset[4];
};

/* First entry of the catalog */
struct validation_entry {
    uint8_t header_id;
    uint8_t platform;
    uint8_t reserved[2];
    uint8_t id[24];
    uint8_t checksum[2];
    uint8_t key55;
    uint8_t keyAA;
};

/* Initial/Default catalog entry */
struct default_entry {
    uint8_t boot_id;
    uint8_t media_type;
#define MEDIA_MASK 0x0f
#define MEDIA_NOEMU 0
#define MEDIA_1200_FD 1
#define MEDIA_1440_FD 2
#define MEDIA_2880_FD 3
#define MEDIA_HD 4
    uint8_t load_segment[2];
    uint8_t system_type;
    uint8_t reserved;
    uint8_t sector_count[2];
    uint8_t start_sector[4];
    uint8_t reserved_too[20];
};

/* Find El-Torito boot disk image */
int open_eltorito_image(int part, unsigned long *offset_p,
	unsigned long *length_p)
{
    struct boot_record boot_record;
    uint32_t cat_offset;
    uint8_t catalog[2048];
    struct validation_entry *ve;
    int i, sum;
    struct default_entry *de;

    /* We always use 512-byte "soft sector", but
     * El-Torito uses 2048-byte CD-ROM sector */

    /* Boot Record is at sector 0x11 */
    if (!devread(0x11<<2, 0, sizeof boot_record, &boot_record))
	return 0;

    if (boot_record.ind != 0
	    || memcmp(boot_record.iso_id, "CD001", 5) != 0
	    || memcmp(boot_record.boot_id, "EL TORITO SPECIFICATION", 23)
		!= 0) {
	debug("No El-Torito signature\n");
	return PARTITION_UNKNOWN;
    }

    if (part != 0) {
	printf("El-Torito entries other than Initial/Default is not supported\n");
	return 0;
    }

    cat_offset = get_le32(boot_record.catalog_offset);
    debug("El-Torito boot catalog at sector %u\n", cat_offset);
    if (!devread(cat_offset<<2, 0, 2048, catalog))
	return 0;

    /* Validate the catalog */
    ve = (void *) catalog;
    //debug_hexdump(ve, sizeof *ve);
    if (ve->header_id != 1 || ve->key55 != 0x55 || ve->keyAA != 0xAA) {
	printf("Invalid El Torito boot catalog\n");
	return 0;
    }
    /* All words must sum up to zero */
    sum = 0;
    for (i = 0; i < sizeof(*ve); i += 2)
	sum += get_le16(&catalog[i]);
    sum &= 0xffff;
    if (sum != 0) {
	printf("El Torito boot catalog verify failed\n");
	return 0;
    }
    debug("id='%.*s'\n", sizeof ve->id, ve->id);

    /* Platform check is warning only, because we won't directly execute
     * the image. Just mounting it should be safe. */
    if (ve->platform != ELTORITO_PLATFORM){
	debugx("WARNING: Boot disk for different platform: %d\n", ve->platform);
	}

    /* Just support initial/default entry for now */
    de = (void *) (ve + 1);
    if (de->boot_id != 0x88) {
	debugx("WARNING: Default boot entry is not bootable\n");
	}

    switch (de->media_type & MEDIA_MASK) {
    case MEDIA_NOEMU:
	printf("Disc doesn't use boot disk emulation\n");
	return 0;
    case MEDIA_1200_FD:
	*length_p = 1200*1024/512;
	break;
    case MEDIA_1440_FD:
	*length_p = 1440*1024/512;
	break;
    case MEDIA_2880_FD:
	*length_p = 2880*1024/512;
	break;
    case MEDIA_HD:
	/* FIXME: read partition table and return first partition.
	 * Spec states emulation HD has only one partition and it must
	 * be the first partition */
	printf("Disc uses hard disk emulation - not supported\n");
	return 0;
    }
    *offset_p = get_le32(de->start_sector) << 2;
    debug("offset=%#lx length=%#lx\n", *offset_p, *length_p);

    return 1;
}
