FILE_LICENCE ( GPL2_OR_LATER );

#include <stdio.h>
#include <realmode.h>
#include <bios.h>
#include <gpxe/io.h>
#include <gpxe/timer.h>

#define K_RDWR		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS	0x64		/* keyboard status */
#define K_CMD		0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL	0x01		/* output buffer full */
#define K_IBUF_FUL	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KC_CMD_NULL	0xff		/* null command ("pulse nothing") */
#define KB_SET_A20	0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   disable clock line */
#define KB_UNSET_A20	0xdd		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   disable clock line */

#define SCP_A		0x92		/* System Control Port A */

enum { Disable_A20 = 0x2400, Enable_A20 = 0x2401, Query_A20_Status = 0x2402,
	Query_A20_Support = 0x2403 };

enum a20_methods {
	A20_UNKNOWN = 0,
	A20_INT15,
	A20_KBC,
	A20_SCPA,
};

#define A20_MAX_RETRIES		32
#define A20_INT15_RETRIES	32
#define A20_KBC_RETRIES		(2^21)
#define A20_SCPA_RETRIES	(2^21)

/**
 * Drain keyboard controller
 */
static void empty_8042 ( void ) {
	unsigned long time;

	time = currticks() + TICKS_PER_SEC;	/* max wait of 1 second */
	while ( ( inb ( K_CMD ) & ( K_IBUF_FUL | K_OBUF_FUL ) ) &&
		currticks() < time ) {
		iodelay();
		( void ) inb_p ( K_RDWR );
		iodelay();
	}
}

/**
 * Fast test to see if gate A20 is already set
 *
 * @v retries		Number of times to retry before giving up
 * @ret set		Gate A20 is set
 */
static int gateA20_is_set ( int retries ) {
	static uint32_t test_pattern = 0xdeadbeef;
	physaddr_t test_pattern_phys = virt_to_phys ( &test_pattern );
	physaddr_t verify_pattern_phys = ( test_pattern_phys ^ 0x100000 );
	userptr_t verify_pattern_user = phys_to_user ( verify_pattern_phys );
	uint32_t verify_pattern;

	do {
		/* Check for difference */
		copy_from_user ( &verify_pattern, verify_pattern_user, 0,
				 sizeof ( verify_pattern ) );
		if ( verify_pattern != test_pattern )
			return 1;

		/* Avoid false negatives */
		test_pattern++;

		iodelay();

		/* Always retry at least once, to avoid false negatives */
	} while ( retries-- >= 0 );

	/* Pattern matched every time; gate A20 is not set */
	return 0;
}

/*
 * Gate A20 for high memory
 *
 * Note that this function gets called as part of the return path from
 * librm's real_call, which is used to make the int15 call if librm is
 * being used.  To avoid an infinite recursion, we make gateA20_set
 * return immediately if it is already part of the call stack.
 */
void gateA20_set ( void ) {
	static char reentry_guard = 0;
	static int a20_method = A20_UNKNOWN;
	unsigned int discard_a;
	unsigned int scp_a;
	int retries = 0;

	/* Avoid potential infinite recursion */
	if ( reentry_guard )
		return;
	reentry_guard = 1;

	/* Fast check to see if gate A20 is already enabled */
	if ( gateA20_is_set ( 0 ) )
		goto out;

	for ( ; retries < A20_MAX_RETRIES ; retries++ ) {
		switch ( a20_method ) {
		case A20_UNKNOWN:
		case A20_INT15:
			/* Try INT 15 method */
			__asm__ __volatile__ ( REAL_CODE ( "int $0x15" )
					       : "=a" ( discard_a )
					       : "a" ( Enable_A20 ) );
			if ( gateA20_is_set ( A20_INT15_RETRIES ) ) {
				DBG ( "Enabled gate A20 using BIOS\n" );
				a20_method = A20_INT15;
				goto out;
			}
			/* fall through */
		case A20_KBC:
			/* Try keyboard controller method */
			empty_8042();
			outb ( KC_CMD_WOUT, K_CMD );
			empty_8042();
			outb ( KB_SET_A20, K_RDWR );
			empty_8042();
			outb ( KC_CMD_NULL, K_CMD );
			empty_8042();
			if ( gateA20_is_set ( A20_KBC_RETRIES ) ) {
				DBG ( "Enabled gate A20 using "
				      "keyboard controller\n" );
				a20_method = A20_KBC;
				goto out;
			}
			/* fall through */
		case A20_SCPA:
			/* Try "Fast gate A20" method */
			scp_a = inb ( SCP_A );
			scp_a &= ~0x01; /* Avoid triggering a reset */
			scp_a |= 0x02; /* Enable A20 */
			iodelay();
			outb ( scp_a, SCP_A );
			iodelay();
			if ( gateA20_is_set ( A20_SCPA_RETRIES ) ) {
				DBG ( "Enabled gate A20 using "
				      "Fast Gate A20\n" );
				a20_method = A20_SCPA;
				goto out;
			}
		}
	}

	/* Better to die now than corrupt memory later */
	printf ( "FATAL: Gate A20 stuck\n" );
	while ( 1 ) {}

 out:
	if ( retries )
		DBG ( "%d attempts were required to enable A20\n",
		      ( retries + 1 ) );
	reentry_guard = 0;
}

void gateA20_unset ( void ) {
	/* Not currently implemented */
}
