/*****************************************************************************
 * NOTE: This code is no longer used in Etherboot.  The obsolete
 * Makefile target .lzrom refers to it, but it is no longer being
 * maintained and may no longer work.  Use .zrom instead (which uses
 * the unnrv2b decompressor).
 *****************************************************************************
 */
	
/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 */


/* LZHuf (LZSS) Decompressing boot loader for ROM images
 *
 * this code is based on the work of Haruyasu Yoshizaki and Haruhiko Okumura
 * who implemented the original compressor and decompressor in C code
 *
 * Converted to 32bit assembly 16 July 2002 Eric Biederman <ebiederman@lnxi.com>
 * Made PIC 10 Aug 2002 Eric Biederman <ebiederman@lnxi.com>
 *
 * Copyright 1997 by M. Gutschke <gutschk@math.uni-muenster.de>
 *
 * Compression pays off, as soon as the uncompressed image is bigger than
 * about 1.5kB. This assumes an average compressibility of about 60%.
 */


/* Do not change these values unless you really know what you are doing
 * the pre-computed lookup tables rely on the buffer size being 4kB or
 * smaller. The buffer size must be a power of two. The lookahead size has
 * to fit into 6 bits. If you change any of these numbers, you will also
 * have to adjust the compressor accordingly.
 */
#define BUFSZ		4096
#define LOOKAHEAD	60
#define THRESHOLD	2
#define NCHAR		(256+LOOKAHEAD-THRESHOLD)
#define TABLESZ		(NCHAR+NCHAR-1)
#define ROOT		(TABLESZ-1)

	.text
	.arch i386
	.globl _start
_start:
	cli

	/* Save the initial register values */
	pushal

	/*
	 * See where I am running, and compute %ebp
	 */
	call	1f
1:	pop	%ebp
	subl	$1b, %ebp

/*
 * INIT -- initializes all data structures
 * ====
 */

init:	
	cld
	leal	dcodrle(%ebp), %esi	/* uncompress run length encoded */
	leal	dcode(%ebp), %edi			/* lookup table for codes */
	movb	$6, %dl
	movb	$0x20, %dh
	xorb	%bh,%bh
init0:	
	lodsb
	movb	%al,%bl
init1:
	xorl	%ecx, %ecx
	movb	%dh,%cl
	movb	%bh,%al
	rep
	stosb
	incb	%bh
	decb	%bl
	jnz	init1
	shrb	%dh
	decb	%dl
	jnz	init0
	movb	$1, %bl			/* uncompress run length encoded */
	movb	$6, %bh			/* lookup table for code lengths */
init2:	
	lodsb
	xorl	%ecx, %ecx
	movb	%al,%cl
	movb	%bl,%al
	rep
	stosb
	incb	%bl
	decb	%bh
	jnz	init2

	movl	$NCHAR, %ecx		/* set all frequencies of leaf nodes */
	movw	$1, %ax			/* to one */
	rep
	stosw
	leal	freq(%ebp), %esi
	movl	$ROOT+1-NCHAR, %ecx
init3:	
	lodsw				/* update frequencies of non-leaf nodes */
	movw	%ax,%bx
	lodsw
	addw	%bx,%ax
	stosw
	loop	init3
	movw	$0xFFFF, %ax
	stosw				/* sentinel with infinite frequency */
	movl	$NCHAR, %ecx
	movw	$TABLESZ, %ax
init4:	
	stosw				/* update son pointers for leaf nodes */
	incw	%ax
	loop	init4
	movl	$ROOT+1-NCHAR, %ecx
	xorw	%ax,%ax
init5:	
	stosw				/* update son ptrs for non-leaf nodes */
	addw	$2, %ax
	loop	init5
	movl	$ROOT+1-NCHAR, %ecx
	movw	$NCHAR, %ax
init6:	
	stosw				/* update parent ptrs for non-leaf nd. */
	stosw
	incw	%ax
	loop	init6
	movl	$NCHAR, %ecx
	xorw	%ax,%ax
	stosw				/* root node has no parent */
init7:	
	stosw				/* update parent ptrs for leaf nodes */
	incw	%ax
	loop	init7
	xorw	%ax,%ax
	stosb				/* clear getlen */
	stosw				/* clear getbuf */
	movb	$0x20, %al		/* fill text buffer with spaces */
	leal	spaces(%ebp), %edi
	movl	$BUFSZ-LOOKAHEAD, %ecx
	rep
	
	stosb
	/* fall thru */

