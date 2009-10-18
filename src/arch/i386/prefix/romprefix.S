/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 *
 * Also be very careful as the stack is at the rear end of the interrupt
 * table so using a noticeable amount of stack space is a no-no.
 */

FILE_LICENCE ( GPL2_OR_LATER )

#include <config/general.h>

#define PNP_SIGNATURE ( '$' + ( 'P' << 8 ) + ( 'n' << 16 ) + ( 'P' << 24 ) )
#define PMM_SIGNATURE ( '$' + ( 'P' << 8 ) + ( 'M' << 16 ) + ( 'M' << 24 ) )
#define PCI_SIGNATURE ( 'P' + ( 'C' << 8 ) + ( 'I' << 16 ) + ( ' ' << 24 ) )
#define STACK_MAGIC ( 'L' + ( 'R' << 8 ) + ( 'E' << 16 ) + ( 'T' << 24 ) )
#define PNP_GET_BBS_VERSION 0x60
#define PMM_ALLOCATE 0x0000
#define PMM_DEALLOCATE 0x0002

/* ROM banner timeout.  Based on the configurable BANNER_TIMEOUT in
 * config.h, but converted to a number of (18Hz) timer ticks, and
 * doubled to allow for BIOSes that switch video modes immediately
 * beforehand, so rendering the message almost invisible to the user.
 */
#define ROM_BANNER_TIMEOUT ( 2 * ( 18 * BANNER_TIMEOUT ) / 10 )

/* We can load a ROM in two ways: have the BIOS load all of it (.rom prefix)
 * or have the BIOS load a stub that loads the rest using PCI (.xrom prefix).
 * The latter is not as widely supported, but allows the use of large ROMs
 * on some systems with crowded option ROM space.
 */

#ifdef LOAD_ROM_FROM_PCI
#define ROM_SIZE_VALUE	_prefix_filesz_sect /* Amount to load in BIOS */
#else
#define ROM_SIZE_VALUE	0		/* Load amount (before compr. fixup) */
#endif


	.text
	.code16
	.arch i386
	.section ".prefix", "ax", @progbits
	
	.org	0x00
romheader:
	.word	0xAA55			/* BIOS extension signature */
romheader_size:	.byte ROM_SIZE_VALUE	/* Size in 512-byte blocks */
	jmp	init			/* Initialisation vector */
checksum:
	.byte	0, 0
real_size:
	.word	0
	.org	0x16
	.word	undiheader
	.org	0x18
	.word	pciheader
	.org	0x1a
	.word	pnpheader
	.size romheader, . - romheader

	.section ".zinfo.fixup", "a", @progbits	/* Compressor fixups */
#ifndef LOAD_ROM_FROM_PCI
	.ascii	"ADDB"
	.long	romheader_size
	.long	512
	.long	0
#endif
	.ascii	"ADDB"
	.long	real_size
	.long	512
	.long	0
	.previous

pciheader:
	.ascii	"PCIR"			/* Signature */
	.word	pci_vendor_id		/* Vendor identification */ 
	.word	pci_device_id		/* Device identification */
	.word	0x0000			/* Device list pointer */
	.word	pciheader_len		/* PCI data structure length */
	.byte	0x03			/* PCI data structure revision */
	.byte	0x02, 0x00, 0x00	/* Class code */
pciheader_image_length:
	.word	ROM_SIZE_VALUE		/* Image length */
	.word	0x0001			/* Revision level */
	.byte	0x00			/* Code type */
	.byte	0x80			/* Last image indicator */
pciheader_runtime_length:
	.word	ROM_SIZE_VALUE		/* Maximum run-time image length */
	.word	0x0000			/* Configuration utility code header */
	.word	0x0000			/* DMTF CLP entry point */
	.equ pciheader_len, . - pciheader
	.size pciheader, . - pciheader

#ifndef LOAD_ROM_FROM_PCI
	.section ".zinfo.fixup", "a", @progbits	/* Compressor fixups */
	.ascii	"ADDW"
	.long	pciheader_image_length
	.long	512
	.long	0
	.ascii	"ADDW"
	.long	pciheader_runtime_length
	.long	512
	.long	0
	.previous
#endif

