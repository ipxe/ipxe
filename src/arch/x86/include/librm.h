#ifndef LIBRM_H
#define LIBRM_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/* Segment selectors as used in our protected-mode GDTs.
 *
 * Don't change these unless you really know what you're doing.
 */
#define VIRTUAL_CS 0x08
#define VIRTUAL_DS 0x10
#define PHYSICAL_CS 0x18
#define PHYSICAL_DS 0x20
#define REAL_CS 0x28
#define REAL_DS 0x30
#define P2R_DS 0x38
#define LONG_CS 0x40

/* Calculate symbol address within VIRTUAL_CS or VIRTUAL_DS
 *
 * In a 64-bit build, we set the bases of VIRTUAL_CS and VIRTUAL_DS
 * such that truncating a .textdata symbol value to 32 bits gives a
 * valid 32-bit virtual address.
 *
 * The C code is compiled with -mcmodel=kernel and so we must place
 * all .textdata symbols within the negative 2GB of the 64-bit address
 * space.  Consequently, all .textdata symbols will have the MSB set
 * after truncation to 32 bits.  This means that a straightforward
 * R_X86_64_32 relocation record for the symbol will fail, since the
 * truncated symbol value will not correctly zero-extend to the
 * original 64-bit value.
 *
 * Using an R_X86_64_32S relocation record would work, but there is no
 * (sensible) way to generate these relocation records within 32-bit
 * or 16-bit code.
 *
 * The simplest solution is to generate an R_X86_64_32 relocation
 * record with an addend of (-0xffffffff00000000).  Since all
 * .textdata symbols are within the negative 2GB of the 64-bit address
 * space, this addend acts to effectively truncate the symbol to 32
 * bits, thereby matching the semantics of the R_X86_64_32 relocation
 * records generated for 32-bit and 16-bit code.
 *
 * In a 32-bit build, this problem does not exist, and we can just use
 * the .textdata symbol values directly.
 */
#ifdef __x86_64__
#define VIRTUAL(address) ( (address) - 0xffffffff00000000 )
#else
#define VIRTUAL(address) (address)
#endif

#ifdef ASSEMBLY

/**
 * Call C function from real-mode code
 *
 * @v function		C function
 */
.macro virtcall function
	pushl	$VIRTUAL(\function)
	call	virt_call
.endm

#else /* ASSEMBLY */

/**
 * Call C function from real-mode code
 *
 * @v function		C function
 */
#define VIRT_CALL( function )						\
	"pushl $( " _S2 ( VIRTUAL ( function ) ) " )\n\t"		\
	"call virt_call\n\t"

/******************************************************************************
 *
 * Access to variables in .data16 and .text16
 *
 */

extern char * const data16;
extern char * const text16;

#define __data16( variable )						\
	__attribute__ (( section ( ".data16" ) ))			\
	_data16_ ## variable __asm__ ( #variable )

#define __data16_array( variable, array )				\
	__attribute__ (( section ( ".data16" ) ))			\
	_data16_ ## variable array __asm__ ( #variable )

#define __bss16( variable )						\
	__attribute__ (( section ( ".bss16" ) ))			\
	_data16_ ## variable __asm__ ( #variable )

#define __bss16_array( variable, array )				\
	__attribute__ (( section ( ".bss16" ) ))			\
	_data16_ ## variable array __asm__ ( #variable )

#define __text16( variable )						\
	__attribute__ (( section ( ".text16.data" ) ))			\
	_text16_ ## variable __asm__ ( #variable )

#define __text16_array( variable, array )				\
	__attribute__ (( section ( ".text16.data" ) ))			\
	_text16_ ## variable array __asm__ ( #variable )

