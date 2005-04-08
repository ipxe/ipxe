#ifndef REGISTERS_H
#define REGISTERS_H

#include "stdint.h"
#include "compiler.h"

/* Basic 16-bit and 32-bit register types */
typedef union {
	struct {
		union {
			uint8_t l;
			uint8_t byte;
		};
		uint8_t h;
	} PACKED;
	uint16_t word;
} PACKED reg16_t;

typedef union {
	reg16_t;
	uint32_t dword;
} PACKED reg32_t;

/* As created by pushal / read by popal */
struct i386_regs {
	union {
		uint16_t di;
		uint32_t edi;
	};
	union {
		uint16_t si;
		uint32_t esi;
	};
	union {
		uint16_t bp;
		uint32_t ebp;
	};
	union {
		uint16_t sp;
		uint32_t esp;
	};
	union {
		struct {
			uint8_t bl;
			uint8_t bh;
		} PACKED;
		uint16_t bx;
		uint32_t ebx;
	};
	union {
		struct {
			uint8_t dl;
			uint8_t dh;
		} PACKED;
		uint16_t dx;
		uint32_t edx;
	};
	union {
		struct {
			uint8_t cl;
			uint8_t ch;
		} PACKED;
		uint16_t cx;
		uint32_t ecx;
	};
	union {
		struct {
			uint8_t al;
			uint8_t ah;
		} PACKED;
		uint16_t ax;
		uint32_t eax;
	};
} PACKED;

/* Our pushal/popal equivalent for segment registers */
struct i386_seg_regs {
	uint16_t cs;
	uint16_t ss;
	uint16_t ds;
	uint16_t es;
	uint16_t fs;
	uint16_t gs;
} PACKED;

/* All i386 registers, as passed in by prot_call or kir_call */
struct i386_all_regs {
	struct i386_seg_regs;
	struct i386_regs;
	uint32_t i386_flags;
} PACKED;

#endif /* REGISTERS_H */
