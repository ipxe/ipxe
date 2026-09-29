/*
 * Temporary SMMUv3 state dump for debugging (not for upstream)
 *
 * Call smmu_dump() once before ringing the AQ doorbell, and
 * smmu_poll() on each iteration of the ena_admin() wait loop.
 */

#include <stdint.h>
#include <stdio.h>
#include <ipxe/io.h>

/** SMMUv3 base addresses (from Linux "ACPI: IORT: SMMU-v3[...]" lines) */
static const unsigned long smmu_bases[] = {
	0x802800000UL, 0x807800000UL, 0x8d2800000UL, 0x8d7800000UL,
};
#define SMMU_COUNT ( sizeof ( smmu_bases ) / sizeof ( smmu_bases[0] ) )

/** Size of SMMUv3 register space (pages 0 and 1) */
#define SMMU_REGS_LEN 0x20000

#define SMMU_IDR0		0x00000
#define SMMU_IDR1		0x00004
#define SMMU_CR0		0x00020
#define SMMU_CR1		0x00028
#define SMMU_CR2		0x0002c
#define SMMU_GBPA		0x00044
#define SMMU_GERROR		0x00060
#define SMMU_GERRORN		0x00064
#define SMMU_STRTAB_BASE	0x00080
#define SMMU_STRTAB_BASE_CFG	0x00088
#define SMMU_EVENTQ_BASE	0x000a0
#define SMMU_EVENTQ_PROD	0x100a8
#define SMMU_EVENTQ_CONS	0x100ac

/** Address field mask (bits 51:6) */
#define SMMU_ADDR_MASK 0x000fffffffffffc0ULL

/** Maximum stream table size to walk (log2 entries) */
#define SMMU_MAX_LOG2 20

static void *smmu_regs[SMMU_COUNT];
static uint32_t smmu_last_gerror[SMMU_COUNT];
static uint32_t smmu_last_prod[SMMU_COUNT];

static uint32_t smmu_readl ( unsigned int i, unsigned int reg ) {
	return readl ( smmu_regs[i] + reg );
}

static uint64_t smmu_readq ( unsigned int i, unsigned int reg ) {
	return ( ( ( uint64_t ) smmu_readl ( i, ( reg + 4 ) ) << 32 ) |
		 smmu_readl ( i, reg ) );
}

/** Print a stream table entry, if valid */
static void smmu_dump_ste ( unsigned int sid, uint64_t ste_phys ) {
	volatile uint64_t *ste = phys_to_virt ( ste_phys );

	if ( ! ( ste[0] & 1 ) )
		return;
	printf ( "  SID %#06x cfg %d: %016llx %016llx %016llx %016llx\n",
		 sid, ( ( unsigned int ) ( ste[0] >> 1 ) & 7 ),
		 ( unsigned long long ) ste[0], ( unsigned long long ) ste[1],
		 ( unsigned long long ) ste[2], ( unsigned long long ) ste[3] );
}

/** Dump all valid stream table entries */
static void smmu_dump_strtab ( unsigned int i ) {
	uint64_t base = ( smmu_readq ( i, SMMU_STRTAB_BASE ) & SMMU_ADDR_MASK );
	uint32_t cfg = smmu_readl ( i, SMMU_STRTAB_BASE_CFG );
	unsigned int log2size = ( cfg & 0x3f );
	unsigned int split = ( ( cfg >> 6 ) & 0x1f );
	unsigned int fmt = ( ( cfg >> 16 ) & 0x3 );
	volatile uint64_t *l1;
	uint64_t desc;
	unsigned int span;
	unsigned int l1idx;
	unsigned int l2idx;

	if ( log2size > SMMU_MAX_LOG2 )
		log2size = SMMU_MAX_LOG2;
	if ( ! base ) {
		printf ( "  no stream table\n" );
		return;
	}

	if ( fmt == 0 ) {
		/* Linear stream table */
		for ( l2idx = 0 ; l2idx < ( 1U << log2size ) ; l2idx++ )
			smmu_dump_ste ( l2idx, ( base + ( l2idx * 64 ) ) );
		return;
	}

	/* Two-level stream table */
	l1 = phys_to_virt ( base );
	for ( l1idx = 0 ; l1idx < ( 1U << ( log2size - split ) ) ; l1idx++ ) {
		desc = l1[l1idx];
		span = ( desc & 0x1f );
		if ( ! span )
			continue;
		printf ( "  L1[%#x] span %d L2 %#llx\n", l1idx, span,
			 ( unsigned long long ) ( desc & SMMU_ADDR_MASK ) );
		for ( l2idx = 0 ; l2idx < ( 1U << ( span - 1 ) ) ; l2idx++ ) {
			smmu_dump_ste ( ( ( l1idx << split ) | l2idx ),
					( ( desc & SMMU_ADDR_MASK ) +
					  ( l2idx * 64 ) ) );
		}
	}
}