pnpheader:
	.ascii	"$PnP"			/* Signature */
	.byte	0x01			/* Structure revision */
	.byte	( pnpheader_len	/ 16 )	/* Length (in 16 byte increments) */
	.word	0x0000			/* Offset of next header */
	.byte	0x00			/* Reserved */
	.byte	0x00			/* Checksum */
	.long	0x00000000		/* Device identifier */
	.word	mfgstr			/* Manufacturer string */
	.word	prodstr			/* Product name */
	.byte	0x02			/* Device base type code */
	.byte	0x00			/* Device sub-type code */
	.byte	0x00			/* Device interface type code */
	.byte	0xf4			/* Device indicator */
	.word	0x0000			/* Boot connection vector */
	.word	0x0000			/* Disconnect vector */
	.word	bev_entry		/* Boot execution vector */
	.word	0x0000			/* Reserved */
	.word	0x0000			/* Static resource information vector*/
	.equ pnpheader_len, . - pnpheader
	.size pnpheader, . - pnpheader

/* Manufacturer string */
mfgstr:
	.asciz	"http://etherboot.org"
	.size mfgstr, . - mfgstr

/* Product string
 *
 * Defaults to PRODUCT_SHORT_NAME.  If the ROM image is writable at
 * initialisation time, it will be filled in to include the PCI
 * bus:dev.fn number of the card as well.
 */
prodstr:
	.ascii	PRODUCT_SHORT_NAME
prodstr_separator:
	.byte	0
	.ascii	"(PCI "
prodstr_pci_id:
	.asciz	"xx:xx.x)"		/* Filled in by init code */
	.size prodstr, . - prodstr

	.globl	undiheader	
	.weak	undiloader
undiheader:
	.ascii	"UNDI"			/* Signature */
	.byte	undiheader_len		/* Length of structure */
	.byte	0			/* Checksum */
	.byte	0			/* Structure revision */
	.byte	0,1,2			/* PXE version: 2.1.0 */
	.word	undiloader		/* Offset to loader routine */
	.word	_data16_memsz		/* Stack segment size */
	.word	_data16_memsz		/* Data segment size */
	.word	_text16_memsz		/* Code segment size */
	.ascii	"PCIR"			/* Bus type */
	.equ undiheader_len, . - undiheader
	.size undiheader, . - undiheader

/* Initialisation (called once during POST)
 *
 * Determine whether or not this is a PnP system via a signature
 * check.  If it is PnP, return to the PnP BIOS indicating that we are
 * a boot-capable device; the BIOS will call our boot execution vector
 * if it wants to boot us.  If it is not PnP, hook INT 19.
 */
init:
	/* Preserve registers, clear direction flag, set %ds=%cs */
	pushaw
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs
	cld
	pushw	%cs
	popw	%ds

	/* Shuffle some registers around.  We need %di available for
	 * the print_xxx functions, and in a register that's
	 * addressable from %es, so shuffle as follows:
	 *
	 *    %di (pointer to PnP structure) => %bx
	 *    %bx (runtime segment address, for PCI 3.0) => %gs
	 */
	movw	%bx, %gs
	movw	%di, %bx

	/* Print message as early as possible */
	movw	$init_message, %si
	xorw	%di, %di
	call	print_message
	call	print_pci_busdevfn

#ifdef LOAD_ROM_FROM_PCI
	/* Save PCI bus:dev.fn for later use */
	movw	%ax, pci_busdevfn
#endif

	/* Fill in product name string, if possible */
	movw	$prodstr_pci_id, %di
	call	print_pci_busdevfn
	movb	$( ' ' ), prodstr_separator

	/* Print segment address */
	movb	$( ' ' ), %al
	xorw	%di, %di
	call	print_character
	movw	%cs, %ax
	call	print_hex_word

	/* Check for PCI BIOS version */
	pushl	%ebx
	pushl	%edx
	pushl	%edi
	stc
	movw	$0xb101, %ax
	int	$0x1a
	jc	no_pci3
	cmpl	$PCI_SIGNATURE, %edx
	jne	no_pci3
	testb	%ah, %ah
	jnz	no_pci3
#ifdef LOAD_ROM_FROM_PCI
	incb	pcibios_present
