OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)

/* Linker-generated symbols are prefixed with a double underscore.
 * Decompressor symbols are prefixed with __decompressor_.  All other
 * symbols are the same as in the original object file, i.e. the
 * runtime addresses.
 */

ENTRY(_start16)

SECTIONS {
	.text : {
		*(.text)
	}
	.payload : {
		__payload_start = .;
		*(.data)
		__payload_end = .;
	}

	/* _payload_size is the size of the binary image appended to
	 * start16, in bytes.
	 */
	__payload_size = __payload_end - __payload_start ;

	/* _size is the size of the runtime image
	 * (start32 + the C code), in bytes.
	 */
	__size = _end - _start ;

	/* _decompressor_size is the size of the decompressor, in
	 * bytes.  For a non-compressed image, start16.lds sets
	 * _decompressor_uncompressed = _decompressor__start = 0.
	 */
	__decompressor_size = __decompressor_uncompressed - __decompressor__start ;

	/* image__size is the total size of the image, after
	 * decompression and including the decompressor if applicable.
	 * It is therefore the amount of memory that start16's payload
	 * needs in order to execute, in bytes.
	 */
	__image_size = __size + __decompressor_size ;

	/* Amount to add to runtime symbols to obtain the offset of
	 * that symbol within the image.
	 */
	__offset_adjust = __decompressor_size - _start ;

	/* Calculations for the stack
	 */
	__stack_size = _estack - _stack ;
	__offset_stack = _stack + __offset_adjust ;

	/* Some symbols will be larger than 16 bits but guaranteed to
	 * be multiples of 16.  We calculate them in paragraphs and
	 * export these symbols which can be used in 16-bit code
	 * without risk of overflow.
	 */
	__image_size_pgh = ( __image_size / 16 );
	__start_pgh = ( _start / 16 );
	__decompressor_size_pgh = ( __decompressor_size / 16 );
	__offset_stack_pgh = ( __offset_stack / 16 );
}

