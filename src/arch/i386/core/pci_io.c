/*
** Support for NE2000 PCI clones added David Monro June 1997
** Generalised to other NICs by Ken Yap July 1997
**
** Most of this is taken from:
**
** /usr/src/linux/drivers/pci/pci.c
** /usr/src/linux/include/linux/pci.h
** /usr/src/linux/arch/i386/bios32.c
** /usr/src/linux/include/linux/bios32.h
** /usr/src/linux/drivers/net/ne.c
*/
#include "etherboot.h"
#include "init.h"
#include "pci.h"
#include "pci_io.h"
#ifdef KEEP_IT_REAL
#include "realmode.h"
#endif

#define DEBUG_PCI_IO

#undef DBG
#ifdef DEBUG_PCI_IO
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif

/* Macros for direct PCI access */
#define CONFIG_ADDRESS	0xcf8
#define CONFIG_DATA	0xcfc
#define CONFIG_CMD( pci, where ) \
	( 0x80000000 | (pci->busdevfn << 8) | (where & ~3) )

/* Signatures for PCI BIOS */
#define BIOS_SIG(a,b,c,d)	( ( a<<0 ) + ( b<<8 ) + ( c<<16 ) + ( d<<24 ) )
#define PRINT_BIOS_SIG(x)	( (x) & 0xff ), ( ( (x)>>8 ) & 0xff ), \
				( ( (x)>>16 ) & 0xff ),( ( (x)>>24 ) & 0xff )
#define BIOS32_SIGNATURE	BIOS_SIG ( '_', '3', '2', '_' )
#define PCI_SIGNATURE		BIOS_SIG ( 'P', 'C', 'I', ' ' )
#define PCI_SERVICE		BIOS_SIG ( '$', 'P', 'C', 'I' )

/* BIOS32 structure as found in PCI BIOS ROM */
struct bios32 {
	unsigned long signature;	/* _32_ */
	unsigned long entry;		/* 32 bit physical address */
	unsigned char revision;		/* Revision level, 0 */
	unsigned char length;		/* Length in paragraphs */
	unsigned char checksum;		/* Should byte sum to zero */
	unsigned char reserved[5];	/* Must be zero */
};

/* Values returned by BIOS32 service directory */
#define BIOS32_SERVICE_PRESENT		0x00
#define BIOS32_SERVICE_NOT_PRESENT	0x80
#define CF ( 1 << 0 )

/* PCI BIOS entry point */
#ifndef KEEP_IT_REAL
static unsigned long pcibios32_entry;
#endif
static int have_pcibios;

/* Macro for calling a 32-bit entry point with flat physical
 * addresses.  Use in a statement such as
 * __asm__ ( FLAT_FAR_CALL_ESI,
 *	     : <output registers>
 *	     : "S" ( entry_point ), <other input registers> );
 */
#define FLAT_FAR_CALL_ESI "call _virt_to_phys\n\t" \
			  "pushl %%cs\n\t" \
			  "call *%%esi\n\t" \
			  "cli\n\t" \
			  "cld\n\t" \
			  "call _phys_to_virt\n\t"

/*
 * Functions for accessing PCI configuration space directly with type
 * 1 accesses.
 *
 */

static inline int pcidirect_read_config_byte ( struct pci_device *pci,
					       unsigned int where,
					       uint8_t *value ) {
    outl ( CONFIG_CMD ( pci, where ), CONFIG_ADDRESS );
    *value = inb ( CONFIG_DATA + ( where & 3 ) );
    return 0;
}

static inline int pcidirect_read_config_word ( struct pci_device *pci,
					       unsigned int where,
					       uint16_t *value ) {
    outl ( CONFIG_CMD ( pci, where ), CONFIG_ADDRESS );
    *value = inw ( CONFIG_DATA + ( where & 2 ) );
    return 0;
}

static inline int pcidirect_read_config_dword ( struct pci_device *pci,
						unsigned int where,
						uint32_t *value ) {
    outl ( CONFIG_CMD ( pci, where ), CONFIG_ADDRESS );
    *value = inl ( CONFIG_DATA );
    return 0;
}