/* 
 * MAIN -- reads compressed codes and writes decompressed data
 * ====
 */

	leal	_payload(%ebp), %esi	/* get length of compressed data stream */
	leal	uncompressed(%ebp), %edi

	lodsl
	movl	%eax, %ecx
main1:	
	pushl	%ecx
	call	dcdchr			/* decode one code symbol */
	orb	%ah,%ah			/* test if 8bit character */
	jnz	main2
	stosb				/* store verbatim */
	popl	%ecx
	loop	main1			/* proceed with next compressed code */
	jmp	done			/* until end of input is detected */
main2:	
	pushl	%eax
	call	dcdpos			/* compute position in output buffer */
	movl	%esi, %eax
	subl	%edi, %ebx
	notl	%ebx
	movl	%ebx, %esi		/* si := di - dcdpos() - 1 */
	popl	%ecx
	subl	$255-THRESHOLD, %ecx	/* compute length of code sequence */
	movl	%ecx, %edx
	rep
	movsb
	movl	%eax,%esi
	popl	%ecx
	subl	%edx, %ecx		/* check end of input condition */
	jnz	main1			/* proceed with next compressed code */
done:
	/* Start Etherboot */
	popal
	jmp	uncompressed
/*
 * GETBIT -- gets one bit pointed to by DS:ESI
 * ======
 *
 * changes: AX,CX,DL
 */

getbit:
	movb	$8, %cl
	movb	getlen(%ebp), %dl	/* compute number of bits required */
	subb	%dl,%cl			/* to fill read buffer */
	jae	getbit1
	movw	getbuf(%ebp), %ax	/* there is still enough read ahead data */
	jmp	getbit2
getbit1:
	lodsb				/* get next byte from input stream */
	xorb	%ah,%ah
	shlw	%cl,%ax			/* shift, so that it will fit into */
	movw	getbuf(%ebp), %cx	/* read ahead buffer */
	orw	%cx,%ax
	addb	$8, %dl			/* update number of bits in buffer */
getbit2:
	movw	%ax,%cx
	shlw	%cx			/* extract one bit from buffer */
	movw	%cx, getbuf(%ebp)
	decb	%dl
	movb	%dl, getlen(%ebp)	/* and update number of bits */
	shlw	%ax			/* return in carry flag */
	ret


/*
 * DCDPOS -- decodes position in textbuffer as pointed to by DS:SI, result in BX
 * ======
 *
 * changes: AX,EBX,ECX,DX
 */

dcdpos:
	movl	$0x0800, %ebx
dcdpos1:
	shlb	%bl			/* read one byte */
	call	getbit
	jnc	dcdpos2
	incb	%bl
dcdpos2:
	decb	%bh
	jnz	dcdpos1
	movb	%bl,%dh			/* read length of code from table */
	xorb	%bh,%bh
	xorl	%ecx, %ecx
	movb	dlen(%ebx, %ebp),%cl
	movb	dcode(%ebx, %ebp),%bl	/* get top six bits from table */
	shll	$6, %ebx
dcdpos3:
	pushl	%ecx			/* read the rest from the input stream */
	shlb	%dh
	call	getbit
	jnc	dcdpos4
	incb	%dh
dcdpos4:
	popl	%ecx
	loop	dcdpos3
	andb	$0x3f, %dh		/* combine upper and lower half of code */
	orb	%dh,%bl
	ret

/*
 * DCDCHR -- decodes one compressed character pointed to by DS:SI
 * ======
 *
 * changes: AX,BX,CX,DX
 */

dcdchr:
	movl	$ROOT, %ebx		/* start at root entry */
	shll	%ebx
	movzwl	son(%ebx, %ebp),%ebx
dcdchr1:
	call	getbit			/* get a single bit */
	jnc	dcdchr2
	incl	%ebx			/* travel left or right */
dcdchr2:
	shll	%ebx
	movzwl	son(%ebx, %ebp), %ebx
	cmpl	$TABLESZ, %ebx		/* until we come to a leaf node */
	jb	dcdchr1
	movl	%ebx, %eax
	subl	$TABLESZ, %eax
	/* fall thru */

