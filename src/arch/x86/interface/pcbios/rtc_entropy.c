/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * RTC-based entropy source
 *
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <biosint.h>
#include <pic8259.h>
#include <rtc.h>
#include <ipxe/cpuid.h>
#include <ipxe/entropy.h>

struct entropy_source rtc_entropy __entropy_source ( ENTROPY_NORMAL );

/** Maximum time to wait for an RTC interrupt, in milliseconds */
#define RTC_MAX_WAIT_MS 100

/** Number of RTC interrupts to check for */
#define RTC_CHECK_COUNT 3

/** RTC interrupt handler */
extern void rtc_isr ( void );

/** Previous RTC interrupt handler */
static struct segoff rtc_old_handler;

/** Previous RTC interrupt enabled state */
static uint8_t rtc_irq_enabled;

/** Previous RTC periodic interrupt enabled state */
static uint8_t rtc_int_enabled;

/** Flag set by RTC interrupt handler */
extern volatile uint8_t __text16 ( rtc_flag );
#define rtc_flag __use_text16 ( rtc_flag )

/**
 * Hook RTC interrupt handler
 *
 */
static void rtc_hook_isr ( void ) {

	/* RTC interrupt handler */
	__asm__ __volatile__ (
		TEXT16_CODE ( "\nrtc_isr:\n\t"
			      /* Preserve registers */
			      "pushw %%ax\n\t"
			      /* Set "interrupt triggered" flag */
			      "movb $0x01, %%cs:rtc_flag\n\t"
			      /* Read RTC status register C to
			       * acknowledge interrupt
			       */
			      "movb %2, %%al\n\t"
			      "outb %%al, %0\n\t"
			      "inb %1\n\t"
			      /* Send EOI */
			      "movb $0x20, %%al\n\t"
			      "outb %%al, $0xa0\n\t"
			      "outb %%al, $0x20\n\t"
			      /* Restore registers and return */
			      "popw %%ax\n\t"
			      "iret\n\t"
			      "\nrtc_flag:\n\t"
			      ".byte 0\n\t" )
		:
		: "i" ( CMOS_ADDRESS ), "i" ( CMOS_DATA ),
		  "i" ( RTC_STATUS_C ) );

	hook_bios_interrupt ( RTC_INT, ( intptr_t ) rtc_isr, &rtc_old_handler );
}

/**
 * Unhook RTC interrupt handler
 *
 */
static void rtc_unhook_isr ( void ) {
	int rc;

	rc = unhook_bios_interrupt ( RTC_INT, ( intptr_t ) rtc_isr,
				     &rtc_old_handler );
	assert ( rc == 0 ); /* Should always be able to unhook */
}

/**
 * Enable RTC interrupts
 *
 * @ret enabled		Periodic interrupt was previously enabled
 */
static int rtc_enable_int ( void ) {
	uint8_t status_b;

	/* Clear any stale pending interrupts via status register C */
	outb ( ( RTC_STATUS_C | CMOS_DISABLE_NMI ), CMOS_ADDRESS );
	inb ( CMOS_DATA );

	/* Set Periodic Interrupt Enable bit in status register B */
	outb ( ( RTC_STATUS_B | CMOS_DISABLE_NMI ), CMOS_ADDRESS );
	status_b = inb ( CMOS_DATA );
	outb ( ( RTC_STATUS_B | CMOS_DISABLE_NMI ), CMOS_ADDRESS );
	outb ( ( status_b | RTC_STATUS_B_PIE ), CMOS_DATA );

	/* Re-enable NMI and reset to default address */
	outb ( CMOS_DEFAULT_ADDRESS, CMOS_ADDRESS );
	inb ( CMOS_DATA ); /* Discard; may be needed on some platforms */

	/* Return previous state */
	return ( status_b & RTC_STATUS_B_PIE );
}

/**
 * Disable RTC interrupts
 *
 */
static void rtc_disable_int ( void ) {
	uint8_t status_b;

	/* Clear Periodic Interrupt Enable bit in status register B */
	outb ( ( RTC_STATUS_B | CMOS_DISABLE_NMI ), CMOS_ADDRESS );
	status_b = inb ( CMOS_DATA );
	outb ( ( RTC_STATUS_B | CMOS_DISABLE_NMI ), CMOS_ADDRESS );
	outb ( ( status_b & ~RTC_STATUS_B_PIE ), CMOS_DATA );

	/* Re-enable NMI and reset to default address */
	outb ( CMOS_DEFAULT_ADDRESS, CMOS_ADDRESS );
	inb ( CMOS_DATA ); /* Discard; may be needed on some platforms */
}

/**
 * Check that entropy gathering is functional
 *
 * @ret rc		Return status code
 */
