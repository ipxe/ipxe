#include "elf.h"
	.explicit

	.section ".hdrs", "a"
	/* First the DOS file header */


dos_header:
	.byte 'M', 'Z'	/* Signature */
	.short dos_program_end - dos_program /* Length of image mod 512 bytes*/
	.short 0x0001	/* Length of image in 512 byte pages */ /* FIXME */
	.short 0x0000	/* Number of relocation items following header */
	.short (dos_header_end - dos_header)/16	/* Size of header in 16 byte paragraphs */
	.short 0x0001	/* Minimum number of paragraphs needed to run image */
	.short 0x0001	/* Maximum number of paragraphs program would like */
	.short 0x0000	/* Initial SS */

	.short 0x00b8	/* Initial SP */
	.short 0x0000	/* Negative checksum of image */
	.short 0x0000	/* Initial IP */
	.short 0x0000	/* Initial CS */
	.short 0x0010	/* Offset in EXEC of first relocation item */
	.short 0x0000	/* Overlay number */

	.balign 16
dos_header_end:
dos_program:
	.byte	0xdb	/* retf */
	.balign 16
dos_program_end:
	.org	0x3c
	.byte	pe_signature - dos_header,  0x00, 0x00, 0x00
	.org	0x40		/* NOTE: set this to 0x80 for debugging, 0x40 otherwise */
pe_signature:
	.byte	'P', 'E', 0, 0
coff_header:
#define IMAGE_MACHINE_IA64	0x0200
coff_machine:	.short	IMAGE_MACHINE_IA64
coff_nsections:	.short	(section_headers_end - section_headers)/40
coff_timdat:	.int	1038168747 /* Sun Nov 24 12:12:27 2002 */
coff_symptr:	.int	0x00000000

coff_nsyms:	.int	0x00000000
coff_opthdr:	.short	pe_end - pe_header
#define CF_RELOC_STRIPPED 0x0001
#define CF_EXECUTABLE     0x0002
#define CF_LINE_STRIPPED  0x0004
#define CF_LOCAL_STRIPPED 0x0008
#define CF_DEBUG_STRIPPED 0x0206
coff_flags:	.short	CF_EXECUTABLE | CF_LINE_STRIPPED | CF_LOCAL_STRIPPED | CF_DEBUG_STRIPPED

	/* Option header */
pe_header:
pe_magic:	.short	0x020b /* 020b or 010b? */
pe_linker:	.byte	0x02, 0x38
pe_text_size:	.int	_text_size
pe_data_size:	.int	_data_size
pe_bss_size:	.int	_bss_size
pe_entry:	.int	_start_plabel_rva
pe_text_base:	.int	_text_rva
pe_image_base:	.quad	_image_base
pe_sec_align:	.int	_sect_align
pe_file_align:	.int	_file_align
pe_os_major:	.short	0
pe_os_minor:	.short	0
pe_image_major:	.short	0
pe_image_minro:	.short	0
pe_sub_major:	.short	0
pe_sub_minor:	.short	0
pe_reserved:	.int	0
pe_image_size:	.int	_image_size
pe_hdrs_size:	.int	_hdrs_size
pe_checksum:	.int	0 /* FIXME how do I compute the checksum, unnecessary */
#define SUBSYS_EFI_APP                  10
#define SUBSYS_EFI_BOOT_SERVICE_DRIVER  11
#define SUBSYS_EFI_RUNTIME_DRIVER       12
pe_subsys:	.short	SUBSYS_EFI_APP
pe_dll_flags:	.short	0
pe_stack_res:	.quad	0
pe_stack_commit:.quad	0
pe_heap_res:	.quad	0
pe_heap_commit:	.quad	0
pe_ld_flags:	.int	0
pe_rvas:	.int	(rvas_end - rvas_start)/8

rvas_start:
rva_0_rva:	.int	0
rva_0_size:	.int	0
rva_1_rva:	.int	0
rva_1_size:	.int	0
rva_2_rva:	.int	0
rva_2_size:	.int	0
rva_3_rva:	.int	0
rva_3_size:	.int	0
rva_4_rva:	.int	0
rva_4_size:	.int	0
rva_5_rva:	.int	_reloc_rva
rva_5_size:	.int	__reloc_size
rvas_end:	
pe_end:

section_headers:
#define SCN_CNT_CODE               0x00000020
#define SCN_CNT_INITIALIZED_DATA   0x00000040
#define SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define SCN_MEM_DISCARDABLE        0x02000000
#define SCN_MEM_SHARED             0x10000000
#define SCN_MEM_EXECUTE            0x20000000
#define SCN_MEM_READ               0x40000000
#define SCN_MEM_WRITE              0x80000000

sec1_name:	.byte '.', 'i', 'm', 'g', 0 , 0, 0, 0
sec1_virt_size:	.int	_img_mem_size
sec1_virt_addr:	.int	_img_rva
sec1_file_size:	.int	_img_size
sec1_file_off:	.int	_img_off
sec1_reloc_off:	.int	0
sec1_line_off:	.int	0
sec1_reloc_cnt:	.short	0
sec1_line_cnt:	.short	0
sec1_flags:	.int	SCN_CNT_CODE | SCN_CNT_INITIALIZED_DATA \
			| SCN_MEM_EXECUTE | SCN_MEM_READ | SCN_MEM_WRITE

section_headers_end:	

	.text
	.psr abi64
	.psr lsb
	.global _start
_start:
	{
		alloc	r8=ar.pfs,2,0,0,0
		mov	gp=ip		/* Get the address of _start/_text/__gp */
	}
	;;
	add	r14=@gprel(n1_desc),gp
	add	r15=@gprel(n2_desc),gp
	add	r16=@gprel(bhdr),gp
	;;
	st8	[r14]=in0
	st8	[r15]=in1
	;;
	mov	ar.pfs=r8
	;; 
	mov	r32=r16
	;; 
	br.sptk.few _payload_start

	.data
	.global _start_plabel
	.balign 16
_start_plabel:
	.quad	_start
	.quad	0	/* I don't need a gp value... */

	/* hand-crafted bhdr and parameters */
	.balign 16
bhdr:
b_signature:	.int	0x0E1FB007
b_size:		.int	bhdr_end - bhdr
b_checksum:	.short	0
b_records:	.short	3
		/* A NOP note to 64bit align later data */
		.balign 4
n0_namesz:	.int	0
n0_descsz:	.int	0
n0_type:	.int	EBN_NOP
		.balign 4
n1_namesz:	.int	10
n1_descsz:	.int	8
n1_type:	.int	EB_IA64_IMAGE_HANDLE
n1_name:	.asciz	"Etherboot"
		.balign 4
n1_desc:	.quad	0
		.balign 4
n2_namesz:	.int	10
n2_descsz:	.int	8
n2_type:	.int	EB_IA64_SYSTAB
n2_name:	.asciz	"Etherboot"
		.balign 4
n2_desc:	.quad	0
bhdr_end:
	

	/* hand-craft a .reloc section for the plabel */
#define IMAGE_REL_BASED_ABS	0
#define IMAGE_REL_BASED_DIR64	10

	.section ".reloc", "a"
	.int	_start_plabel_rva			// PAGE RVA
	.int	12					// Block Size (2*4+2*2)
	.short	(IMAGE_REL_BASED_DIR64<<12) + 0 // reloc for plabel's entry point
	.short	(IMAGE_REL_BASED_ABS  <<12) + 0	// dummy reloc for good alignment


