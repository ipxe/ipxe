#ifndef _IPXE_RISCV_IO_H
#define _IPXE_RISCV_IO_H

/** @file
 *
 * iPXE I/O API for RISC-V
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#ifdef IOAPI_RISCV
#define IOAPI_PREFIX_riscv
#else
#define IOAPI_PREFIX_riscv __riscv_
#endif

#include <ipxe/dummy_pio.h>

/*
 * Memory space mappings
 *
 */

/*
 * Physical<->Bus address mappings
 *
 */

static inline __always_inline unsigned long
IOAPI_INLINE ( riscv, phys_to_bus ) ( unsigned long phys_addr ) {
	return phys_addr;
}

static inline __always_inline unsigned long
IOAPI_INLINE ( riscv, bus_to_phys ) ( unsigned long bus_addr ) {
	return bus_addr;
}

/*
 * MMIO reads and writes
 *
 */

/* Single-register read */
#define RISCV_READX( _suffix, _type, _insn_suffix )			\
static inline __always_inline _type					\
IOAPI_INLINE ( riscv, read ## _suffix ) ( volatile _type *io_addr ) {	\
	unsigned long data;						\
	__asm__ __volatile__ ( "l" _insn_suffix " %0, %1"		\
			       : "=r" ( data ) : "m" ( *io_addr ) );	\
	return data;							\
}

/* Single-register write */
#define RISCV_WRITEX( _suffix, _type, _insn_suffix)			\
static inline __always_inline void					\
IOAPI_INLINE ( riscv, write ## _suffix ) ( _type data,			\
					   volatile _type *io_addr ) {	\
	__asm__ __volatile__ ( "s" _insn_suffix " %0, %1"		\
			       : : "r" ( data ), "m" ( *io_addr ) );	\
}

/* Double-register hopefully-fused read */
#define RISCV_READX_FUSED( _suffix, _type, _insn_suffix )		\
static inline __always_inline _type					\
IOAPI_INLINE ( riscv, read ## _suffix ) ( volatile _type *io_addr ) {	\
	union {								\
		unsigned long half[2];					\
	        _type data;						\
	} u;								\
	__asm__ __volatile__ ( "l" _insn_suffix " %0, 0(%2)\n\t"	\
			       "l" _insn_suffix " %1, %3(%2)\n\t"	\
			       : "=&r" ( u.half[0] ),			\
				 "=&r" ( u.half[1] )			\
			       : "r" ( io_addr ),			\
				 "i" ( sizeof ( u.half[0] ) ) );	\
	return u.data;							\
}

/* Double-register hopefully-fused write */
#define RISCV_WRITEX_FUSED( _suffix, _type, _insn_suffix )		\
static inline __always_inline void					\
IOAPI_INLINE ( riscv, write ## _suffix ) ( _type data,			\
					   volatile _type *io_addr ) {	\
	union {								\
		unsigned long half[2];					\
		_type data;						\
	} u = { .data = data };						\
	__asm__ __volatile__ ( "s" _insn_suffix " %0, 0(%2)\n\t"	\
			       "s" _insn_suffix " %1, %3(%2)\n\t" :	\
			       : "r" ( u.half[0] ),			\
				 "r" ( u.half[1] ),			\
				 "r" ( io_addr ),			\
				 "i" ( sizeof ( u.half[0] ) ) );	\
}

RISCV_READX ( b, uint8_t, "bu" );
RISCV_WRITEX ( b, uint8_t, "b" );

RISCV_READX ( w, uint16_t, "hu" );
RISCV_WRITEX ( w, uint16_t, "h" );

#if __riscv_xlen > 32
  RISCV_READX ( l, uint32_t, "wu" );
  RISCV_WRITEX ( l, uint32_t, "w" );
#else
  RISCV_READX ( l, uint32_t, "w" );
  RISCV_WRITEX ( l, uint32_t, "w" );
#endif

#if __riscv_xlen >= 64
  #if __riscv_xlen > 64
    RISCV_READX ( q, uint64_t, "du" );
    RISCV_WRITEX ( q, uint64_t, "d" );
  #else
    RISCV_READX ( q, uint64_t, "d" );
    RISCV_WRITEX ( q, uint64_t, "d" );
  #endif
#else
  RISCV_READX_FUSED ( q, uint64_t, "w" );
  RISCV_WRITEX_FUSED ( q, uint64_t, "w" );
#endif

/*
 * Memory barrier
 *
 */
static inline __always_inline void
IOAPI_INLINE ( riscv, mb ) ( void ) {
	__asm__ __volatile__ ( "fence" : : : "memory" );
}

/* Dummy PIO */
DUMMY_PIO ( riscv );

#endif /* _IPXE_RISCV_IO_H */