static int rtc_entropy_check ( void ) {
	unsigned int count = 0;
	unsigned int i;

	/* Check that RTC interrupts are working */
	rtc_flag = 0;
	for ( i = 0 ; i < RTC_MAX_WAIT_MS ; i++ ) {

		/* Allow interrupts to occur */
		__asm__ __volatile__ ( "sti\n\t"
				       "nop\n\t"
				       "nop\n\t"
				       "cli\n\t" );

		/* Check for RTC interrupt flag */
		if ( rtc_flag ) {
			rtc_flag = 0;
			if ( ++count >= RTC_CHECK_COUNT )
				return 0;
		}

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( &rtc_flag, "RTC timed out waiting for interrupt %d/%d\n",
	       ( count + 1 ), RTC_CHECK_COUNT );
	return -ETIMEDOUT;
}

/**
 * Enable entropy gathering
 *
 * @ret rc		Return status code
 */
static int rtc_entropy_enable ( void ) {
	struct x86_features features;
	int rc;

	/* Check that TSC is supported */
	x86_features ( &features );
	if ( ! ( features.intel.edx & CPUID_FEATURES_INTEL_EDX_TSC ) ) {
		DBGC ( &rtc_flag, "RTC has no TSC\n" );
		rc = -ENOTSUP;
		goto err_no_tsc;
	}

	/* Hook ISR and enable RTC interrupts */
	rtc_hook_isr();
	rtc_irq_enabled = enable_irq ( RTC_IRQ );
	rtc_int_enabled = rtc_enable_int();
	DBGC ( &rtc_flag, "RTC had IRQ%d %sabled, interrupt %sabled\n",
	       RTC_IRQ, ( rtc_irq_enabled ? "en" : "dis" ),
	       ( rtc_int_enabled ? "en" : "dis" ) );

	/* Check that RTC interrupts are working */
	if ( ( rc = rtc_entropy_check() ) != 0 )
		goto err_check;

	/* The min-entropy has been measured on several platforms
	 * using the entropy_sample test code.  Modelling the samples
	 * as independent, and using a confidence level of 99.99%, the
	 * measurements were as follows:
	 *
	 *    qemu-kvm		: 7.38 bits
	 *    VMware		: 7.46 bits
	 *    Physical hardware	: 2.67 bits
	 *
	 * We choose the lowest of these (2.67 bits) and apply a 50%
	 * safety margin to allow for some potential non-independence
	 * of samples.
	 */
	entropy_init ( &rtc_entropy, MIN_ENTROPY ( 1.3 ) );

	return 0;

 err_check:
	if ( ! rtc_int_enabled )
		rtc_disable_int();
	if ( ! rtc_irq_enabled )
		disable_irq ( RTC_IRQ );
	rtc_unhook_isr();
 err_no_tsc:
	return rc;
}

/**
 * Disable entropy gathering
 *
 */
static void rtc_entropy_disable ( void ) {

	/* Restore RTC interrupt state and unhook ISR */
	if ( ! rtc_int_enabled )
		rtc_disable_int();
	if ( ! rtc_irq_enabled )
		disable_irq ( RTC_IRQ );
	rtc_unhook_isr();
}

/**
 * Get noise sample
 *
 * @ret noise		Noise sample
 * @ret rc		Return status code
 */
static int rtc_get_noise ( noise_sample_t *noise ) {
	uint32_t before;
	uint32_t after;
	uint32_t temp;

	__asm__ __volatile__ (
		REAL_CODE ( /* Enable interrupts */
			    "sti\n\t"
			    /* Wait for RTC interrupt */
			    "movb %b2, %%cs:rtc_flag\n\t"
			    "\n1:\n\t"
			    "xchgb %b2, %%cs:rtc_flag\n\t" /* Serialize */
			    "testb %b2, %b2\n\t"
			    "jz 1b\n\t"
			    /* Read "before" TSC */
			    "rdtsc\n\t"
			    /* Store "before" TSC on stack */
			    "pushl %0\n\t"
			    /* Wait for another RTC interrupt */
			    "xorb %b2, %b2\n\t"
			    "movb %b2, %%cs:rtc_flag\n\t"
			    "\n1:\n\t"
			    "xchgb %b2, %%cs:rtc_flag\n\t" /* Serialize */
			    "testb %b2, %b2\n\t"
			    "jz 1b\n\t"
			    /* Read "after" TSC */
			    "rdtsc\n\t"
			    /* Retrieve "before" TSC on stack */
			    "popl %1\n\t"
			    /* Disable interrupts */
			    "cli\n\t"
			    )
		: "=a" ( after ), "=d" ( before ), "=Q" ( temp )
		: "2" ( 0 ) );

	*noise = ( after - before );
	return 0;
}

/** RTC entropy source */
struct entropy_source rtc_entropy __entropy_source ( ENTROPY_NORMAL ) = {
	.name = "rtc",
	.enable = rtc_entropy_enable,
	.disable = rtc_entropy_disable,
	.get_noise = rtc_get_noise,
};
