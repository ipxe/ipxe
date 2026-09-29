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

/*
 * Install a single-StreamID bypass entry for the AWS ENA NIC
 *
 * The firmware enables the SMMU but never enumerates the ENA, so it
 * has no stream table entry and its first DMA faults with
 * C_BAD_STREAMID.  Install a bypass STE for exactly the ENA's
 * StreamID, using a one-entry level-2 table so that no other StreamID
 * is affected.  Call once, after ena_membases() and before any DMA.
 *
 * Hardcoded for ENA at 0002:04:00.0 (StreamID 0x400, SMMU
 * 0x807800000).  Not for upstream.
 */

#include <string.h>
#include <ipxe/malloc.h>

/** SMMU instance and StreamID serving the ENA (0002:04:00.0) */
#define SMMU_ENA_BASE 0x807800000UL
#define SMMU_ENA_SID  0x400

/** STRTAB_BASE_CFG fields */
#define SMMU_STRTAB_CFG_FMT( x )	( ( (x) >> 16 ) & 0x3 )
#define SMMU_STRTAB_CFG_SPLIT( x )	( ( (x) >> 6 ) & 0x1f )
#define SMMU_STRTAB_CFG_LOG2SIZE( x )	( (x) & 0x3f )
#define SMMU_STRTAB_FMT_2LVL 1

/** Stream table entry (STE) qword 0 */
#define SMMU_STE_0_V		0x0000000000000001ULL
#define SMMU_STE_0_CFG_BYPASS	0x0000000000000008ULL /* CFG=0b100 << 1 */

/** Stream table entry (STE) qword 1: SHCFG=Use-incoming (bits 45:44) */
#define SMMU_STE_1_SHCFG_INCOMING 0x0000100000000000ULL

/** Level-1 stream table descriptor: span for a single-entry L2 table */
#define SMMU_L1_DESC_SPAN_ONE 1

/**
 * Install ENA bypass stream table entry
 *
 * @ret rc		Return status code
 */
int smmu_ena_bypass ( void ) {
	void *regs;
	uint32_t cr0;
	uint32_t cfg;
	unsigned int split;
	unsigned int l1idx;
	unsigned int l2idx;
	uint64_t strtab;
	volatile uint64_t *l1desc;
	uint64_t desc;
	uint64_t *ste;
	uint64_t l2phys;

	/* Map SMMU register page */
	regs = ioremap ( SMMU_ENA_BASE, 0x1000 );
	if ( ! regs ) {
		printf ( "SMMU %#lx: could not map registers\n",
			 SMMU_ENA_BASE );
		return -1;
	}

	/* Nothing to do unless the SMMU is enabled */
	cr0 = readl ( regs + SMMU_CR0 );
	if ( ! ( cr0 & 1 ) ) {
		printf ( "SMMU %#lx: not enabled (CR0 %08x); no bypass "
			 "needed\n", SMMU_ENA_BASE, cr0 );
		iounmap ( regs );
		return 0;
	}

	/* Require a two-level stream table (which is what we observe) */
	cfg = readl ( regs + SMMU_STRTAB_BASE_CFG );
	if ( SMMU_STRTAB_CFG_FMT ( cfg ) != SMMU_STRTAB_FMT_2LVL ) {
		printf ( "SMMU %#lx: not a two-level stream table (CFG "
			 "%08x)\n", SMMU_ENA_BASE, cfg );
		iounmap ( regs );
		return -1;
	}
	split = SMMU_STRTAB_CFG_SPLIT ( cfg );
	strtab = ( ( ( uint64_t ) readl ( regs + SMMU_STRTAB_BASE + 4 ) << 32 )
		   | readl ( regs + SMMU_STRTAB_BASE ) );
	strtab &= SMMU_ADDR_MASK;

	/* Locate the level-1 descriptor for our StreamID */
	l1idx = ( SMMU_ENA_SID >> split );
	l2idx = ( SMMU_ENA_SID & ( ( 1U << split ) - 1 ) );
	if ( l2idx != 0 ) {
		/* A one-entry L2 table only covers L2 index 0 */
		printf ( "SMMU %#lx: SID %#x has L2 index %#x != 0\n",
			 SMMU_ENA_BASE, SMMU_ENA_SID, l2idx );
		iounmap ( regs );
		return -1;
	}

	/* Map and inspect the level-1 descriptor */
	l1desc = ioremap ( ( strtab + ( l1idx * sizeof ( *l1desc ) ) ),
			   sizeof ( *l1desc ) );
	if ( ! l1desc ) {
		printf ( "SMMU %#lx: could not map L1 descriptor\n",
			 SMMU_ENA_BASE );
		iounmap ( regs );
		return -1;
	}
	desc = *l1desc;
	if ( ( desc & 0x1f ) != 0 ) {
		/* Firmware has already populated this L1 slot: do not
		 * touch it, since it may cover a real neighbour.
		 */
		printf ( "SMMU %#lx: L1[%#x] already has span %d (%016llx); "
			 "refusing to overwrite\n", SMMU_ENA_BASE, l1idx,
			 ( int ) ( desc & 0x1f ), ( unsigned long long ) desc );
		iounmap ( l1desc );
		iounmap ( regs );
		return -1;
	}

	/* Allocate and build a one-entry level-2 table (one STE) */
	ste = malloc_phys ( 64, 64 );
	if ( ! ste ) {
		iounmap ( l1desc );
		iounmap ( regs );
		return -1;
	}
	memset ( ste, 0, 64 );
	ste[0] = ( SMMU_STE_0_V | SMMU_STE_0_CFG_BYPASS );
	ste[1] = SMMU_STE_1_SHCFG_INCOMING;
	l2phys = virt_to_phys ( ste );

	/* Ensure the STE is visible before the L1 descriptor points at
	 * it (the SMMU table walker is coherent: IDR0.COHACC=1).
	 */
	wmb();

	/* Point the level-1 descriptor at the new L2 table */
	desc = ( ( l2phys & SMMU_ADDR_MASK ) | SMMU_L1_DESC_SPAN_ONE );
	writeq ( desc, l1desc );
	wmb();

	printf ( "SMMU %#lx: installed bypass for SID %#x: L1[%#x]=%016llx "
		 "STE=%016llx/%016llx @ %#llx\n", SMMU_ENA_BASE, SMMU_ENA_SID,
		 l1idx, ( unsigned long long ) desc,
		 ( unsigned long long ) ste[0], ( unsigned long long ) ste[1],
		 ( unsigned long long ) l2phys );

	iounmap ( l1desc );
	iounmap ( regs );
	return 0;
}