static inline int pcidirect_write_config_byte ( struct pci_device *pci,
						unsigned int where,
						uint8_t value ) {
    outl ( CONFIG_CMD ( pci, where ), CONFIG_ADDRESS );
    outb ( value, CONFIG_DATA + ( where & 3 ) );
    return 0;
}

static inline int pcidirect_write_config_word ( struct pci_device *pci,
						unsigned int where,
						uint16_t value ) {
    outl ( CONFIG_CMD ( pci, where ), CONFIG_ADDRESS );
    outw ( value, CONFIG_DATA + ( where & 2 ) );
    return 0;
}

static inline int pcidirect_write_config_dword ( struct pci_device *pci,
						 unsigned int where,
						 uint32_t value ) {
    outl ( CONFIG_CMD ( pci, where ), CONFIG_ADDRESS );
    outl ( value, CONFIG_DATA );
    return 0;
}

/*
 * Functions for accessing PCI configuration space directly via the
 * PCI BIOS.
 *
 * Under -DKEEP_IT_REAL, we use INT 1A, otherwise we use the BIOS32
 * interface.
 */

#ifdef KEEP_IT_REAL

static void find_pcibios16 ( void ) {
	uint16_t present;
	uint32_t signature;
	uint16_t flags;
	uint16_t revision;

	/* PCI BIOS installation check */
	REAL_EXEC ( rm_pcibios_check,
		    "int $0x1a\n\t"
		    "pushfw\n\t"
		    "popw %%cx\n\t",
		    4,
		    OUT_CONSTRAINTS ( "=a" ( present ), "=b" ( revision ),
				      "=c" ( flags ), "=d" ( signature ) ),
		    IN_CONSTRAINTS ( "a" ( ( PCIBIOS_PCI_FUNCTION_ID << 8 ) +
					   PCIBIOS_PCI_BIOS_PRESENT ) ),
		    CLOBBER ( "esi", "edi", "ebp" ) );

	if ( ( flags & CF ) ||
	     ( ( present >> 8 ) != 0 ) ||
	     ( signature != PCI_SIGNATURE ) ) {
		DBG ( "PCI BIOS installation check failed\n" );
		return;
	}

	/* We have a PCI BIOS */
	DBG ( "Found 16-bit PCI BIOS interface\n" );
	have_pcibios = 1;
	return;
}

INIT_FN ( INIT_PCIBIOS, find_pcibios16, NULL, NULL );

#define pcibios16_read_write( command, pci, where, value )		\
	( {								\
		uint32_t discard_b, discard_D;				\
		uint16_t ret;						\
									\
		REAL_EXEC ( 999, /* need a local label */		\
			    "int $0x1a\n\t"				\
			    "jc 1f\n\t"					\
			    "xorw %%ax, %%ax\n\t"			\
			    "\n1:\n\t",					\
			    5,						\
			    OUT_CONSTRAINTS ( "=a" ( ret ),		\
					      "=b" ( discard_b ),	\
					      "=c" ( value ),		\
					      "=D" ( discard_D ) ),	\
			    IN_CONSTRAINTS ( "a" ( command +		\
				    ( PCIBIOS_PCI_FUNCTION_ID << 8 ) ),	\
					     "b" ( pci->busdevfn ),	\
					     "c" ( value ),		\
			    		     "D" ( where ) ),		\
			    CLOBBER ( "edx", "esi", "ebp" ) );		\
									\
		( ret >> 8 );						\
	} )
#define pcibios_read_write pcibios16_read_write

#else /* KEEP_IT_REAL */

/*
 * Locate the BIOS32 service directory by scanning for a valid BIOS32
 * structure
 *
 */
static struct bios32 * find_bios32 ( void ) {
	uint32_t address;