/** Dump new event queue entries */
static void smmu_dump_events ( unsigned int i ) {
	uint64_t qbase = smmu_readq ( i, SMMU_EVENTQ_BASE );
	unsigned int log2size = ( qbase & 0x1f );
	uint32_t prod = smmu_readl ( i, SMMU_EVENTQ_PROD );
	uint32_t cons = smmu_readl ( i, SMMU_EVENTQ_CONS );
	uint32_t mask = ( ( 1U << log2size ) - 1 );
	uint32_t wrap = ( ( 2U << log2size ) - 1 );
	volatile uint64_t *evt;

	qbase &= 0x000fffffffffffe0ULL;
	if ( ! qbase )
		return;
	for ( ; ( cons & wrap ) != ( prod & wrap ) ; cons++ ) {
		evt = phys_to_virt ( qbase + ( ( cons & mask ) * 32 ) );
		printf ( "  EVT id %#02x SID %#06x: %016llx %016llx %016llx "
			 "%016llx\n", ( unsigned int ) ( evt[0] & 0xff ),
			 ( unsigned int ) ( evt[0] >> 32 ),
			 ( unsigned long long ) evt[0],
			 ( unsigned long long ) evt[1],
			 ( unsigned long long ) evt[2],
			 ( unsigned long long ) evt[3] );
	}
}

/** Dump state of all SMMUs */
void smmu_dump ( void ) {
	unsigned int i;

	for ( i = 0 ; i < SMMU_COUNT ; i++ ) {
		smmu_regs[i] = ioremap ( smmu_bases[i], SMMU_REGS_LEN );
		if ( ! smmu_regs[i] ) {
			printf ( "SMMU %#lx: could not map\n", smmu_bases[i] );
			continue;
		}
		printf ( "SMMU %#lx: IDR0 %08x IDR1 %08x CR0 %08x CR1 %08x "
			 "CR2 %08x GBPA %08x\n", smmu_bases[i],
			 smmu_readl ( i, SMMU_IDR0 ),
			 smmu_readl ( i, SMMU_IDR1 ),
			 smmu_readl ( i, SMMU_CR0 ),
			 smmu_readl ( i, SMMU_CR1 ),
			 smmu_readl ( i, SMMU_CR2 ),
			 smmu_readl ( i, SMMU_GBPA ) );
		printf ( "SMMU %#lx: GERROR %08x/%08x STRTAB %016llx/%08x "
			 "EVENTQ %016llx %08x/%08x\n", smmu_bases[i],
			 smmu_readl ( i, SMMU_GERROR ),
			 smmu_readl ( i, SMMU_GERRORN ),
			 ( unsigned long long )
			 smmu_readq ( i, SMMU_STRTAB_BASE ),
			 smmu_readl ( i, SMMU_STRTAB_BASE_CFG ),
			 ( unsigned long long )
			 smmu_readq ( i, SMMU_EVENTQ_BASE ),
			 smmu_readl ( i, SMMU_EVENTQ_PROD ),
			 smmu_readl ( i, SMMU_EVENTQ_CONS ) );
		smmu_last_gerror[i] = smmu_readl ( i, SMMU_GERROR );
		smmu_last_prod[i] = smmu_readl ( i, SMMU_EVENTQ_PROD );
		if ( smmu_readl ( i, SMMU_CR0 ) & 1 ) {
			smmu_dump_strtab ( i );
			smmu_dump_events ( i );
		}
	}
}

/** Report any change in SMMU error or event state */
void smmu_poll ( void ) {
	uint32_t gerror;
	uint32_t prod;
	unsigned int i;

	for ( i = 0 ; i < SMMU_COUNT ; i++ ) {
		if ( ! smmu_regs[i] )
			continue;
		gerror = smmu_readl ( i, SMMU_GERROR );
		prod = smmu_readl ( i, SMMU_EVENTQ_PROD );
		if ( ( gerror == smmu_last_gerror[i] ) &&
		     ( prod == smmu_last_prod[i] ) )
			continue;
		printf ( "SMMU %#lx: GERROR %08x EVENTQ_PROD %08x\n",
			 smmu_bases[i], gerror, prod );
		smmu_last_gerror[i] = gerror;
		smmu_last_prod[i] = prod;
		smmu_dump_events ( i );
	}
}
