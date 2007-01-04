/** @file
 *
 * 
 *
 */

/*
 * Copyright (C) 2004 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "dev.h"
#include "pxe.h"

#warning "pxe_netdev should hold a persistent reference to the net device"
struct net_device *pxe_netdev = NULL;

#if 0

/* Global pointer to currently installed PXE stack */
pxe_stack_t *pxe_stack = NULL;

/* Various startup/shutdown routines.  The startup/shutdown call
 * sequence is incredibly badly defined in the Intel PXE spec, for
 * example:
 *
 *   PXENV_UNDI_INITIALIZE says that the parameters used to initialize
 *   the adaptor should be those supplied to the most recent
 *   PXENV_UNDI_STARTUP call.  PXENV_UNDI_STARTUP takes no parameters.
 *
 *   PXENV_UNDI_CLEANUP says that the rest of the API will not be
 *   available after making this call.  Figure 3-3 ("Early UNDI API
 *   usage") shows a call to PXENV_UNDI_CLEANUP being followed by a
 *   call to the supposedly now unavailable PXENV_STOP_UNDI.
 *
 *   PXENV_UNLOAD_BASE_STACK talks about freeing up the memory
 *   occupied by the PXE stack.  Figure 4-3 ("PXE IPL") shows a call
 *   to PXENV_STOP_UNDI being made after the call to
 *   PXENV_UNLOAD_BASE_STACK, by which time the entire PXE stack
 *   should have been freed (and, potentially, zeroed).
 *
 *   Nothing, anywhere, seems to mention who's responsible for freeing
 *   up the base memory allocated for the stack segment.  It's not
 *   even clear whether or not this is expected to be in free base
 *   memory rather than claimed base memory.
 *
 * Consequently, we adopt a rather defensive strategy, designed to
 * work with any conceivable sequence of initialisation or shutdown
 * calls.  We have only two things that we care about:
 *
 *   1. Have we hooked INT 1A and INT 15,E820(etc.)?
 *   2. Is the NIC initialised?
 *
 * The NIC should never be initialised without the vectors being
 * hooked, similarly the vectors should never be unhooked with the NIC
 * still initialised.  We do, however, want to be able to have the
 * vectors hooked with the NIC shutdown.  We therefore have three
 * possible states:
 *
 *   1. Ready to unload: interrupts unhooked, NIC shutdown.
 *   2. Midway: interrupts hooked, NIC shutdown.
 *   3. Fully ready: interrupts hooked, NIC initialised.
 *
 * We provide the three states CAN_UNLOAD, MIDWAY and READY to define
 * these, and the call pxe_ensure_state() to ensure that the stack is
 * in the specified state.  All our PXE API call implementations
 * should use this call to ensure that the state is as required for
 * that PXE API call.  This enables us to cope with whatever the
 * end-user's interpretation of the PXE spec may be.  It even allows
 * for someone calling e.g. PXENV_START_UNDI followed by
 * PXENV_UDP_WRITE, without bothering with any of the intervening
 * calls.
 *
 * pxe_ensure_state() returns 1 for success, 0 for failure.  In the
 * event of failure (which can arise from e.g. asking for state READY
 * when we don't know where our NIC is), the error code
 * PXENV_STATUS_UNDI_INVALID_STATE should be returned to the user.
 * The macros ENSURE_XXX() can be used to achieve this without lots of
 * duplicated code.
 */

/* pxe_[un]hook_stack are architecture-specific and provided in
 * pxe_callbacks.c
 */

int pxe_initialise_nic ( void ) {
	if ( pxe_stack->state >= READY ) return 1;

#warning "device probing mechanism has completely changed"
#if 0

	/* Check if NIC is initialised.  dev.disable is set to 0
	 * when disable() is called, so we use this.
	 */
	if ( dev.disable ) {
		/* NIC may have been initialised independently
		 * (e.g. when we set up the stack prior to calling the
		 * NBP).
		 */
		pxe_stack->state = READY;
		return 1;
	}

	/* If we already have a NIC defined, reuse that one with
	 * PROBE_AWAKE.  If one was specifed via PXENV_START_UNDI, try
	 * that one first.  Otherwise, set PROBE_FIRST.
	 */

	if ( dev.state.pci.dev.use_specified == 1 ) {
		dev.how_probe = PROBE_NEXT;
		DBG ( " initialising NIC specified via START_UNDI" );
	} else if ( dev.state.pci.dev.driver ) {
		DBG ( " reinitialising NIC" );
		dev.how_probe = PROBE_AWAKE;
	} else {
		DBG ( " probing for any NIC" );
		dev.how_probe = PROBE_FIRST;
	}

	/* Call probe routine to bring up the NIC */
	if ( eth_probe ( &dev ) != PROBE_WORKED ) {
		DBG ( " failed" );
		return 0;
	}
#endif
	

	pxe_stack->state = READY;
	return 1;
}

int pxe_shutdown_nic ( void ) {
	if ( pxe_stack->state <= MIDWAY ) return 1;

	eth_irq ( DISABLE );
	disable ( &dev );
	pxe_stack->state = MIDWAY;
	return 1;
}

int ensure_pxe_state ( pxe_stack_state_t wanted ) {
	int success = 1;

	if ( ! pxe_stack ) return 0;
	if ( wanted >= MIDWAY )
		success = success & hook_pxe_stack();
	if ( wanted > MIDWAY ) {
		success = success & pxe_initialise_nic();
	} else {
		success = success & pxe_shutdown_nic();
	}
	if ( wanted < MIDWAY )
		success = success & unhook_pxe_stack();
	return success;
}

#endif
