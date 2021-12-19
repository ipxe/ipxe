#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

/**
 * @file
 *
 * Multiboot operating systems
 *
 */

#include "multiboot.h"
FILE_LICENCE(GPL2_OR_LATER_OR_UBDL);

#include <stdbool.h>
#include <stdint.h>

/** The magic number for the Multiboot header */
#define MULTIBOOT_HEADER_MAGIC 0xE85250D6

/**
 * The magic number passed by a Multiboot-compliant boot loader
 *
 * Must be passed in register %eax when jumping to the Multiboot OS
 * image.
 */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x36D76289

/* Alignment of multiboot modules. */
#define MULTIBOOT_MOD_ALIGN 0x00001000

/* Alignment of the multiboot info structure. */
#define MULTIBOOT_INFO_ALIGN 0x00000008

/* Flags set in the ’flags’ member of the multiboot header. */
#define MULTIBOOT_TAG_ALIGN 8
#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_CMDLINE 1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT_TAG_TYPE_MODULE 3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO 4
#define MULTIBOOT_TAG_TYPE_BOOTDEV 5

#define MULTIBOOT_TAG_TYPE_MMAP 6
#define MULTIBOOT_TAG_TYPE_VBE 7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS 9
#define MULTIBOOT_TAG_TYPE_APM 10

#define MULTIBOOT_HEADER_TAG_END 0
#define MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST 1
#define MULTIBOOT_HEADER_TAG_ADDRESS 2
#define MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS 3
#define MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS 4
#define MULTIBOOT_HEADER_TAG_FRAMEBUFFER 5
#define MULTIBOOT_HEADER_TAG_MODULE_ALIGN 6

#define MULTIBOOT_ARCHITECTURE_I386 0
#define MULTIBOOT_ARCHITECTURE_MIPS32 4
#define MULTIBOOT_HEADER_TAG_OPTIONAL 1

#define MULTIBOOT_CONSOLE_FLAGS_CONSOLE_REQUIRED 1
#define MULTIBOOT_CONSOLE_FLAGS_EGA_TEXT_SUPPORTED 2

/*
	Header tags embedded in current image
*/
struct multiboot_header {
	uint32_t magic;
	uint32_t architecture;
	uint32_t header_length;
	uint32_t checksum;
} __attribute__((packed, may_alias));

struct multiboot_header_tag_address {
	uint32_t header_addr;
	uint32_t load_addr;
	uint32_t load_end_addr;
	uint32_t bss_end_addr;
} __attribute__((packed, may_alias));

struct multiboot_header_tag_entry_address {
	uint32_t entry_address;
} __attribute__((packed, may_alias));

struct multiboot_header_tag {
	uint16_t type;
	uint16_t flags;
	uint32_t size;
	union {
		struct multiboot_header_tag_entry_address entry_tag;
		struct multiboot_header_tag_address address_tag;
	};
} __attribute__((packed, may_alias));

/*
	Boot info list written by ipxe into os memory
*/
struct multiboot_bootinfo_start {
	uint32_t total_size;
	uint32_t reserved;
} __attribute__((packed, may_alias));

struct multiboot_bootinfo_header {
	uint32_t type;
	uint32_t size;
} __attribute__((packed, may_alias));

struct multiboot_module_tag {
	struct multiboot_bootinfo_header header;
	uint32_t mod_start;
	uint32_t mod_end;
} __attribute__((packed, may_alias));

struct multiboot_memory_info_tag {
	struct multiboot_bootinfo_header header;
	uint32_t mem_lower;
	uint32_t mem_upper;
} __attribute__((packed, may_alias));

struct multiboot_cmd_line_tag {
	struct multiboot_bootinfo_header header;
} __attribute__((packed, may_alias));

struct multiboot_bootloader_name_tag {
	struct multiboot_bootinfo_header header;
} __attribute__((packed, may_alias));

struct multiboot_memory_map_tag {
	struct multiboot_bootinfo_header header;
	uint32_t entry_size;
	uint32_t entry_version;
} __attribute__((packed, may_alias));

struct multiboot_memory_map_entry {
	uint64_t base_addr;
	uint64_t length;
	uint32 type;
	uint32_t reserved;
} __attribute__((packed, may_alias));

/** Multiboot2 memory tagging */
#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS 4

/** Usable RAM */
#define MBMEM_RAM 1

#endif /* _MULTIBOOT_H */
