#ifndef	ETHERBOOT_OSDEP_H
#define ETHERBOOT_OSDEP_H

#define __unused __attribute__((unused))
#define __aligned __attribute__((aligned(16)))
#define PACKED __attribute__((packed))

/* Optimization barrier */
/* The "volatile" is due to gcc bugs */
#define barrier() __asm__ __volatile__("": : :"memory")

#include "stdint.h"
#include "limits.h"
#include "string.h"
#include "io.h"
#include "endian.h"
#include "byteswap.h"
#include "setjmp.h"
#include "latch.h"
#include "callbacks.h"
#include "hooks.h"

/* within 1MB of 4GB is too close. 
 * MAX_ADDR is the maximum address we can easily do DMA to.
 */
#define MAX_ADDR (0xfff00000UL)

typedef	unsigned long Address;

/* ANSI prototyping macro */
#ifdef	__STDC__
#define	P(x)	x
#else
#define	P(x)	()
#endif

#endif

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
