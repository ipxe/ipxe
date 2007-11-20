/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 *
 * Also be very careful as the stack is at the rear end of the interrupt
 * table so using a noticeable amount of stack space is a no-no.
 */

	.text
	.code16
	.arch i386
	.section ".prefix", "ax", @progbits
	
	.org	0x00
romheader:
	.word	0xAA55			/* BIOS extension signature */
romheader_size:	.byte _load_size_sect	/* Size in 512-byte blocks */
	jmp	init_vector		/* Initialisation vector */
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
	.word	exec_vector		/* Boot execution vector */
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

/* Initialisation vector
 *
 * Determine whether or not this is a PnP system via a signature
 * check.  If it is PnP, return to the PnP BIOS indicating that we are
 * a boot-capable device; the BIOS will call our boot execution vector
 * if it wants to boot us.  If it is not PnP, hook INT 19.
 */
init_vector:
	pushw	%si
	cmpw	$'$'+'P'*256, %es:0(%di)
	jne	notpnp
	cmpw	$'n'+'P'*256, %es:2(%di)
	jne	notpnp
ispnp:
	movw	$ispnp_message, %si
	jmp	99f
notpnp:
	pushw	%ds
	pushw	$0
	popw	%ds
	pushw	%cs
	pushw	$exec_vector
	popl	( 0x19 * 4 )
	popw	%ds
	movw	$notpnp_message, %si
99:
	call	print_message
	movw	$0x20, %ax
	popw	%si
	lret
	.size init_vector, . - init_vector

ispnp_message:
	.asciz	"gPXE detected PnP BIOS\r\n"
	.size ispnp_message, . - ispnp_message
notpnp_message:
	.asciz	"gPXE detected non-PnP BIOS\r\n"
	.size notpnp_message, . - notpnp_message

/* Boot execution vector
 *pciheader_size
 * Called by the PnP BIOS when it wants to boot us, or via the hooked
 * INT 19 if we detected a non-PnP BIOS.
 */	
exec_vector:
	/* Obtain a reasonably-sized stack */
	xorw	%ax, %ax
	movw	%ax, %ss
	movw	$0x7c00, %sp
	
	movw	$exec_message, %si
	call	print_message

	call	install

	/* Set up real-mode stack */
	movw	%bx, %ss
	movw	$_estack16, %sp

	/* Jump to .text16 segment */
	pushw	%ax
	pushw	$1f
	lret
	.section ".text16", "awx", @progbits
1:
	pushl	$main
	pushw	%cs
	call	prot_call
	popl	%eax /* discard */

	/* Boot next device */
	int	$0x18
	.previous

exec_message:
	.asciz	"gPXE starting boot\r\n"
	.size exec_message, . - exec_message

/* UNDI loader
 *
 * Called by an external program to load our PXE stack.
 */
undiloader:
	/* Save registers */
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
	lret
	.size undiloader, . - undiloader
				
/* Utility function: print string
 */
print_message:
	pushw	%ax
	pushw	%bx
	pushw	%bp
	movw    $0x0007, %bx
1:	cs lodsb
	testb	%al, %al
	je	2f
	movb    $0x0e, %ah              /* write char, tty mode */
	int     $0x10
	jmp	1b
2:	popw	%bp
	popw	%bx
	popw	%ax
	ret
	.size print_message, . - print_message
