#include <stdio.h>
#include <gpxe/init.h>
#include <console.h>
#include <realmode.h>

extern char __text[];
extern char __rodata[];
extern char __data[];
extern char __bss[];
extern char __text16[];
extern char __data16[];


static void gdb_symbol_line ( void ) {
	printf ( "Commands to start up gdb:\n\n" );
	printf ( "gdb\n" );
	printf ( "target remote localhost:1234\n" );
	printf ( "set confirm off\n" );
	printf ( "add-symbol-file symbols %#lx", virt_to_phys ( __text ) );
	printf ( " -s .rodata %#lx", virt_to_phys ( __rodata ) );
	printf ( " -s .data %#lx", virt_to_phys ( __data ) );
	printf ( " -s .bss %#lx", virt_to_phys ( __bss ) );
	printf ( " -s .text16 %#x", ( ( rm_cs << 4 ) + (int)__text16 ) );
	printf ( " -s .data16 %#x", ( ( rm_ds << 4 ) + (int)__data16 ) );
	printf ( "\n" );
	printf ( "add-symbol-file symbols 0\n" );
	printf ( "set confirm on\n" );
	getkey();
}

INIT_FN ( INIT_GDBSYM, gdb_symbol_line, NULL );
