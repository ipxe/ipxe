#include "etherboot.h"
#include "sal.h"

struct sal_entry_base {
	uint8_t entry_type;
#define SAL_TYPE_ENTRYPOINT           0
#define SAL_TYPE_MEMORY               1
#define SAL_TYPE_PLATFORM_FEATURES    2
#define SAL_TYPE_TRANSLATION_REGISTER 3
#define SAL_TYPE_PURGE_DOMAIN         4
#define SAL_TYPE_AP_WAKEUP            5
};

struct sal_entry_point_descriptor {
	uint8_t  entry_type;
	uint8_t  reserved[7];
	uint64_t pal_proc;
	uint64_t sal_proc;
	uint64_t sal_gp;
	uint8_t  reserved2[16];
};

struct sal_memory_descriptor {
	uint8_t  entry_type;
	uint8_t  sal_needs_virt_mapping;
	uint8_t  mem_attr;
#define MEM_ATTR_WB   0
#define MEM_ATTR_UC   8
#define MEM_ATTR_UCE  9
#define MEM_ATTR_WC  10	
	uint8_t  access_rights;
	uint8_t  mem_attr_support;
#define MEM_ATTR_SUPPORTS_WB  1
#define MEM_ATTR_SUPPORTS_UC  2
#define MEM_ATTR_SUPPORTS_UCE 4
#define MEM_ATTR_SUPPORTS_WC  8
	uint8_t  reserved;
	uint8_t  mem_type;
#define MEM_TYPE_RAM         0
#define MEM_TYPE_MIO         1
#define MEM_TYPE_SAPIC       2
#define MEM_TYPE_PIO         3
#define MEM_TYPE_FIRMWARE    4
#define MEM_TYPE_BAD_RAM     9
#define MEM_TYPE_BLACK_HOLE 10
	uint8_t  mem_usage;
#define MEM_USAGE_UNSPECIFIED            0
#define MEM_USAGE_PAL_CODE               1
#define MEM_USAGE_BOOT_SERVICES_CODE     2
#define MEM_USAGE_BOOT_SERVICES_DATA     3
#define MEM_USAGE_RUNTIME_SERVICES_CODE  4
#define MEM_USAGE_RUNTIME_SERVICES_DATA  5
#define MEM_USAGE_IA32_OPTION_ROM        6
#define MEM_USAGE_IA32_SYSTEM_ROM        7
#define MEM_USAGE_ACPI_RECLAIM_MEMORY    8
#define MEM_USAGE_ACPI_NVS_MEMORY        9
#define MEM_USAGE_SAL_PMI_CODE          10
#define MEM_USAGE_SAL_PMI_DATA          11
#define MEM_USAGE_FIRMWARE_RESERVED_RAM 12

#define MEM_USAGE_CPU_TO_IO              0
	uint64_t phys_address;
	uint32_t pages; /* In 4k pages */
	uint32_t reserved2; 
	uint8_t  oem_reserved[8];
};

struct sal_platform_features {
	uint8_t  entry_type;
	uint8_t  feature_list;
#define SAL_FEATURE_BUS_LOCK                   1
#define SAL_FEATURE_PLATFORM_REDIRECTION_HINT  2
#define SAL_FEATURE_PROCESSOR_REDIRECTION_HINT 3
	uint8_t  reserved[14];
};
struct sal_translation_register {
	uint8_t  entry_type;
	uint8_t  tr_type;
#define SAL_ITR 0
#define SAL_DTR 1
	uint8_t  tr_number;
	uint8_t  reserved[5];
	uint64_t virtual_address;
	uint64_t page_size;
	uint8_t  reserved2[8];
};

struct sal_purge_translation_cache_coherency_domain {
	uint8_t  entry_type;
	uint8_t  reserved[3];
	uint32_t coherence_domain_count;
	uint64_t coherence_domain_addr;
};

struct sal_ap_wakeup_descriptor {
	uint8_t  entry_type;
	uint8_t  wakeup_mechanism;
	uint8_t  reserved[6];
	uint64_t interrupt;
};

struct sal_entry {
	union {
		struct sal_entry_base base;
		struct sal_entry_point_descriptor entry_point;
		struct sal_memory_descriptor mem;
		struct sal_platform_features features;
		struct sal_translation_register tr;
		struct sal_purge_translation_cache_coherency_domain purge;
		struct sal_ap_wakeup_descriptor ap_wakeup;
	};
};

struct sal_system_table {
	uint8_t  signature[4]; /* SST_ */
	uint32_t table_length;

	uint16_t sal_rev;
	uint16_t entry_count;
	uint8_t  checksum;
	uint8_t  reserved1[7];
	uint16_t sal_a_version;
	uint16_t sal_b_version;

