/** @file
 *
 * PXE UNDI loader
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

#warning "Currently broken"
#if 0
#include "pxe.h"

/* PXENV_UNDI_LOADER
 *
 * Status: working
 *
 * NOTE: This is not a genuine PXE API call; the loader has a separate
 * entry point.  However, to simplify the mapping of the PXE API to
 * the internal Etherboot API, both are directed through the same
 * interface.
 */
PXENV_EXIT_t undi_loader ( struct s_UNDI_LOADER *undi_loader ) {
	uint32_t loader_phys = virt_to_phys ( undi_loader );

	DBG ( "PXENV_UNDI_LOADER" );
	
	/* Set UNDI DS as our real-mode stack */
	use_undi_ds_for_rm_stack ( undi_loader->undi_ds );

	/* FIXME: These lines are borrowed from main.c.  There should
	 * probably be a single initialise() function that does all
	 * this, but it's currently split interestingly between main()
	 * and main_loop()...
	 */


	/* CHECKME: Our init functions have probably already been
	   called by the ROM prefix's call to setup(), haven't
	   they? */



	/* We have relocated; the loader pointer is now invalid */
	undi_loader = phys_to_virt ( loader_phys );

	/* Install PXE stack to area specified by NBP */
	install_pxe_stack ( VIRTUAL ( undi_loader->undi_cs, 0 ) );
	
	/* Call pxenv_start_undi to set parameters.  Why the hell PXE
	 * requires these parameters to be provided twice is beyond
	 * the wit of any sane man.  Don't worry if it fails; the NBP
	 * should call PXENV_START_UNDI separately anyway.
	 */
	pxenv_start_undi ( &undi_loader->u.start_undi );
	/* Unhook stack; the loader is not meant to hook int 1a etc,
	 * but the call the pxenv_start_undi will cause it to happen.
	 */

	/* FIXME: can't use ENSURE_CAN_UNLOAD() thanks to newer gcc's
	 * barfing on unnamed struct/unions. */
	/*	ENSURE_CAN_UNLOAD ( undi_loader ); */

	/* Fill in addresses of !PXE and PXENV+ structures */
	PTR_TO_SEGOFF16 ( &pxe_stack->pxe, undi_loader->pxe_ptr );
	PTR_TO_SEGOFF16 ( &pxe_stack->pxenv, undi_loader->pxenv_ptr );
	
	undi_loader->u.Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}
#endif
