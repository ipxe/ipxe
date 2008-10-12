	.text
	.code32
	.arch i386
	.section ".prefix", "a", @progbits
	.org 0x00

	/* DOS (.com) header
	 *
	 * EFI executables seem to leave most of this empty
	 */
mzhdr:
	.ascii	"MZ"		/* Magic number */
	.word	0		/* Bytes on last page of file */
	.word	0		/* Pages in file */
	.word	0		/* Relocations */
	.word	0		/* Size of header in paragraphs */
	.word	0		/* Minimum extra paragraphs needed */
	.word	0		/* Maximum extra paragraphs needed */
	.word	0		/* Initial (relative) SS value */
	.word	0		/* Initial SP value */
	.word	0		/* "Checksum" */
	.word	0		/* Initial IP value */
	.word	0		/* Initial (relative) CS value */
	.word	0		/* File address of relocation table */
	.word	0		/* Ovesrlay number */
	.word	0, 0, 0, 0	/* Reserved words */
	.word	0		/* OEM identifier (for e_oeminfo) */
	.word	0		/* OEM information; e_oemid specific */
	.word	0, 0, 0, 0, 0	/* Reserved words */
	.word	0, 0, 0, 0, 0	/* Reserved words */
	.long	pehdr_lma	/* File address of new exe header */
	.size	mzhdr, . - mzhdr

	/* PE header */
	.org	0xc0 /* For compatibility with MS toolchain */
pehdr:
	.ascii	"PE\0\0"	/* Magic number */
	.word	0x014c		/* CPU architecture: i386 */
	.word	num_pe_sections	/* Number of sections */
	.long	0x10d1a884	/* Timestamp */
	.long	0		/* Symbol table */
	.long	0		/* Number of symbols */
	.word	opthdr_size	/* Size of optional header */
	.word	0x2102		/* Characteristics */
	.size	pehdr, . - pehdr
	.equ	pehdr_lma, pehdr - mzhdr

	/* "Optional" header */
opthdr:
	.word	0x010b		/* Magic number */
	.byte	0		/* Linker major version number */
	.byte	0		/* Linker minor version number */
	.long	_text_filesz	/* Size of text section */
	.long	_data_filesz	/* Size of data section */
	.long	_bss_filesz	/* Size of bss section */
	.long	efi_entry_lma	/* Entry point */
	.long	_text_lma	/* Text section start RVA */
	.long	_data_lma	/* Data section start RVA */
	.long	0		/* Image base address */
	.long	_max_align	/* Section alignment */
	.long	_max_align	/* File alignment */
	.word	0		/* Operating system major version number */
	.word	0		/* Operating system minor version number */
	.word	0		/* Image major version number */
	.word	0		/* Image minor version number */
	.word	0		/* Subsystem major version number */
	.word	0		/* Subsystem minor version number */
	.long	0		/* Reserved */
	.long	_filesz		/* Total image size */
	.long	_prefix_filesz	/* Total header size */
	.long	0		/* "Checksum" */
	.word	0x0a		/* Subsystem: EFI */
	.word	0		/* DLL characteristics */
	.long	0		/* Size of stack reserve */
	.long	0		/* Size of stack commit */
	.long	0		/* Size of heap reserve */
	.long	0		/* Size of heap commit */
	.long	0		/* Loader flags */
	.long	16		/* Number of data directory entries */
	.long	0, 0		/* Export directory */
	.long	0, 0		/* Import directory */
	.long	0, 0		/* Resource directory */
	.long	0, 0		/* Exception directory */
	.long	0, 0		/* Security directory */
	.long	_reloc_lma, _reloc_filesz /* Base relocation directory */
	.long	debugdir_lma, debugdir_size /* Debug directory */
	.long	0, 0		/* Description directory */
	.long	0, 0		/* Special directory */
	.long	0, 0		/* Thread storage directory */
	.long	0, 0		/* Load configuration directory */
	.long	0, 0		/* Bound import directory */
	.long	0, 0		/* Import address table directory */
	.long	0, 0		/* Delay import directory */
	.long	0, 0		/* Reserved */
	.long	0, 0		/* Reserved */
	.size	opthdr, . - opthdr
	.equ	opthdr_size, . - opthdr

	/* PE sections */
pe_sections:
text_section:
	.asciz	".text"		/* Section name */
	.align	8
	.long	_text_filesz	/* Section size */
	.long	_text_lma	/* Relative Virtual Address */
	.long	_text_filesz	/* Section size (rounded up) */
	.long	_text_lma	/* Pointer to raw data */
	.long	0		/* Link-time relocations */
	.long	0		/* Line numbers */
	.word	0		/* Number of link-time relocations */
	.word	0		/* Number of line numbers */
	.long	0x68000020	/* Characteristics */
rodata_section:
	.asciz	".rodata"	/* Section name */
	.align	8
	.long	_rodata_filesz	/* Section size */
	.long	_rodata_lma	/* Relative Virtual Address */
	.long	_rodata_filesz	/* Section size (rounded up) */
	.long	_rodata_lma	/* Pointer to raw data */
	.long	0		/* Link-time relocations */
	.long	0		/* Line numbers */
	.word	0		/* Number of link-time relocations */
	.word	0		/* Number of line numbers */
	.long	0x48000040	/* Characteristics */
data_section:
	.asciz	".data"		/* Section name */
	.align	8
	.long	_data_filesz	/* Section size */
	.long	_data_lma	/* Relative Virtual Address */
	.long	_data_filesz	/* Section size (rounded up) */
	.long	_data_lma	/* Pointer to raw data */
	.long	0		/* Link-time relocations */
	.long	0		/* Line numbers */
	.word	0		/* Number of link-time relocations */
	.word	0		/* Number of line numbers */
	.long	0xc8000040	/* Characteristics */
reloc_section:
	.asciz	".reloc"	/* Section name */
	.align	8
	.long	_reloc_filesz	/* Section size */
	.long	_reloc_lma	/* Relative Virtual Address */
	.long	_reloc_filesz	/* Section size (rounded up) */
	.long	_reloc_lma	/* Pointer to raw data */
	.long	0		/* Link-time relocations */
	.long	0		/* Line numbers */
	.word	0		/* Number of link-time relocations */
	.word	0		/* Number of line numbers */
	.long	0x42000040	/* Characteristics */

pe_sections_end:
	.size	pe_sections, . - pe_sections
	.equ	num_pe_sections, ( ( . - pe_sections ) / 0x28 )

	/* Debug directory */
	.section ".rodata"
	.globl	debugdir
debugdir:
	.long	0		/* Characteristics */
	.long	0x10d1a884	/* Timestamp */
	.word	0		/* Major version */
	.word	0		/* Minor version */
	.long	0x02		/* RSDS? */
	.long	codeview_rsds_size /* Size of data */
	.long	codeview_rsds_lma /* RVA */
	.long	codeview_rsds_lma /* File offset */
	.size	debugdir, . - debugdir
	.equ	debugdir_size, . - debugdir
	/* Codeview structure */
	.globl	codeview_rsds
codeview_rsds:
	.ascii	"RSDS"		/* Magic number */
	.long	0, 0, 0, 0, 0	/* Unused by EFI */
	.asciz	"efiprefix.pdb"
	.size	codeview_rsds, . - codeview_rsds
	.equ	codeview_rsds_size, . - codeview_rsds