	/*
	 * Follow the standard procedure for locating the BIOS32 Service
	 * directory by scanning the permissible address range from
	 * 0xe0000 through 0xfffff for a valid BIOS32 structure.
	 *
	 */
	for ( address = 0xe0000 ; address < 0xffff0 ; address += 16 ) {
		struct bios32 * candidate = phys_to_virt ( address );
		unsigned int length, i;
		unsigned char sum;

		if ( candidate->signature != BIOS32_SIGNATURE )
			continue;

		length = candidate->length * 16;
		if ( ! length )
			continue;

		for ( sum = 0, i = 0 ; i < length ; i++ )
			sum += ( ( char * ) candidate ) [i];
		if ( sum != 0 )
			continue;

		if ( candidate->revision != 0 ) {
			DBG ( "unsupported BIOS32 revision %d at %#x\n",
			      candidate->revision, address );
			continue;
		}

		DBG ( "BIOS32 Service Directory structure at %#x\n", address );

		return candidate;
	}

	return NULL;
}

/*
 * Look up a service in the BIOS32 service directory
 *
 */
static unsigned long find_bios32_service ( struct bios32 * bios32,
					   unsigned long service ) {
	uint8_t return_code;
	uint32_t address;
	uint32_t length;
	uint32_t entry;
	uint32_t discard;

	__asm__ ( FLAT_FAR_CALL_ESI
		  : "=a" ( return_code ), "=b" ( address ),
		    "=c" ( length ), "=d" ( entry ), "=S" ( discard )
		  : "a" ( service ), "b" ( 0 ), "S" ( bios32->entry )
		  : "edi", "ebp" );

	switch ( return_code ) {
	case BIOS32_SERVICE_PRESENT:
		DBG ( "BIOS32 service %c%c%c%c present at %#x\n",
		      PRINT_BIOS_SIG ( service ), ( address + entry ) );
		return ( address + entry );
	case BIOS32_SERVICE_NOT_PRESENT:
		DBG ( "BIOS32 service %c%c%c%c : not present\n",
		      PRINT_BIOS_SIG ( service ) );
		return 0;
	default: /* Shouldn't happen */
		DBG ( "BIOS32 returned %#x for service %c%c%c%c!\n",
		      return_code, PRINT_BIOS_SIG ( service ) );
		return 0;
	}
}

/*
 * Find the 32-bit PCI BIOS interface, if present.
 *
 */
static void find_pcibios32 ( void ) {
	struct bios32 *bios32;
	uint32_t signature;
	uint16_t present;
	uint32_t flags;
	uint16_t revision;
	uint32_t discard;

	/* Locate BIOS32 service directory */
	bios32 = find_bios32 ();
	if ( ! bios32 ) {
		DBG ( "No BIOS32\n" );
		return;
	}

	/* Locate PCI BIOS service */
	pcibios32_entry = find_bios32_service ( bios32, PCI_SERVICE );
	if ( ! pcibios32_entry ) {
		DBG ( "No PCI BIOS\n" );
		return;
	}
	
	/* PCI BIOS installation check */
	__asm__ ( FLAT_FAR_CALL_ESI
		  "pushfl\n\t"
		  "popl %%ecx\n\t"
		  : "=a" ( present ), "=b" ( revision ), "=c" ( flags ),
		    "=d" ( signature ), "=S" ( discard )
		  : "a" ( ( PCIBIOS_PCI_FUNCTION_ID << 8 )
			  + PCIBIOS_PCI_BIOS_PRESENT ),
		    "S" ( pcibios32_entry )
		  : "edi", "ebp" );

	if ( ( flags & CF ) ||
	     ( ( present >> 8 ) != 0 ) ||
	     ( signature != PCI_SIGNATURE ) ) {
		DBG ( "PCI BIOS installation check failed\n" );
		return;
	}

	/* We have a PCI BIOS */
	DBG ( "Found 32-bit PCI BIOS interface at %#x\n", pcibios32_entry );
	have_pcibios = 1;
	return;
}

INIT_FN ( INIT_PCIBIOS, find_pcibios32, NULL, NULL );