#endif
	movw	$init_message_pci, %si
	xorw	%di, %di
	call	print_message
	movb	%bh, %al
	call	print_hex_nibble
	movb	$( '.' ), %al
	call	print_character
	movb	%bl, %al
	call	print_hex_byte
	cmpb	$3, %bh
	jb	no_pci3
	/* PCI >=3.0: leave %gs as-is if sane */
	movw	%gs, %ax
	cmpw	$0xa000, %ax	/* Insane if %gs < 0xa000 */
	jb	pci3_insane
	movw	%cs, %bx	/* Sane if %cs == %gs */
	cmpw	%bx, %ax
	je	1f
	movzbw	romheader_size, %cx /* Sane if %cs+len <= %gs */
	shlw	$5, %cx
	addw	%cx, %bx
	cmpw	%bx, %ax
	jae	1f
	movw	%cs, %bx	/* Sane if %gs+len <= %cs */
	addw	%cx, %ax
	cmpw	%bx, %ax
	jbe	1f
pci3_insane: /* PCI 3.0 with insane %gs value: print error and ignore %gs */
	movb	$( '!' ), %al
	call	print_character
	movw	%gs, %ax
	call	print_hex_word
no_pci3:
	/* PCI <3.0: set %gs (runtime segment) = %cs (init-time segment) */
	pushw	%cs
	popw	%gs
1:	popl	%edi
	popl	%edx
	popl	%ebx

	/* Check for PnP BIOS.  Although %es:di should point to the
	 * PnP BIOS signature on entry, some BIOSes fail to do this.
	 */
	movw	$( 0xf000 - 1 ), %bx
pnp_scan:
	incw	%bx
	jz	no_pnp
	movw	%bx, %es
	cmpl	$PNP_SIGNATURE, %es:0
	jne	pnp_scan
	xorw	%dx, %dx
	xorw	%si, %si
	movzbw	%es:5, %cx
1:	es lodsb
	addb	%al, %dl
	loop	1b
	jnz	pnp_scan
	/* Is PnP: print PnP message */
	movw	$init_message_pnp, %si
	xorw	%di, %di
	call	print_message
	/* Check for BBS */
	pushw	%es:0x1b	/* Real-mode data segment */
	pushw	%ds		/* &(bbs_version) */
	pushw	$bbs_version
	pushw	$PNP_GET_BBS_VERSION
	lcall	*%es:0xd
	addw	$8, %sp
	testw	%ax, %ax
	je	got_bbs
no_pnp:	/* Not PnP-compliant - therefore cannot be BBS-compliant */
no_bbs:	/* Not BBS-compliant - must hook INT 19 */
	movw	$init_message_int19, %si
	xorw	%di, %di
	call	print_message
	xorw	%ax, %ax
	movw	%ax, %es
	pushl	%es:( 0x19 * 4 )
	popl	orig_int19
	pushw	%gs /* %gs contains runtime %cs */
	pushw	$int19_entry
	popl	%es:( 0x19 * 4 )
	jmp	bbs_done
got_bbs: /* BBS compliant - no need to hook INT 19 */
	movw	$init_message_bbs, %si
	xorw	%di, %di
	call	print_message
bbs_done:

	/* Check for PMM */
	movw	$( 0xe000 - 1 ), %bx
pmm_scan:
	incw	%bx
	jz	no_pmm
	movw	%bx, %es
	cmpl	$PMM_SIGNATURE, %es:0
	jne	pmm_scan
	xorw	%dx, %dx
	xorw	%si, %si
	movzbw	%es:5, %cx
1:	es lodsb
	addb	%al, %dl
	loop	1b
	jnz	pmm_scan
	/* PMM found: print PMM message */
	movw	$init_message_pmm, %si
	xorw	%di, %di
	call	print_message
	/* We have PMM and so a 1kB stack: preserve upper register halves */
	pushal
	/* Calculate required allocation size in %esi */
	movzwl	real_size, %eax
	shll	$9, %eax
	addl	$_textdata_memsz, %eax
	orw	$0xffff, %ax	/* Ensure allocation size is at least 64kB */
	bsrl	%eax, %ecx
	subw	$15, %cx	/* Round up and convert to 64kB count */
	movw	$1, %si
	shlw	%cl, %si
pmm_loop:
	/* Try to allocate block via PMM */
	pushw	$0x0006		/* Aligned, extended memory */
	pushl	$0xffffffff	/* No handle */
	movzwl	%si, %eax
	shll	$12, %eax
	pushl	%eax		/* Allocation size in paragraphs */
	pushw	$PMM_ALLOCATE
	lcall	*%es:7
	addw	$12, %sp
	/* Abort if allocation fails */
	testw	%dx, %dx	/* %ax==0 even on success, since align>=64kB */
	jz	pmm_fail
	/* If block has A20==1, free block and try again with twice
	 * the allocation size (and hence alignment).
	 */
	testw	$0x0010, %dx
	jz	got_pmm
	pushw	%dx
	pushw	$0
	pushw	$PMM_DEALLOCATE
	lcall	*%es:7
	addw	$6, %sp
	addw	%si, %si
	jmp	pmm_loop