#define __use_data16( variable )					\
	( * ( ( typeof ( _data16_ ## variable ) * )			\
	      & ( data16 [ ( size_t ) & ( _data16_ ## variable ) ] ) ) )

#define __use_text16( variable )					\
	( * ( ( typeof ( _text16_ ## variable ) * )			\
	      & ( text16 [ ( size_t ) & ( _text16_ ## variable ) ] ) ) )

#define __from_data16( pointer )					\
	( ( unsigned int )						\
	  ( ( ( void * ) (pointer) ) - ( ( void * ) data16 ) ) )

#define __from_text16( pointer )					\
	( ( unsigned int )						\
	  ( ( ( void * ) (pointer) ) - ( ( void * ) text16 ) ) )

/* Variables in librm.S, present in the normal data segment */
extern uint16_t rm_sp;
extern uint16_t rm_ss;
extern const uint16_t __text16 ( rm_cs );
#define rm_cs __use_text16 ( rm_cs )
extern const uint16_t __text16 ( rm_ds );
#define rm_ds __use_text16 ( rm_ds )

extern uint16_t copy_to_rm_stack ( const void *data, size_t size );
extern void remove_from_rm_stack ( void *data, size_t size );

/* CODE_DEFAULT: restore default .code32/.code64 directive */
#ifdef __x86_64__
#define CODE_DEFAULT ".code64"
#define STACK_DEFAULT "q"
#else
#define CODE_DEFAULT ".code32"
#define STACK_DEFAULT "l"
#endif

/* LINE_SYMBOL: declare a symbol for the current source code line */
#define LINE_SYMBOL _S2 ( OBJECT ) "__line_" _S2 ( __LINE__ ) "__%=:"

/* TEXT16_CODE: declare a fragment of code that resides in .text16 */
#define TEXT16_CODE( asm_code_str )			\
	".section \".text16\", \"ax\", @progbits\n\t"	\
	"\n" LINE_SYMBOL "\n\t"				\
	".code16\n\t"					\
	asm_code_str "\n\t"				\
	CODE_DEFAULT "\n\t"				\
	".previous\n\t"

/* REAL_CODE: declare a fragment of code that executes in real mode */
#define REAL_CODE( asm_code_str )			\
	"push" STACK_DEFAULT " $1f\n\t"			\
	"call real_call\n\t"				\
	TEXT16_CODE ( "\n1:\n\t"			\
		      asm_code_str			\
		      "\n\t"				\
		      "ret\n\t" )

/* PHYS_CODE: declare a fragment of code that executes in flat physical mode */
#define PHYS_CODE( asm_code_str )			\
	"push" STACK_DEFAULT " $1f\n\t"			\
	"call phys_call\n\t"				\
	".section \".text.phys\", \"ax\", @progbits\n\t"\
	"\n" LINE_SYMBOL "\n\t"				\
	".code32\n\t"					\
	"\n1:\n\t"					\
	asm_code_str					\
	"\n\t"						\
	"ret\n\t"					\
	CODE_DEFAULT "\n\t"				\
	".previous\n\t"

/** Number of interrupts */
#define NUM_INT 256

/** A 32-bit interrupt descriptor table register */
struct idtr32 {
	/** Limit */
	uint16_t limit;
	/** Base */
	uint32_t base;
} __attribute__ (( packed ));

/** A 64-bit interrupt descriptor table register */
struct idtr64 {
	/** Limit */
	uint16_t limit;
	/** Base */
	uint64_t base;
} __attribute__ (( packed ));

/** A 32-bit interrupt descriptor table entry */
struct interrupt32_descriptor {
	/** Low 16 bits of address */
	uint16_t low;
	/** Code segment */
	uint16_t segment;
	/** Unused */
	uint8_t unused;
	/** Type and attributes */
	uint8_t attr;
	/** High 16 bits of address */
	uint16_t high;
} __attribute__ (( packed ));

/** A 64-bit interrupt descriptor table entry */
struct interrupt64_descriptor {
	/** Low 16 bits of address */
	uint16_t low;
	/** Code segment */
	uint16_t segment;
	/** Unused */
	uint8_t unused;
	/** Type and attributes */
	uint8_t attr;
	/** Middle 16 bits of address */
	uint16_t mid;
	/** High 32 bits of address */
	uint32_t high;
	/** Reserved */
	uint32_t reserved;
} __attribute__ (( packed ));

/** Interrupt descriptor is present */
#define IDTE_PRESENT 0x80

/** Interrupt descriptor 32-bit interrupt gate type */
#define IDTE_TYPE_IRQ32 0x0e

/** Interrupt descriptor 64-bit interrupt gate type */
#define IDTE_TYPE_IRQ64 0x0e

/** An interrupt vector
 *
 * Each interrupt vector comprises an eight-byte fragment of code:
 *
 *   50			pushl %eax (or pushq %rax in long mode)
 *   b0 xx		movb $INT, %al
 *   e9 xx xx xx xx	jmp interrupt_wrapper
 */
struct interrupt_vector {
	/** "push" instruction */
	uint8_t push;
	/** "movb" instruction */
	uint8_t movb;
	/** Interrupt number */
	uint8_t intr;
	/** "jmp" instruction */
	uint8_t jmp;
	/** Interrupt wrapper address offset */
	uint32_t offset;
	/** Next instruction after jump */
	uint8_t next[0];
} __attribute__ (( packed ));

/** "push %eax" instruction */
#define PUSH_INSN 0x50

/** "movb" instruction */
#define MOVB_INSN 0xb0

/** "jmp" instruction */
#define JMP_INSN 0xe9

/** 32-bit interrupt wrapper stack frame */
struct interrupt_frame32 {
	uint32_t esp;
	uint32_t ss;
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;
	uint32_t ebp;
	uint32_t edi;
	uint32_t esi;
	uint32_t edx;
	uint32_t ecx;
	uint32_t ebx;
	uint32_t eax;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
} __attribute__ (( packed ));

/** 64-bit interrupt wrapper stack frame */
struct interrupt_frame64 {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__ (( packed ));

extern void set_interrupt_vector ( unsigned int intr, void *vector );

/** A page table */
struct page_table {
	/** Page address and flags */
	uint64_t page[512];
};

/** Page flags */
enum page_flags {
	/** Page is present */
	PAGE_P = 0x01,
	/** Page is writable */
	PAGE_RW = 0x02,
	/** Page is accessible by user code */
	PAGE_US = 0x04,
	/** Page-level write-through */
	PAGE_PWT = 0x08,
	/** Page-level cache disable */
	PAGE_PCD = 0x10,
	/** Page is a large page */
	PAGE_PS = 0x80,
	/** Page is the last page in an allocation
	 *
	 * This bit is ignored by the hardware.  We use it to track
	 * the size of allocations made by ioremap().
	 */
	PAGE_LAST = 0x800,
};

/** The I/O space page table */
extern struct page_table io_pages;

/** I/O page size
 *
 * We choose to use 2MB pages for I/O space, to minimise the number of
 * page table entries required.
 */
#define IO_PAGE_SIZE 0x200000UL

/** I/O page base address
 *
 * We choose to place I/O space immediately above the identity-mapped
 * 32-bit address space.
 */
#define IO_BASE ( ( void * ) 0x100000000ULL )

/** Startup IPI real-mode handler */
extern char __text16_array ( sipi, [] );
#define sipi __use_text16 ( sipi )

/** Length of startup IPI real-mode handler */
extern size_t ABS_SYMBOL ( sipi_len );
#define sipi_len ABS_VALUE ( sipi_len )

/** Startup IPI real-mode handler copy of real-mode data segment */
extern uint16_t __text16 ( sipi_ds );
#define sipi_ds __use_text16 ( sipi_ds )

/** Startup IPI protected-mode handler (physical address) */
extern uint32_t sipi_handler;

/** Startup IPI register state */
extern struct i386_regs sipi_regs;

extern void setup_sipi ( unsigned int vector, uint32_t handler,
			 struct i386_regs *regs );

#endif /* ASSEMBLY */

#endif /* LIBRM_H */
