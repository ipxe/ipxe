#ifndef ELF_H
#define ELF_H

/**
 * @file
 *
 * ELF headers
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;


typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Section;
typedef Elf64_Half Elf64_Versym;

/** Length of ELF identifier */
#define EI_NIDENT 16

/** ELF header */
typedef struct {
	unsigned char e_ident[EI_NIDENT];
	Elf32_Half e_type;
	Elf32_Half e_machine;
	Elf32_Word e_version;
	Elf32_Addr e_entry;
	Elf32_Off e_phoff;
	Elf32_Off e_shoff;
	Elf32_Word e_flags;
	Elf32_Half e_ehsize;
	Elf32_Half e_phentsize;
	Elf32_Half e_phnum;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum;
	Elf32_Half e_shstrndx;
} Elf32_Ehdr;

typedef struct
{
	unsigned char e_ident[EI_NIDENT];     /* Magic number and other info */
	Elf64_Half    e_type;                 /* Object file type */
	Elf64_Half    e_machine;              /* Architecture */
	Elf64_Word    e_version;              /* Object file version */
	Elf64_Addr    e_entry;                /* Entry point virtual address */
	Elf64_Off     e_phoff;                /* Program header table file offset */
	Elf64_Off     e_shoff;                /* Section header table file offset */
	Elf64_Word    e_flags;                /* Processor-specific flags */
	Elf64_Half    e_ehsize;               /* ELF header size in bytes */
	Elf64_Half    e_phentsize;            /* Program header table entry size */
	Elf64_Half    e_phnum;                /* Program header table entry count */
	Elf64_Half    e_shentsize;            /* Section header table entry size */
	Elf64_Half    e_shnum;                /* Section header table entry count */
	Elf64_Half    e_shstrndx;             /* Section header string table index */
} Elf64_Ehdr;


/* ELF identifier indexes */
#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6

/* ELF magic signature bytes */
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

/* ELF classes */
#define ELFCLASS32 1
#define ELFCLASS64 2

/* ELF data encodings */
#define ELFDATA2LSB 1

/* ELF versions */
#define EV_CURRENT 1

/** ELF program header */
typedef struct {
	Elf32_Word p_type;
	Elf32_Off p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
} Elf32_Phdr;

typedef struct
{
  Elf64_Word    p_type;                 /* Segment type */
  Elf64_Word    p_flags;                /* Segment flags */
  Elf64_Off     p_offset;               /* Segment file offset */
  Elf64_Addr    p_vaddr;                /* Segment virtual address */
  Elf64_Addr    p_paddr;                /* Segment physical address */
  Elf64_Xword   p_filesz;               /* Segment size in file */
  Elf64_Xword   p_memsz;                /* Segment size in memory */
  Elf64_Xword   p_align;                /* Segment alignment */
} Elf64_Phdr;

/* ELF segment types */
#define PT_LOAD 1

#endif /* ELF_H */