got_pmm: /* PMM allocation succeeded */
	movw	%dx, ( image_source + 2 )
	movw	%dx, %ax
	xorw	%di, %di
	call	print_hex_word
	movb	$( '@' ), %al
	call	print_character
	movw	%si, %ax
	call	print_hex_byte
pmm_copy:
	/* Copy ROM to PMM block */
	xorw	%ax, %ax
	movw	%ax, %es
	movl	image_source, %edi
	xorl	%esi, %esi
	movzbl	romheader_size, %ecx
	shll	$9, %ecx
	addr32 rep movsb	/* PMM presence implies flat real mode */
	movl	%edi, decompress_to
	/* Shrink ROM */
	movb	$_prefix_memsz_sect, romheader_size
#if defined(SHRINK_WITHOUT_PMM) || defined(LOAD_ROM_FROM_PCI)
	jmp	pmm_done
pmm_fail:
	/* Print marker and copy ourselves to high memory */
	movl	$HIGHMEM_LOADPOINT, image_source
	xorw	%di, %di
	movb	$( '!' ), %al
	call	print_character
	jmp	pmm_copy
pmm_done:
#else
pmm_fail:
#endif
	/* Restore upper register halves */
	popal
#if defined(LOAD_ROM_FROM_PCI)
	call	load_from_pci
	jc	load_err
	jmp	load_ok
no_pmm:
	/* Cannot continue without PMM - print error message */
	xorw	%di, %di
	movw	$init_message_no_pmm, %si
	call	print_message
load_err:
	/* Wait for five seconds to let user see message */
	movw	$90, %cx
1:	call	wait_for_tick
	loop	1b
	/* Mark environment as invalid and return */
	movl	$0, decompress_to
	jmp	out

load_ok:
#else
no_pmm:
#endif
	/* Update checksum */
	xorw	%bx, %bx
	xorw	%si, %si
	movzbw	romheader_size, %cx
	shlw	$9, %cx
1:	lodsb
	addb	%al, %bl
	loop	1b
	subb	%bl, checksum

	/* Copy self to option ROM space.  Required for PCI3.0, which
	 * loads us to a temporary location in low memory.  Will be a
	 * no-op for lower PCI versions.
	 */
	movb	$( ' ' ), %al
	xorw	%di, %di
	call	print_character
	movw	%gs, %ax
	call	print_hex_word
	movzbw	romheader_size, %cx
	shlw	$9, %cx
	movw	%ax, %es
	xorw	%si, %si
	xorw	%di, %di
	cs rep	movsb

	/* Prompt for POST-time shell */
	movw	$init_message_prompt, %si
	xorw	%di, %di
	call	print_message
	movw	$prodstr, %si
	call	print_message
	movw	$init_message_dots, %si
	call	print_message
	/* Wait for Ctrl-B */
	movw	$0xff02, %bx
	call	wait_for_key
	/* Clear prompt */
	pushf
	xorw	%di, %di
	call	print_kill_line
	movw	$init_message_done, %si
	call	print_message
	popf
	jnz	out
	/* Ctrl-B was pressed: invoke gPXE.  The keypress will be
	 * picked up by the initial shell prompt, and we will drop
	 * into a shell.
	 */
	pushw	%cs
	call	exec
out:
	/* Restore registers */
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	popaw

	/* Indicate boot capability to PnP BIOS, if present */
	movw	$0x20, %ax
	lret
	.size init, . - init

/*
 * Note to hardware vendors:
 *
 * If you wish to brand this boot ROM, please do so by defining the
 * strings PRODUCT_NAME and PRODUCT_SHORT_NAME in config/general.h.
 *
 * While nothing in the GPL prevents you from removing all references
 * to gPXE or http://etherboot.org, we prefer you not to do so.
 *
 * If you have an OEM-mandated branding requirement that cannot be
 * satisfied simply by defining PRODUCT_NAME and PRODUCT_SHORT_NAME,
 * please contact us.
 *
 * [ Including an ASCII NUL in PRODUCT_NAME is considered to be
 *   bypassing the spirit of this request! ]
 */
init_message:
	.ascii	"\n"
	.ascii	PRODUCT_NAME
	.ascii	"\n"
	.asciz	"gPXE (http://etherboot.org) - "
	.size	init_message, . - init_message
