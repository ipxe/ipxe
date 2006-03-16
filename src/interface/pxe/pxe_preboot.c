/** @file
 *
 * PXE Preboot API
 *
 */

/* PXE API interface for Etherboot.
 *
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

#include "pxe.h"
#include "pxe_callbacks.h"

/**
 * UNLOAD BASE CODE STACK
 *
 * @v None				-
 * @ret ...
 *
 */
PXENV_EXIT_t pxenv_unload_stack ( struct s_PXENV_UNLOAD_STACK *unload_stack ) {
	int success;

	DBG ( "PXENV_UNLOAD_STACK" );
	success = ensure_pxe_state ( CAN_UNLOAD );

	/* We need to call cleanup() at some point.  The network card
	 * has already been disabled by ENSURE_CAN_UNLOAD(), but for
	 * the sake of completeness we should call the console_fini()
	 * etc. that are part of cleanup().
	 *
	 * There seems to be a lack of consensus on which is the final
	 * PXE API call to make, but it's a fairly safe bet that all
	 * the potential shutdown sequences will include a call to
	 * PXENV_UNLOAD_STACK at some point, so we may as well do it
	 * here.
	 */
	cleanup();

	if ( ! success ) {
		unload_stack->Status = PXENV_STATUS_KEEP_ALL;
		return PXENV_EXIT_FAILURE;
	}

	unload_stack->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_GET_CACHED_INFO
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_get_cached_info ( struct s_PXENV_GET_CACHED_INFO
				     *get_cached_info ) {
	BOOTPLAYER_t *cached_info = &pxe_stack->cached_info;
	DBG ( "PXENV_GET_CACHED_INFO %d", get_cached_info->PacketType );
	ENSURE_READY ( get_cached_info );

	/* Fill in cached_info structure in our pxe_stack */

	/* I don't think there's actually any way we can be called in
	 * the middle of a DHCP request... 
	 */
	cached_info->opcode = BOOTP_REP;
	/* We only have Ethernet drivers */
	cached_info->Hardware = ETHER_TYPE;
	cached_info->Hardlen = ETH_ALEN;
	/* PXESPEC: "Client sets" says the spec, but who's filling in
	 * this structure?  It ain't the client.
	 */
	cached_info->Gatehops = 0;
	cached_info->ident = 0;
	cached_info->seconds = 0;
	cached_info->Flags = BOOTP_BCAST;
	/* PXESPEC: What do 'Client' and 'Your' IP address refer to? */
	cached_info->cip = arptable[ARP_CLIENT].ipaddr.s_addr;
	cached_info->yip = arptable[ARP_CLIENT].ipaddr.s_addr;
	cached_info->sip = arptable[ARP_SERVER].ipaddr.s_addr;
	/* PXESPEC: Does "GIP" mean "Gateway" or "Relay agent"? */
	cached_info->gip = arptable[ARP_GATEWAY].ipaddr.s_addr;
	memcpy ( cached_info->CAddr, arptable[ARP_CLIENT].node, ETH_ALEN );
	/* Nullify server name */
	cached_info->Sname[0] = '\0';
	memcpy ( cached_info->bootfile, KERNEL_BUF,
		 sizeof(cached_info->bootfile) );
	/* Copy DHCP vendor options */
	memcpy ( &cached_info->vendor.d, bootp_data.bootp_reply.bp_vend,
		 sizeof(cached_info->vendor.d) );
	
	/* Copy to user-specified buffer, or set pointer to our buffer */
	get_cached_info->BufferLimit = sizeof(*cached_info);
	/* PXESPEC: says to test for Buffer == NULL *and* BufferSize =
	 * 0, but what are we supposed to do with a null buffer of
	 * non-zero size?!
	 */
	if ( IS_NULL_SEGOFF16 ( get_cached_info->Buffer ) ) {
		/* Point back to our buffer */
		PTR_TO_SEGOFF16 ( cached_info, get_cached_info->Buffer );
		get_cached_info->BufferSize = sizeof(*cached_info);
	} else {
		/* Copy to user buffer */
		size_t size = sizeof(*cached_info);
		void *buffer = SEGOFF16_TO_PTR ( get_cached_info->Buffer );
		if ( get_cached_info->BufferSize < size )
			size = get_cached_info->BufferSize;
		DBG ( " to %x", virt_to_phys ( buffer ) );
		memcpy ( buffer, cached_info, size );
		/* PXESPEC: Should we return an error if the user
		 * buffer is too small?  We do return the actual size
		 * of the buffer via BufferLimit, so the user does
		 * have a way to detect this already.
		 */
	}

	get_cached_info->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_RESTART_TFTP
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_restart_tftp ( struct s_PXENV_TFTP_READ_FILE
				  *restart_tftp ) {
	PXENV_EXIT_t tftp_exit;

	DBG ( "PXENV_RESTART_TFTP" );
	ENSURE_READY ( restart_tftp );

	/* Words cannot describe the complete mismatch between the PXE
	 * specification and any possible version of reality...
	 */
	restart_tftp->Buffer = PXE_LOAD_ADDRESS; /* Fixed by spec, apparently */
	restart_tftp->BufferSize = get_free_base_memory() - PXE_LOAD_ADDRESS; /* Near enough */
	DBG ( "(" );
	tftp_exit = pxe_api_call ( PXENV_TFTP_READ_FILE, (union u_PXENV_ANY*)restart_tftp );
	DBG ( ")" );
	if ( tftp_exit != PXENV_EXIT_SUCCESS ) return tftp_exit;

	/* Fire up the new NBP */
	restart_tftp->Status = xstartpxe();

	/* Not sure what "SUCCESS" actually means, since we can only
	 * return if the new NBP failed to boot...
	 */
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_START_UNDI
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_start_undi ( struct s_PXENV_START_UNDI *start_undi ) {
	unsigned char bus, devfn;

	DBG ( "PXENV_START_UNDI" );
	ENSURE_MIDWAY(start_undi);

	/* Record PCI bus & devfn passed by caller, so we know which
	 * NIC they want to use.
	 *
	 * If they don't match our already-existing NIC structure, set
	 * values to ensure that the specified NIC is used at the next
	 * call to pxe_intialise_nic().
	 */
	bus = ( start_undi->AX >> 8 ) & 0xff;
	devfn = start_undi->AX & 0xff;

#warning "device probing mechanism has completely changed"
#if 0
	if ( ( pci->dev.driver == NULL ) ||
	     ( pci->dev.bus != bus ) || ( pci->dev.devfn != devfn ) ) {
		/* This is quite a bit of a hack and relies on
		 * knowledge of the internal operation of Etherboot's
		 * probe mechanism.
		 */
		DBG ( " set PCI %hhx:%hhx.%hhx",
		      bus, PCI_SLOT(devfn), PCI_FUNC(devfn) );
		dev->type = BOOT_NIC;
		dev->to_probe = PROBE_PCI;
		memset ( &dev->state, 0, sizeof(dev->state) );
		pci->advance = 1;
		pci->dev.use_specified = 1;
		pci->dev.bus = bus;
		pci->dev.devfn = devfn;
	}
#endif

	start_undi->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_STOP_UNDI
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_stop_undi ( struct s_PXENV_STOP_UNDI *stop_undi ) {
	DBG ( "PXENV_STOP_UNDI" );

	if ( ! ensure_pxe_state(CAN_UNLOAD) ) {
		stop_undi->Status = PXENV_STATUS_KEEP_UNDI;
		return PXENV_EXIT_FAILURE;
	}

	stop_undi->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/* PXENV_START_BASE
 *
 * Status: won't implement (requires major structural changes)
 */
PXENV_EXIT_t pxenv_start_base ( struct s_PXENV_START_BASE *start_base ) {
	DBG ( "PXENV_START_BASE" );
	/* ENSURE_READY ( start_base ); */
	start_base->Status = PXENV_STATUS_UNSUPPORTED;
	return PXENV_EXIT_FAILURE;
}

/* PXENV_STOP_BASE
 *
 * Status: working
 */
PXENV_EXIT_t pxenv_stop_base ( struct s_PXENV_STOP_BASE *stop_base ) {
	DBG ( "PXENV_STOP_BASE" );

	/* The only time we will be called is when the NBP is trying
	 * to shut down the PXE stack.  There's nothing we need to do
	 * in this call.
	 */

	stop_base->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}
