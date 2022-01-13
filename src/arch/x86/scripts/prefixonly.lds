/* -*- ld-script -*- */

/*
 * Linker script for prefix-only binaries (e.g. USB disk MBR)
 *
 */

SECTIONS {

    .prefix 0x0 : AT ( 0x0 ) {
	*(.prefix)
    }

    /DISCARD/ : {
	*(.comment)
	*(.comment.*)
	*(.note)
	*(.note.*)
	*(.eh_frame)
	*(.eh_frame.*)
	*(.rel)
	*(.rel.*)
	*(.einfo)
	*(.einfo.*)
	*(.discard)
	*(.discard.*)
	*(.sbat)
	*(.sbat.*)
    }

}