init_message_pci:
	.asciz	" PCI"
	.size	init_message_pci, . - init_message_pci
init_message_pnp:
	.asciz	" PnP"
	.size	init_message_pnp, . - init_message_pnp
init_message_bbs:
	.asciz	" BBS"
	.size	init_message_bbs, . - init_message_bbs
init_message_pmm:
	.asciz	" PMM"
	.size	init_message_pmm, . - init_message_pmm
#ifdef LOAD_ROM_FROM_PCI
init_message_no_pmm:
	.asciz	"\nPMM required but not present!\n"
	.size	init_message_no_pmm, . - init_message_no_pmm
#endif
init_message_int19:
	.asciz	" INT19"
	.size	init_message_int19, . - init_message_int19
init_message_prompt:
	.asciz	"\nPress Ctrl-B to configure "
	.size	init_message_prompt, . - init_message_prompt
init_message_dots:
	.asciz	"..."
	.size	init_message_dots, . - init_message_dots
init_message_done:
	.asciz	"\n\n"
	.size	init_message_done, . - init_message_done

/* ROM image location
 *
 * May be either within option ROM space, or within PMM-allocated block.
 */
	.globl	image_source
image_source:
	.long	0
	.size	image_source, . - image_source

/* Temporary decompression area
 *
 * May be either at HIGHMEM_LOADPOINT, or within PMM-allocated block.
 * If a PCI ROM load fails, this will be set to zero.
 */
	.globl	decompress_to
decompress_to:
	.long	HIGHMEM_LOADPOINT
	.size	decompress_to, . - decompress_to

#ifdef LOAD_ROM_FROM_PCI

/* Set if the PCI BIOS is present, even <3.0 */
pcibios_present:
	.byte	0
	.byte	0		/* for alignment */
	.size	pcibios_present, . - pcibios_present

/* PCI bus:device.function word
 *
 * Filled in by init in the .xrom case, so the remainder of the ROM
 * can be located.
 */
pci_busdevfn:
	.word	0
	.size	pci_busdevfn, . - pci_busdevfn

#endif

/* BBS version
 *
 * Filled in by BBS BIOS.  We ignore the value.
 */
bbs_version:
	.word	0
	.size	bbs_version, . - bbs_version

/* Boot Execution Vector entry point
 *
 * Called by the PnP BIOS when it wants to boot us.
 */
bev_entry:
	pushw	%cs
	call	exec
	lret
	.size	bev_entry, . - bev_entry


#ifdef LOAD_ROM_FROM_PCI

#define PCI_ROM_ADDRESS		0x30	/* Bits 31:11 address, 10:1 reserved */
#define PCI_ROM_ADDRESS_ENABLE	 0x00000001
#define PCI_ROM_ADDRESS_MASK	 0xfffff800

#define PCIBIOS_READ_WORD	0xb109
#define PCIBIOS_READ_DWORD	0xb10a
#define PCIBIOS_WRITE_WORD	0xb10c
#define PCIBIOS_WRITE_DWORD	0xb10d

/* Determine size of PCI BAR
 *
 *  %bx : PCI bus:dev.fn to probe
 *  %di : Address of BAR to find size of
 * %edx : Mask of address bits within BAR
 *
 * %ecx : Size for a memory resource,
 *	  1 for an I/O resource (bit 0 set).
 *   CF : Set on error or nonexistent device (all-ones read)
 *
 * All other registers saved.
 */
pci_bar_size:
	/* Save registers */
	pushw	%ax
	pushl	%esi
	pushl	%edx

	/* Read current BAR value */
	movw	$PCIBIOS_READ_DWORD, %ax
	int	$0x1a

	/* Check for device existence and save it */
	testb	$1, %cl		/* I/O bit? */
	jz	1f
	andl	$1, %ecx	/* If so, exit with %ecx = 1 */
	jmp	99f
1:	notl	%ecx
	testl	%ecx, %ecx	/* Set ZF iff %ecx was all-ones */
	notl	%ecx
	jnz	1f
	stc			/* All ones - exit with CF set */
	jmp	99f
