/* Placeholder for decompress16 in non-compressed images */

	.text
	.arch i386
	.section ".prefix.lib", "ax", @progbits

	.code16
	.globl	nodecompress16
nodecompress16:
	rep addr32 movsb
	ret

	/* File split information for the compressor */
	.section ".zinfo", "a"
	.ascii	"COPY"
	.long	_prefix_load_offset
	.long	_load_size
	.long	_max_align
