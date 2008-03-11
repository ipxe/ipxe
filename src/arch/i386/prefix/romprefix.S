/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 *
 * Also be very careful as the stack is at the rear end of the interrupt
 * table so using a noticeable amount of stack space is a no-no.
 */

#define PNP_SIGNATURE ( '$' + ( 'P' << 8 ) + ( 'n' << 16 ) + ( 'P' << 24 ) )
#define PMM_SIGNATURE ( '$' + ( 'P' << 8 ) + ( 'M' << 16 ) + ( 'M' << 24 ) )
#define STACK_MAGIC ( 'L' + ( 'R' << 8 ) + ( 'E' << 16 ) + ( 'T' << 24 ) )
#define PNP_GET_BBS_VERSION 0x60

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
	.word	pci_vendor_id		/* Vendor ID */ 
	.word	pci_device_id		/* Device ID */
	.word	0x0000			/* pointer to vital product data */
	.word	pciheader_len		/* PCI data structure length */
	.byte	0x00			/* PCI data structure revision */
	.byte	0x02			/* Device Base Type code */
	.byte	0x00			/* Device Sub-Type code */
	.byte	0x00			/* Device Interface Type code */
pciheader_size:	.word _load_size_sect	/* Image length same as offset 02h */
	.word	0x0001			/* revision level of code/data */
	.byte	0x00			/* code type */
	.byte	0x80			/* Flags (last PCI data structure) */
	.word	0x0000			/* reserved */
	.equ pciheader_len, . - pciheader
	.size pciheader, . - pciheader
	
	.section ".zinfo.fixup", "a"	/* Compressor fixup information */
	.ascii	"SUBW"
	.long	pciheader_size
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
	.byte	0x54			/* Device indicator */
	.word	0x0000			/* Boot connection vector */
	.word	0x0000			/* Disconnect vector */
	.word	bev_entry		/* Boot execution vector */
	.word	0x0000			/* Reserved */
	.word	0x0000			/* Static resource information vector*/
	.equ pnpheader_len, . - pnpheader
	.size pnpheader, . - pnpheader

mfgstr:
	.asciz	"http://etherboot.org"
	.size mfgstr, . - mfgstr
prodstr:
	.asciz	"gPXE"
	.size prodstr, . - prodstr
	
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
	cld
	pushw	%cs
	popw	%ds
	/* Print message as early as possible */
	movw	$init_message, %si
	call	print_message
	/* Check for PnP BIOS */
	testw	$0x0f, %di	/* PnP signature must be aligned - bochs    */
	jnz	hook_int19	/* uses unalignment to indicate 'fake' PnP. */
	cmpl	$PNP_SIGNATURE, %es:0(%di)
	jne	hook_int19
	/* Is PnP: print PnP message */
	movw	$init_message_pnp, %si
	call	print_message
	xchgw	%bx, %bx
	/* Check for BBS */
	pushw	%es:0x1b(%di)	/* Real-mode data segment */
	pushw	%ds		/* &(bbs_version) */
	pushw	$bbs_version
	pushw	$PNP_GET_BBS_VERSION
	lcall	*%es:0xd(%di)
	addw	$16, %sp
	testw	%ax, %ax
	jne	hook_int19
	movw	$init_message_bbs, %si
	call	print_message
	jmp	hook_bbs
	/* Not BBS-compliant - must hook INT 19 */
hook_int19:
	movw	$init_message_int19, %si
	call	print_message
	xorw	%ax, %ax
	movw	%ax, %es
	pushw	%cs
	pushw	$int19_entry
	popl	%es:( 0x19 * 4 )
hook_bbs:
	/* Check for PMM */
	movw	$( 0xe000 - 1 ), %di
pmm_scan:
	incw	%di
	jz	no_pmm
	movw	%di, %es
	cmpl	$PMM_SIGNATURE, %es:0
	jne	pmm_scan
	xorw	%bx, %bx
	xorw	%si, %si
	movzbw	%es:5, %cx
1:	es lodsb
	addb	%al, %bl
	loop	1b
	jnz	pmm_scan
	/* PMM found: print PMM message */
	movw	$init_message_pmm, %si
	call	print_message
	/* Try to allocate 2MB block via PMM */
	pushw	$0x0006		/* Aligned, extended memory */
	pushl	$0xffffffff	/* No handle */
	pushl	$( 0x00200000 / 16 ) /* 2MB in paragraphs */
	pushw	$0x0000		/* pmmAllocate */
	lcall	*%es:7
	addw	$12, %sp
	testw	%dx, %dx	/* %ax==0 even on success, since align=2MB */
	jnz	gotpmm
	movw	$init_message_pmm_failed, %si
	call	print_message
	jmp	no_pmm
gotpmm:	/* PMM allocation succeeded: copy ROM to PMM block */
	pushal			/* PMM presence implies 1kB stack */
	movw	%ax, %es	/* %ax=0 already - see above */
	pushw	%dx
	pushw	%ax
	popl	%edi
	movl	%edi, image_source
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
	/* Print CRLF to terminate messages */
	movw	$'\n', %ax
	call	print_character
	/* Restore registers */
	popw	%es
	popw	%ds
	popaw
	/* Indicate boot capability to PnP BIOS, if present */
	movw	$0x20, %ax
	lret
	.size init, . - init

init_message:
	.asciz	"gPXE (http://etherboot.org) -"
	.size	init_message, . - init_message
init_message_pnp:
	.asciz	" PnP"
	.size init_message_pnp, . - init_message_pnp
init_message_bbs:
	.asciz	" BBS"
	.size init_message_bbs, . - init_message_bbs
init_message_pmm:
	.asciz	" PMM"
	.size init_message_pmm, . - init_message_pmm
init_message_pmm_failed:
	.asciz	"(failed)"
	.size init_message_pmm_failed, . - init_message_pmm_failed
init_message_int19:
	.asciz	" INT19"
	.size init_message_int19, . - init_message_int19

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
 * Called via the hooked INT 19 if we detected a non-PnP BIOS.
 */
int19_entry:
	pushw	%cs
	call	exec
	/* No real way to return from INT19 */
	int	$0x18
	.size	int19_entry, . - int19_entry

/* Execute as a boot device
 *
 */
exec:	/* Set %ds = %cs */
	pushw	%cs
	popw	%ds

	/* Print message as soon as possible */
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
	.asciz	"gPXE starting boot\n"
	.size exec_message, . - exec_message

/* UNDI loader
 *
 * Called by an external program to load our PXE stack.
 */
undiloader:
	/* Save registers */
	pushl	%esi
	pushl	%edi
	pushw	%es
	pushw	%bx
	/* UNDI loader parameter structure address into %es:%di */
	movw	%sp, %bx
	movw	%ss:12(%bx), %di
	movw	%ss:14(%bx), %es
	/* Install to specified real-mode addresses */
	pushw	%di
	movw	%es:12(%di), %bx
	movw	%es:14(%di), %ax
	movl	%cs:image_source, %esi
	movl	%cs:decompress_to, %edi
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
	popl	%edi
	popl	%esi
	lret
	.size undiloader, . - undiloader