1:	movl	%ecx, %esi	/* Save in %esi */

	/* Write all ones to BAR */
	movl	%edx, %ecx
	movw	$PCIBIOS_WRITE_DWORD, %ax
	int	$0x1a

	/* Read back BAR */
	movw	$PCIBIOS_READ_DWORD, %ax
	int	$0x1a

	/* Find decode size from least set bit in mask BAR */
	bsfl	%ecx, %ecx	/* Find least set bit, log2(decode size) */
	jz	1f		/* Mask BAR should not be zero */
	xorl	%edx, %edx
	incl	%edx
	shll	%cl, %edx	/* %edx = decode size */
	jmp	2f
1:	xorl	%edx, %edx	/* Return zero size for mask BAR zero */

	/* Restore old BAR value */
2:	movl	%esi, %ecx
	movw	$PCIBIOS_WRITE_DWORD, %ax
	int	$0x1a

	movl	%edx, %ecx	/* Return size in %ecx */

	/* Restore registers and return */
99:	popl	%edx
	popl	%esi
	popw	%ax
	ret

	.size	pci_bar_size, . - pci_bar_size

/* PCI ROM loader
 *
 * Called from init in the .xrom case to load the non-prefix code
 * using the PCI ROM BAR.
 *
 * Returns with carry flag set on error. All registers saved.
 */
load_from_pci:
	/*
	 * Use PCI BIOS access to config space. The calls take
	 *
	 *   %ah : 0xb1		%al : function
	 *   %bx : bus/dev/fn
	 *   %di : config space address
	 *  %ecx : value to write (for writes)
	 *
	 *  %ecx : value read (for reads)
	 *   %ah : return code
	 *    CF : error indication
	 *
	 * All registers not used for return are preserved.
	 */

	/* Save registers and set up %es for big real mode */
	pushal
	pushw	%es
	xorw	%ax, %ax
	movw	%ax, %es

	/* Check PCI BIOS presence */
	cmpb	$0, pcibios_present
	jz	err_pcibios

	/* Load existing PCI ROM BAR */
	movw	$PCIBIOS_READ_DWORD, %ax
	movw	pci_busdevfn, %bx
	movw	$PCI_ROM_ADDRESS, %di
	int	$0x1a

	/* Maybe it's already enabled? */
	testb	$PCI_ROM_ADDRESS_ENABLE, %cl
	jz	1f
	movb	$1, %dl		/* Flag indicating no deinit required */
	movl	%ecx, %ebp
	jmp	check_rom

	/* Determine PCI BAR decode size */
1:	movl	$PCI_ROM_ADDRESS_MASK, %edx
	call	pci_bar_size	/* Returns decode size in %ecx */
	jc	err_size_insane	/* CF => no ROM BAR, %ecx == ffffffff */

	/* Check sanity of decode size */
	xorl	%eax, %eax
	movw	real_size, %ax
	shll	$9, %eax	/* %eax = ROM size */
	cmpl	%ecx, %eax
	ja	err_size_insane	/* Insane if decode size < ROM size */
	cmpl	$0x100000, %ecx
	jae	err_size_insane	/* Insane if decode size >= 1MB */

	/* Find a place to map the BAR
	 * In theory we should examine e820 and all PCI BARs to find a
	 * free region. However, we run at POST when e820 may not be
	 * available, and memory reads of an unmapped location are
	 * de facto standardized to return all-ones. Thus, we can get
	 * away with searching high memory (0xf0000000 and up) on
	 * multiples of the ROM BAR decode size for a sufficiently
	 * large all-ones region.
	 */
	movl	%ecx, %edx	/* Save ROM BAR size in %edx */
	movl	$0xf0000000, %ebp
	xorl	%eax, %eax
	notl	%eax		/* %eax = all ones */
bar_search:
	movl	%ebp, %edi
	movl	%edx, %ecx
	shrl	$2, %ecx
	addr32 repe scasl	/* Scan %es:edi for anything not all-ones */
	jz	bar_found
	addl	%edx, %ebp
	testl	$0x80000000, %ebp
	jz	err_no_bar
	jmp	bar_search

bar_found:
	movl	%edi, %ebp
	/* Save current BAR value on stack to restore later */
	movw	$PCIBIOS_READ_DWORD, %ax
	movw	$PCI_ROM_ADDRESS, %di
	int	$0x1a
	pushl	%ecx

	/* Map the ROM */
	movw	$PCIBIOS_WRITE_DWORD, %ax
	movl	%ebp, %ecx
	orb	$PCI_ROM_ADDRESS_ENABLE, %cl
	int	$0x1a

	xorb	%dl, %dl	/* %dl = 0 : ROM was not already mapped */
