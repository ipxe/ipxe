#include <stdio.h>
#include <realmode.h>

void __cdecl _dump_regs ( struct i386_all_regs *ix86 ) {

	__asm__ __volatile__ (
		TEXT16_CODE ( ".globl dump_regs\n\t"
			      "\ndump_regs:\n\t"
			      "pushl $_dump_regs\n\t"
			      "pushw %%cs\n\t"
			      "call prot_call\n\t"
			      "addr32 leal 4(%%esp), %%esp\n\t"
			      "ret\n\t" ) : : );

	printf ( "EAX=%08lx EBX=%08lx ECX=%08lx EDX=%08lx\n"
		 "ESI=%08lx EDI=%08lx EBP=%08lx ESP=%08lx\n"
		 "CS=%04x SS=%04x DS=%04x ES=%04x FS=%04x GS=%04x\n",
		 ix86->regs.eax, ix86->regs.ebx, ix86->regs.ecx,
		 ix86->regs.edx, ix86->regs.esi, ix86->regs.edi,
		 ix86->regs.ebp, ix86->regs.esp,
		 ix86->segs.cs, ix86->segs.ss, ix86->segs.ds,
		 ix86->segs.es, ix86->segs.fs, ix86->segs.gs );
}