	uint8_t  oem_id[32];
	uint8_t  product_id[32];
	uint8_t  reserved2[8];
	struct sal_entry entry[0];
};

static struct sal_system_table *sal;
struct fptr sal_entry;

int parse_sal_system_table(void *table)
{
	struct sal_system_table *salp = table;
	uint8_t *ptr;
	uint8_t checksum;
	struct sal_entry *entry;
	unsigned i;
	if (memcmp(salp->signature, "SST_", 4) != 0) {
		return 0;
	}
	ptr = table;
	checksum = 0;
	for(i = 0; i < salp->table_length; i++) {
		checksum += ptr[i];
	}
	if (checksum != 0) {
		return 0;
	}
#if 0
	printf("SALA: %hx SALB: %hx\n",
		salp->sal_a_version,
		salp->sal_b_version);
	printf("SAL OEM: ");
	for(i = 0; i < sizeof(salp->oem_id); i++) {
		uint8_t ch = salp->oem_id[i];
		if (ch == 0)
			break;
		printf("%c", ch);
	}
	printf("\n");

	printf("SAL PRODUCT: ");
	for(i = 0; i < sizeof(salp->product_id); i++) {
		uint8_t ch = salp->product_id[i];
		if (ch == 0)
			break;
		printf("%c", ch);
	}
	printf("\n");
#endif
	sal = salp;
	pal_entry.entry = 0;
	pal_entry.gp = 0;
	sal_entry.entry = 0;
	sal_entry.gp = 0;
	entry = sal->entry;
	i = 0;
	while(i < salp->entry_count) {
		unsigned long size = 0;

		switch(entry->base.entry_type) {
		case SAL_TYPE_ENTRYPOINT:
			size = sizeof(entry->entry_point);
			pal_entry.entry = entry->entry_point.pal_proc;
			sal_entry.entry = entry->entry_point.sal_proc;
			sal_entry.gp    = entry->entry_point.sal_gp;
			break;
		case SAL_TYPE_MEMORY:
			size = sizeof(entry->mem);
			break;
		case SAL_TYPE_PLATFORM_FEATURES:
			size = sizeof(entry->features);
			break;
		case SAL_TYPE_TRANSLATION_REGISTER:
			size = sizeof(entry->tr);
			break;
		case SAL_TYPE_PURGE_DOMAIN:
			size = sizeof(entry->purge);
			break;
		case SAL_TYPE_AP_WAKEUP:
			size = sizeof(entry->ap_wakeup);
			break;
		default:
			break;
		}
		entry = (struct sal_entry *)(((char *)entry) + size);
		i++;
	}
	return 1;
}

#define SAL_SET_VECTORS			0x01000000
#define SAL_GET_STATE_INFO		0x01000001
#define SAL_GET_STATE_INFO_SIZE		0x01000002
#define SAL_CLEAR_STATE_INFO		0x01000003
#define SAL_MC_RENDEZ			0x01000004
#define SAL_MC_SET_PARAMS		0x01000005
#define SAL_REGISTER_PHYSICAL_ADDR	0x01000006

#define SAL_CACHE_FLUSH			0x01000008
#define SAL_CACHE_INIT			0x01000009
#define SAL_PCI_CONFIG_READ		0x01000010
#define SAL_PCI_CONFIG_WRITE		0x01000011
#define SAL_FREQ_BASE			0x01000012

#define SAL_UPDATE_PAL			0x01000020

/*
 * Now define a couple of inline functions for improved type checking
 * and convenience.
 */
long sal_freq_base (unsigned long which, unsigned long *ticks_per_second,
	unsigned long *drift_info)
{
	struct {
		long status;
		unsigned long ticks_per_second;
		unsigned long drift_info;
	} result, __call(void *,...);

	result = __call(&sal_entry, SAL_FREQ_BASE, which, 0, 0, 0, 0, 0, 0); 

	*ticks_per_second = result.ticks_per_second;
	*drift_info = result.drift_info;
	return result.status;
}



/* Read from PCI configuration space */
long sal_pci_config_read (
	unsigned long pci_config_addr, unsigned long size, unsigned long *value)
{
	struct {
		long status;
		unsigned long value;
	} result, __call(void *,...);
	
	result = __call(&sal_entry, SAL_PCI_CONFIG_READ, pci_config_addr, size, 0, 0, 0, 0, 0);
	if (value)
		*value = result.value;
	return result.status;
}

/* Write to PCI configuration space */
long sal_pci_config_write (
	unsigned long pci_config_addr, unsigned long size, unsigned long value)
{
	struct {
		long status;
	} result, __call(void *,...);

	result = __call(&sal_entry, SAL_PCI_CONFIG_WRITE, pci_config_addr, size, value, 0, 0, 0, 0);
	return result.status;
}