check_rom:
	/* Check and copy ROM - enter with %dl set to skip unmapping,
	 * %ebp set to mapped ROM BAR address.
	 * We check up to prodstr_separator for equality, since anything past
	 * that may have been modified. Since our check includes the checksum
	 * byte over the whole ROM stub, that should be sufficient.
	 */
	xorb	%dh, %dh	/* %dh = 0 : ROM did not fail integrity check */

	/* Verify ROM integrity */
	xorl	%esi, %esi
	movl	%ebp, %edi
	movl	$prodstr_separator, %ecx
	addr32 repe cmpsb
	jz	copy_rom
	incb	%dh		/* ROM failed integrity check */
	movl	%ecx, %ebp	/* Save number of bytes left */
	jmp	skip_load

copy_rom:
	/* Print BAR address and indicate whether we mapped it ourselves */
	movb	$( ' ' ), %al
	xorw	%di, %di
	call	print_character
	movl	%ebp, %eax
	call	print_hex_dword
	movb	$( '-' ), %al	/* '-' for self-mapped */
	subb	%dl, %al
	subb	%dl, %al	/* '+' = '-' - 2 for BIOS-mapped */
	call	print_character

	/* Copy ROM at %ebp to PMM or highmem block */
	movl	%ebp, %esi
	movl	image_source, %edi
	movzwl	real_size, %ecx
	shll	$9, %ecx
	addr32 es rep movsb
	movl	%edi, decompress_to
skip_load:
	testb	%dl, %dl	/* Was ROM already mapped? */
	jnz	skip_unmap

	/* Unmap the ROM by restoring old ROM BAR */
	movw	$PCIBIOS_WRITE_DWORD, %ax
	movw	$PCI_ROM_ADDRESS, %di
	popl	%ecx
	int	$0x1a

skip_unmap:
	/* Error handling */
	testb	%dh, %dh
	jnz	err_rom_invalid
	clc
	jmp	99f

err_pcibios:			/* No PCI BIOS available */
	movw	$load_message_no_pcibios, %si
	xorl	%eax, %eax	/* "error code" is zero */
	jmp	1f
err_size_insane:		/* BAR has size (%ecx) that is insane */
	movw	$load_message_size_insane, %si
	movl	%ecx, %eax
	jmp	1f
err_no_bar:			/* No space of sufficient size (%edx) found */
	movw	$load_message_no_bar, %si
	movl	%edx, %eax
	jmp	1f
err_rom_invalid:		/* Loaded ROM does not match (%ebp bytes left) */
	movw	$load_message_rom_invalid, %si
	movzbl	romheader_size, %eax
	shll	$9, %eax
	subl	%ebp, %eax
	decl	%eax		/* %eax is now byte index of failure */

1:	/* Error handler - print message at %si and dword in %eax */
	xorw	%di, %di
	call	print_message
	call	print_hex_dword
	stc
99:	popw	%es
	popal
	ret

	.size	load_from_pci, . - load_from_pci

load_message_no_pcibios:
	.asciz	"\nNo PCI BIOS found! "
	.size	load_message_no_pcibios, . - load_message_no_pcibios

load_message_size_insane:
	.asciz	"\nROM resource has invalid size "
	.size	load_message_size_insane, . - load_message_size_insane

load_message_no_bar:
	.asciz	"\nNo memory hole of sufficient size "
	.size	load_message_no_bar, . - load_message_no_bar

load_message_rom_invalid:
	.asciz	"\nLoaded ROM is invalid at "
	.size	load_message_rom_invalid, . - load_message_rom_invalid

#endif /* LOAD_ROM_FROM_PCI */


/* INT19 entry point
 *
 * Called via the hooked INT 19 if we detected a non-PnP BIOS.  We
 * attempt to return via the original INT 19 vector (if we were able
 * to store it).
 */
int19_entry:
	pushw	%cs
	popw	%ds
	/* Prompt user to press B to boot */
	movw	$int19_message_prompt, %si
	xorw	%di, %di
	call	print_message
	movw	$prodstr, %si
	call	print_message
	movw	$int19_message_dots, %si
	call	print_message
	movw	$0xdf4e, %bx
	call	wait_for_key
	pushf
	xorw	%di, %di
	call	print_kill_line
	movw	$int19_message_done, %si
	call	print_message
	popf
	jz	1f
	/* Leave keypress in buffer and start gPXE.  The keypress will
	 * cause the usual initial Ctrl-B prompt to be skipped.
	 */
	pushw	%cs
	call	exec
