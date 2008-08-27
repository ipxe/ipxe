/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 *
 * Also be very careful as the stack is at the rear end of the interrupt
 * table so using a noticeable amount of stack space is a no-no.
 */

#define PNP_SIGNATURE ( '$' + ( 'P' << 8 ) + ( 'n' << 16 ) + ( 'P' << 24 ) )
#define PMM_SIGNATURE ( '$' + ( 'P' << 8 ) + ( 'M' << 16 ) + ( 'M' << 24 ) )
#define PCI_SIGNATURE ( 'P' + ( 'C' << 8 ) + ( 'I' << 16 ) + ( ' ' << 24 ) )
#define STACK_MAGIC ( 'L' + ( 'R' << 8 ) + ( 'E' << 16 ) + ( 'T' << 24 ) )
#define PNP_GET_BBS_VERSION 0x60
#define PMM_ALLOCATE 0x0000

	.text
	.code16
	.arch i386
	.section ".prefix", "ax", @progbits
	
	.org	0x00
romheader:
	.word	0xAA55			/* BIOS extension signature */
romheader_size:	.byte _load_size_sect	/* Size in 512-byte blocks */
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
	
	.section ".zinfo.fixup", "a"	/* Compressor fixup information */
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
	.word	_load_size_sect		/* Image length */
	.word	0x0001			/* Revision level */
	.byte	0x00			/* Code type */
	.byte	0x80			/* Last image indicator */
pciheader_runtime_length:
	.word	_load_size_sect		/* Maximum run-time image length */
	.word	0x0000			/* Configuration utility code header */
	.word	0x0000			/* DMTF CLP entry point */
	.equ pciheader_len, . - pciheader
	.size pciheader, . - pciheader
	
	.section ".zinfo.fixup", "a"	/* Compressor fixup information */
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
 * Defaults to "gPXE".  If the ROM image is writable at initialisation
 * time, it will be filled in to include the PCI bus:dev.fn number of
 * the card as well.
 */
prodstr:
	.ascii	"gPXE"
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
	.word	_data16_size		/* Stack segment size */
	.word	_data16_size		/* Data segment size */
	.word	_text16_size		/* Code segment size */
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
	pushw	$0x40
	popw	%fs

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
	movb	$' ', prodstr_separator

	/* Print segment address */
	movb	$' ', %al
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
	jc	1f
	cmpl	$PCI_SIGNATURE, %edx
	jne	1f
	testb	%ah, %ah
	jnz	1f
	movw	$init_message_pci, %si
	xorw	%di, %di
	call	print_message
	movb	%bh, %al
	call	print_hex_nibble
	movb	$'.', %al
	call	print_character
	movb	%bl, %al
	call	print_hex_byte
	cmpb	$3, %bh
	jae	2f
1:	/* PCI <3.0: set %gs (runtime segment) = %cs (init-time segment) */
	pushw	%cs
	popw	%gs
2:	popl	%edi
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
	/* Try to allocate 2MB block via PMM */
	pushw	$0x0006		/* Aligned, extended memory */
	pushl	$0xffffffff	/* No handle */
	pushl	$( 0x00200000 / 16 ) /* 2MB in paragraphs */
	pushw	$PMM_ALLOCATE
	lcall	*%es:7
	addw	$12, %sp
	movw	%dx, %ax
	xorw	%di, %di
	call	print_hex_word
	movw	%dx, ( image_source + 2 )
	testw	%dx, %dx	/* %ax==0 even on success, since align=2MB */
	jz	no_pmm
	/* PMM allocation succeeded: copy ROM to PMM block */
	pushal			/* PMM presence implies 1kB stack */
	xorw	%ax, %ax
	movw	%ax, %es
	movl	image_source, %edi
	xorl	%esi, %esi
	movzbl	romheader_size, %ecx
	shll	$9, %ecx
	addr32 rep movsb	/* PMM presence implies flat real mode */
	movl	%edi, decompress_to
	/* Shrink ROM and update checksum */
	xorw	%bx, %bx
	xorw	%si, %si
	movw	$_prefix_size_sect, %cx
	movb	%cl, romheader_size
	shlw	$9, %cx
1:	lodsb
	addb	%al, %bl
	loop	1b
	subb	%bl, checksum
	popal
no_pmm:

	/* Copy self to option ROM space.  Required for PCI3.0, which
	 * loads us to a temporary location in low memory.  Will be a
	 * no-op for lower PCI versions.
	 */
	movb	$' ', %al
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
	/* Empty the keyboard buffer before waiting for input */
empty_keyboard_buffer:
	movb	$0x01, %ah
	int	$0x16
	jz	1f
	xorw	%ax, %ax
	int	$0x16
	jmp	empty_keyboard_buffer
1:	/* Wait for up to 3s for a key press */
	movw	$(18 * 3), %cx	/* Approx 3s worth of timer ticks */
wait_for_key:
	decw	%cx
	jz	no_key_pressed
	/* Wait for timer tick to be updated */
	movl	%fs:(0x6c), %eax
1:	pushf
	sti
	hlt
	popf
	cmpl	%fs:(0x6c), %eax
	je	1b
	/* Check to see if a key was pressed */
	movb	$0x01, %ah
	int	$0x16
	jz	wait_for_key
	/* Check to see if key was Ctrl-B */
	cmpb	$0x02, %al
	je	1f
	/* Key was not Ctrl-B: remove from buffer and stop waiting */
	xorw	%ax, %ax
	int	$0x16
	jmp	no_key_pressed
1:	/* Key was Ctrl-B: leave in keyboard buffer and invoke gPXE.
	 * The keypress will be picked up by the initial shell
	 * prompt, and we will drop into a shell.
	 */
	pushw	%cs
	call	exec
no_key_pressed:

	/* Print blank lines to terminate messages */
	movw	$init_message_end, %si
	xorw	%di, %di
	call	print_message

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

init_message:
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
	.asciz	"\nPress Ctrl-B to configure gPXE..."
	.size	init_message_prompt, . - init_message_prompt
init_message_end:
	.asciz	"\n\n\n"
	.size	init_message_end, . - init_message_end

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
 * attempt to return via the original INT 19 vector (if we were able to
 * store it).
 */
int19_entry:
	pushw	%cs
	call	exec
	movl	%cs:orig_int19, %eax
	testl	%eax, %eax
	je	1f
	/* Chain to original INT 19 vector */
	ljmp	*%cs:orig_int19
1:	/* No chained vector: issue INT 18 as a last resort */
	int	$0x18
	.size	int19_entry, . - int19_entry
orig_int19:
	.long	0
	.size	orig_int19, . - orig_int19

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
	/* No need to clean up stack; we are about to reload %ss:sp */
	
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