#define pcibios32_read_write( command, pci, where, value )		\
	( {								\
		uint32_t discard_b, discard_D, discard_S;		\
		uint16_t ret;						\
									\
		__asm__ ( FLAT_FAR_CALL_ESI				\
			  "jc 1f\n\t"					\
			  "xorl %%eax, %%eax\n\t"			\
			  "\n1:\n\t"					\
			  : "=a" ( ret ), "=b" ( discard_b ),		\
			    "=c" ( value ),				\
			    "=S" ( discard_S ), "=D" ( discard_D )	\
			  : "a" ( ( PCIBIOS_PCI_FUNCTION_ID << 8 )	\
				  + command ),			       	\
			    "b" ( pci->busdevfn ), "c" ( value ),	\
			    "D" ( where ), "S" ( pcibios32_entry )	\
			  : "edx", "ebp" );				\
									\
		( ret >> 8 );						\
	} )
#define pcibios_read_write pcibios32_read_write

#endif /* KEEP_IT_REAL */

static inline int pcibios_read_config_byte ( struct pci_device *pci,
					     unsigned int where,
					     uint8_t *value ) {
	return pcibios_read_write ( PCIBIOS_READ_CONFIG_BYTE,
				    pci, where, *value );
}

static inline int pcibios_read_config_word ( struct pci_device *pci,
					     unsigned int where,
					     uint16_t *value ) {
	return pcibios_read_write ( PCIBIOS_READ_CONFIG_WORD,
				    pci, where, *value );
}

static inline int pcibios_read_config_dword ( struct pci_device *pci,
					      unsigned int where,
					      uint32_t *value ) {
	return pcibios_read_write ( PCIBIOS_READ_CONFIG_DWORD,
				    pci, where, *value );
}

static inline int pcibios_write_config_byte ( struct pci_device *pci,
					      unsigned int where,
					      uint8_t value ) {
	return pcibios_read_write ( PCIBIOS_WRITE_CONFIG_BYTE,
				    pci, where, value );
}

static inline int pcibios_write_config_word ( struct pci_device *pci,
					      unsigned int where,
					      uint16_t value ) {
	return pcibios_read_write ( PCIBIOS_WRITE_CONFIG_WORD,
				    pci, where, value );
}

static inline int pcibios_write_config_dword ( struct pci_device *pci,
					       unsigned int where,
					       uint32_t value ) {
	return pcibios_read_write ( PCIBIOS_WRITE_CONFIG_DWORD,
				    pci, where, value );
}

/*
 * Functions for accessing PCI configuration space via the PCI BIOS if
 * present, otherwise directly via type 1 accesses.
 *
 */

int pci_read_config_byte ( struct pci_device *pci, unsigned int where,
			   uint8_t *value ) {
	return have_pcibios ?
		pcibios_read_config_byte ( pci, where, value ) :
		pcidirect_read_config_byte ( pci, where, value );
}
		
int pci_read_config_word ( struct pci_device *pci, unsigned int where,
			   uint16_t *value ) {
	return have_pcibios ?
		pcibios_read_config_word ( pci, where, value ) :
		pcidirect_read_config_word ( pci, where, value );
}
		
int pci_read_config_dword ( struct pci_device *pci, unsigned int where,
			    uint32_t *value ) {
	return have_pcibios ?
		pcibios_read_config_dword ( pci, where, value ) :
		pcidirect_read_config_dword ( pci, where, value );
}
		
int pci_write_config_byte ( struct pci_device *pci, unsigned int where,
			    uint8_t value ) {
	return have_pcibios ?
		pcibios_write_config_byte ( pci, where, value ) :
		pcidirect_write_config_byte ( pci, where, value );
}
		
int pci_write_config_word ( struct pci_device *pci, unsigned int where,
			    uint16_t value ) {
	return have_pcibios ?
		pcibios_write_config_word ( pci, where, value ) :
		pcidirect_write_config_word ( pci, where, value );
}
		
int pci_write_config_dword ( struct pci_device *pci, unsigned int where,
			     uint32_t value ) {
	return have_pcibios ?
		pcibios_write_config_dword ( pci, where, value ) :
		pcidirect_write_config_dword ( pci, where, value );
}
		
unsigned long pci_bus_base ( struct pci_device *pci __unused ) {
	/* architecturally this must be 0 */
	return 0;
}