1:	/* Try to call original INT 19 vector */
	movl	%cs:orig_int19, %eax
	testl	%eax, %eax
	je	2f
	ljmp	*%cs:orig_int19
2:	/* No chained vector: issue INT 18 as a last resort */
	int	$0x18
	.size	int19_entry, . - int19_entry
orig_int19:
	.long	0
	.size	orig_int19, . - orig_int19

int19_message_prompt:
	.asciz	"Press N to skip booting from "
	.size	int19_message_prompt, . - int19_message_prompt
int19_message_dots:
	.asciz	"..."
	.size	int19_message_dots, . - int19_message_dots
int19_message_done:
	.asciz	"\n\n"
	.size	int19_message_done, . - int19_message_done
	
/* Execute as a boot device
 *
 */
exec:	/* Set %ds = %cs */
	pushw	%cs
	popw	%ds

#ifdef LOAD_ROM_FROM_PCI
	/* Don't execute if load was invalid */
	cmpl	$0, decompress_to
	jne	1f
	lret
1:
#endif

	/* Print message as soon as possible */
	movw	$prodstr, %si
	xorw	%di, %di
	call	print_message
	movw	$exec_message, %si
	call	print_message

	/* Store magic word on BIOS stack and remember BIOS %ss:sp */
	pushl	$STACK_MAGIC
	movw	%ss, %dx
	movw	%sp, %bp

	/* Obtain a reasonably-sized temporary stack */
	xorw	%ax, %ax
	movw	%ax, %ss
	movw	$0x7c00, %sp

	/* Install gPXE */
	movl	image_source, %esi
	movl	decompress_to, %edi
	call	alloc_basemem
	call	install_prealloc

	/* Set up real-mode stack */
	movw	%bx, %ss
	movw	$_estack16, %sp

	/* Jump to .text16 segment */
	pushw	%ax
	pushw	$1f
	lret
	.section ".text16", "awx", @progbits
1:	/* Call main() */
	pushl	$main
	pushw	%cs
	call	prot_call
	popl	%ecx /* discard */

	/* Uninstall gPXE */
	call	uninstall

	/* Restore BIOS stack */
	movw	%dx, %ss
	movw	%bp, %sp

	/* Check magic word on BIOS stack */
	popl	%eax
	cmpl	$STACK_MAGIC, %eax
	jne	1f
	/* BIOS stack OK: return to caller */
	lret
1:	/* BIOS stack corrupt: use INT 18 */
	int	$0x18
	.previous

exec_message:
	.asciz	" starting execution\n"
	.size exec_message, . - exec_message

/* Wait for key press specified by %bl (masked by %bh)
 *
 * Used by init and INT19 code when prompting user.  If the specified
 * key is pressed, it is left in the keyboard buffer.
 *
 * Returns with ZF set iff specified key is pressed.
 */
wait_for_key:
	/* Preserve registers */
	pushw	%cx
	pushw	%ax
1:	/* Empty the keyboard buffer before waiting for input */
	movb	$0x01, %ah
	int	$0x16
	jz	2f
	xorw	%ax, %ax
	int	$0x16
	jmp	1b
2:	/* Wait for a key press */
	movw	$ROM_BANNER_TIMEOUT, %cx
3:	decw	%cx
	js	99f		/* Exit with ZF clear */
	/* Wait for timer tick to be updated */
	call	wait_for_tick
	/* Check to see if a key was pressed */
	movb	$0x01, %ah
	int	$0x16
	jz	3b
	/* Check to see if key was the specified key */
	andb	%bh, %al
	cmpb	%al, %bl
	je	99f		/* Exit with ZF set */
	/* Not the specified key: remove from buffer and stop waiting */
	pushfw
	xorw	%ax, %ax
	int	$0x16
	popfw			/* Exit with ZF clear */
99:	/* Restore registers and return */
	popw	%ax
	popw	%cx
	ret
	.size wait_for_key, . - wait_for_key

/* Wait for timer tick
 *
 * Used by wait_for_key
 */
wait_for_tick:
	pushl	%eax
	pushw	%fs
	movw	$0x40, %ax
	movw	%ax, %fs
	movl	%fs:(0x6c), %eax
1:	pushf
	sti
	hlt
	popf
	cmpl	%fs:(0x6c), %eax
	je	1b
	popw	%fs
	popl	%eax
	ret
	.size wait_for_tick, . - wait_for_tick
