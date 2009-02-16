/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 *
 * Also be very careful as the stack is at the rear end of the interrupt
 * table so using a noticeable amount of stack space is a no-no.
 */

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

	.text
	.code16
	.arch i386
	.section ".prefix", "ax", @progbits
	
	.org	0x00
romheader:
	.word	0xAA55			/* BIOS extension signature */
romheader_size:	.byte _filesz_sect	/* Size in 512-byte blocks */
	jmp	init			/* Initialisation vector */
checksum:
	.byte	0
	.org	0x16
	.word	undiheader
	.org	0x18
	.word	pciheader
	.org	0x1a
	.word	pnpheader
	.size romheader, . - romheader
	
	.section ".zinfo.fixup", "a", @progbits	/* Compressor fixups */
	.ascii	"SUBB"
	.long	romheader_size
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
	.word	_filesz_sect		/* Image length */
	.word	0x0001			/* Revision level */
	.byte	0x00			/* Code type */
	.byte	0x80			/* Last image indicator */
pciheader_runtime_length:
	.word	_filesz_sect		/* Maximum run-time image length */
	.word	0x0000			/* Configuration utility code header */
	.word	0x0000			/* DMTF CLP entry point */
	.equ pciheader_len, . - pciheader
	.size pciheader, . - pciheader
	
	.section ".zinfo.fixup", "a", @progbits	/* Compressor fixups */
	.ascii	"SUBW"
	.long	pciheader_image_length
	.long	512
	.long	0
	.ascii	"SUBW"
	.long	pciheader_runtime_length
	.long	512
	.long	0
	.previous

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

	/* Check for PnP BIOS */
	testw	$0x0f, %bx	/* PnP signature must be aligned - bochs    */
	jnz	no_bbs		/* uses unalignment to indicate 'fake' PnP. */
	cmpl	$PNP_SIGNATURE, %es:0(%bx)
	jne	no_bbs
	/* Is PnP: print PnP message */
	movw	$init_message_pnp, %si
	xorw	%di, %di
	call	print_message
	/* Check for BBS */
	pushw	%es:0x1b(%bx)	/* Real-mode data segment */
	pushw	%ds		/* &(bbs_version) */
	pushw	$bbs_version
	pushw	$PNP_GET_BBS_VERSION
	lcall	*%es:0xd(%bx)
	addw	$8, %sp
	testw	%ax, %ax
	je	got_bbs
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
	movzbl	romheader_size, %eax
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
pmm_fail:
	/* Restore upper register halves */
	popal
no_pmm:

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
	jnz	2f
	/* Ctrl-B was pressed: invoke gPXE.  The keypress will be
	 * picked up by the initial shell prompt, and we will drop
	 * into a shell.
	 */
	pushw	%cs
	call	exec
2:
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
image_source:
	.long	0
	.size	image_source, . - image_source

/* Temporary decompression area
 *
 * May be either at HIGHMEM_LOADPOINT, or within PMM-allocated block.
 */
decompress_to:
	.long	HIGHMEM_LOADPOINT
	.size	decompress_to, . - decompress_to

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

/* UNDI loader
 *
 * Called by an external program to load our PXE stack.
 */
undiloader:
	/* Save registers */
	pushl	%esi
	pushl	%edi
	pushw	%ds
	pushw	%es
	pushw	%bx
	/* ROM segment address to %ds */
	pushw	%cs
	popw	%ds
	/* UNDI loader parameter structure address into %es:%di */
	movw	%sp, %bx
	movw	%ss:18(%bx), %di
	movw	%ss:20(%bx), %es
	/* Install to specified real-mode addresses */
	pushw	%di
	movw	%es:12(%di), %bx
	movw	%es:14(%di), %ax
	movl	image_source, %esi
	movl	decompress_to, %edi
	call	install_prealloc
	popw	%di
	/* Call UNDI loader C code */
	pushl	$pxe_loader_call
	pushw	%cs
	pushw	$1f
	pushw	%ax
	pushw	$prot_call
	lret
1:	popw	%bx	/* discard */
	popw	%bx	/* discard */
	/* Restore registers and return */
	popw	%bx
	popw	%es
	popw	%ds
	popl	%edi
	popl	%esi
	lret
	.size undiloader, . - undiloader

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