/*
 * UPDATE -- updates huffman tree after incrementing frequency for code in BX
 * ======
 *
 * changes: BX,CX,DX
 */

update:
	/* we do not check whether the frequency count has overrun.
	 * this will cause problems for large files, but be should be fine
	 * as long as the compressed size does not exceed 32kB and we
	 * cannot do more than this anyways, because we load into the
	 * upper 32kB of conventional memory
	 */
	pushl	%esi
	pushl	%eax
	shll	%ebx
	movzwl	parent(%ebx, %ebp),%ebx
update1:
	shll	%ebx
	movzwl	freq(%ebx, %ebp), %edx
	incl	%edx				/* increment frequency count by one */
	movw	%dx, freq(%ebx, %ebp)
	leal	2+freq(%ebx, %ebp), %esi
	lodsw					/* check if nodes need reordering */
	cmpw	%ax, %dx
	jbe	update5
update2:
	lodsw
	cmpw	%dx, %ax
	jb	update2
	movzwl	-4(%esi), %ecx
	movw	%cx, freq(%ebx, %ebp)		/* swap frequency of entries */
	movw	%dx, -4(%esi)

	movl	%esi, %eax			/* compute index of new entry */
	subl	$freq+4, %eax
	subl	%ebp, %eax
	
	movl	%eax, %edx
	shrl	%eax
	movzwl	son(%ebx, %ebp), %ecx		/* get son of old entry */
	movl	%ecx, %esi
	addl	%esi, %esi
	movw	%ax, parent(%esi, %ebp)		/* and update the ptr to new parent */
	cmpl	$TABLESZ, %ecx
	jae	update3				/* do this for both branches */
	movw	%ax, parent+2(%esi, %ebp)	/* if not a leaf node */
update3:
	movl	%edx, %esi
	movzwl	son(%esi, %ebp), %edx		/* get son of new entry */
	movw	%cx, son(%esi, %ebp)		/* update its contents */
	movl	%edx, %esi
	addl	%esi, %esi
	movl	%ebx, %ecx
	shrl	%ecx
	movw	%cx, parent(%esi, %ebp)		/* and update the ptr to new paren */
	cmpl	$TABLESZ, %edx
	jae	update4				/* do this for both branches */
	movw	%cx, parent+2(%esi, %ebp)	/* if not a leaf node */
update4:
	movw	%dx, son(%ebx, %ebp)		/* update son of old entry */
	movl	%eax, %ebx			/* continue with new entry */
	shll	%ebx
update5:
	movzwl	parent(%ebx, %ebp), %ebx	/* continue with parent */
	orl	%ebx, %ebx
	jnz	update1				/* until we found the root entry */
	popl	%eax
	popl	%esi
	ret

/*
 * constant data. this part of the program resides in ROM and cannot be
 * changed
 *
 * run length encoded tables will be uncompressed into the bss segment
 * take care with any symbols here for .com files to add 0x100 to address
 */

dcodrle: .byte 0x01,0x03,0x08,0x0C,0x18,0x10
dlenrle: .byte 0x20,0x30,0x40,0x30,0x30,0x10

/*
 * variable data segment (bss)
 * this segment will always be found at 0x90000 (i.e. at RELOC - SCRATCH)
 *
 * do not change the order or the sizes of any of the following tables
 * the initialization code makes assumptions on the exact layout of the
 * data structures...
 */

.bss
/* lookup table for index into buffer of recently output characters */
dcode:	.skip	256
	
/* lookup table for length of code sequence from buffer of recent characters */
dlen:	.skip	256
	
/* table with frequency counts for all codes */
freq:	.skip	2*(TABLESZ+1)
	
/* pointer to child nodes */
son:	.skip	2*(TABLESZ)
	
/* the first part of this table contains all the codes	(0..TABLESZ-1) */
/* the second part contains all leaf nodes		(TABLESZ..) */
parent:	.skip	2*(TABLESZ+NCHAR)
	
/* temporary storage for extracting bits from compressed data stream */
getlen:	.skip	1
getbuf:	.skip	1

	/* the initial buffer has to be filled with spaces  */
	.balign 4
spaces:
	.skip	BUFSZ - LOOKAHEAD
	/* uncompressed data will be written here */
uncompressed:

