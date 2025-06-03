/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <string.h>
#include <unistd.h>
#include <byteswap.h>
#include <pxe.h>
#include <realmode.h>
#include <pic8259.h>
#include <biosint.h>
#include <pnpbios.h>
#include <basemem_packet.h>
#include <ipxe/io.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/if_ether.h>
#include <ipxe/ethernet.h>
#include <ipxe/pci.h>
#include <ipxe/profile.h>
#include <undi.h>
#include <undinet.h>

/** @file
 *
 * UNDI network device driver
 *
 */

/** An UNDI NIC */
struct undi_nic {
	/** Device supports IRQs */
	int irq_supported;
	/** Assigned IRQ number */
	unsigned int irq;
	/** Currently processing ISR */
	int isr_processing;
	/** Bug workarounds */
	int hacks;
};

/* Disambiguate the various error causes */
#define EINFO_EPXECALL							\
	__einfo_uniqify ( EINFO_EPLATFORM, 0x01,			\
			  "External PXE API error" )
#define EPXECALL( status ) EPLATFORM ( EINFO_EPXECALL, status )

/**
 * @defgroup undi_hacks UNDI workarounds
 * @{
 */

/** Work around Etherboot 5.4 bugs */
#define UNDI_HACK_EB54		0x0001

/** @} */

/** Maximum number of times to retry PXENV_UNDI_INITIALIZE */
#define UNDI_INITIALIZE_RETRY_MAX 10

/** Delay between retries of PXENV_UNDI_INITIALIZE */
#define UNDI_INITIALIZE_RETRY_DELAY_MS 200

/** Maximum number of received packets per poll */
#define UNDI_RX_QUOTA 4

/** Alignment of received frame payload */
#define UNDI_RX_ALIGN 16

static void undinet_close ( struct net_device *netdev );

/**
 * UNDI parameter block
 *
 * Used as the parameter block for all UNDI API calls.  Resides in
 * base memory.
 */
static union u_PXENV_ANY __bss16 ( undinet_params );
#define undinet_params __use_data16 ( undinet_params )

/**
 * UNDI entry point
 *
 * Used as the indirection vector for all UNDI API calls.  Resides in
 * base memory.
 */
SEGOFF16_t __bss16 ( undinet_entry_point );
#define undinet_entry_point __use_data16 ( undinet_entry_point )

/* Read TSC in real mode only when profiling */
#if PROFILING
#define RDTSC_IF_PROFILING "rdtsc\n\t"
#else
#define RDTSC_IF_PROFILING ""
#endif

/** IRQ profiler */
static struct profiler undinet_irq_profiler __profiler =
	{ .name = "undinet.irq" };

/** Receive profiler */
static struct profiler undinet_rx_profiler __profiler =
	{ .name = "undinet.rx" };

/** A PXE API call breakdown profiler */
struct undinet_profiler {
	/** Total time spent performing REAL_CALL() */
	struct profiler total;
	/** Time spent transitioning to real mode */
	struct profiler p2r;
	/** Time spent in external code */
	struct profiler ext;
	/** Time spent transitioning back to protected mode */
	struct profiler r2p;
};

/** PXENV_UNDI_TRANSMIT profiler */
static struct undinet_profiler undinet_tx_profiler __profiler = {
	{ .name = "undinet.tx" },
	{ .name = "undinet.tx_p2r" },
	{ .name = "undinet.tx_ext" },
	{ .name = "undinet.tx_r2p" },
};

/** PXENV_UNDI_ISR profiler
 *
 * Note that this profiler will not see calls to
 * PXENV_UNDI_ISR_IN_START, which are handled by the UNDI ISR and do
 * not go via undinet_call().
 */
static struct undinet_profiler undinet_isr_profiler __profiler = {
	{ .name = "undinet.isr" },
	{ .name = "undinet.isr_p2r" },
	{ .name = "undinet.isr_ext" },
	{ .name = "undinet.isr_r2p" },
};

/** PXE unknown API call profiler
 *
 * This profiler can be used to measure the overhead of a dummy PXE
 * API call.
 */
static struct undinet_profiler undinet_unknown_profiler __profiler = {
	{ .name = "undinet.unknown" },
	{ .name = "undinet.unknown_p2r" },
	{ .name = "undinet.unknown_ext" },
	{ .name = "undinet.unknown_r2p" },
};

/** Miscellaneous PXE API call profiler */
static struct undinet_profiler undinet_misc_profiler __profiler = {
	{ .name = "undinet.misc" },
	{ .name = "undinet.misc_p2r" },
	{ .name = "undinet.misc_ext" },
	{ .name = "undinet.misc_r2p" },
};

/*****************************************************************************
 *
 * UNDI API call
 *
 *****************************************************************************
 */

/**
 * Name PXE API call
 *
 * @v function		API call number
 * @ret name		API call name
 */
static inline __attribute__ (( always_inline )) const char *
undinet_function_name ( unsigned int function ) {
	switch ( function ) {
	case PXENV_START_UNDI:
		return "PXENV_START_UNDI";
	case PXENV_STOP_UNDI:
		return "PXENV_STOP_UNDI";
	case PXENV_UNDI_STARTUP:
		return "PXENV_UNDI_STARTUP";
	case PXENV_UNDI_CLEANUP:
		return "PXENV_UNDI_CLEANUP";
	case PXENV_UNDI_INITIALIZE:
		return "PXENV_UNDI_INITIALIZE";
	case PXENV_UNDI_RESET_ADAPTER:
		return "PXENV_UNDI_RESET_ADAPTER";
	case PXENV_UNDI_SHUTDOWN:
		return "PXENV_UNDI_SHUTDOWN";
	case PXENV_UNDI_OPEN:
		return "PXENV_UNDI_OPEN";
	case PXENV_UNDI_CLOSE:
		return "PXENV_UNDI_CLOSE";
	case PXENV_UNDI_TRANSMIT:
		return "PXENV_UNDI_TRANSMIT";
	case PXENV_UNDI_SET_MCAST_ADDRESS:
		return "PXENV_UNDI_SET_MCAST_ADDRESS";
	case PXENV_UNDI_SET_STATION_ADDRESS:
		return "PXENV_UNDI_SET_STATION_ADDRESS";
	case PXENV_UNDI_SET_PACKET_FILTER:
		return "PXENV_UNDI_SET_PACKET_FILTER";
	case PXENV_UNDI_GET_INFORMATION:
		return "PXENV_UNDI_GET_INFORMATION";
	case PXENV_UNDI_GET_STATISTICS:
		return "PXENV_UNDI_GET_STATISTICS";
	case PXENV_UNDI_CLEAR_STATISTICS:
		return "PXENV_UNDI_CLEAR_STATISTICS";
	case PXENV_UNDI_INITIATE_DIAGS:
		return "PXENV_UNDI_INITIATE_DIAGS";
	case PXENV_UNDI_FORCE_INTERRUPT:
		return "PXENV_UNDI_FORCE_INTERRUPT";
	case PXENV_UNDI_GET_MCAST_ADDRESS:
		return "PXENV_UNDI_GET_MCAST_ADDRESS";
	case PXENV_UNDI_GET_NIC_TYPE:
		return "PXENV_UNDI_GET_NIC_TYPE";
	case PXENV_UNDI_GET_IFACE_INFO:
		return "PXENV_UNDI_GET_IFACE_INFO";
	/*
	 * Duplicate case value; this is a bug in the PXE specification.
	 *
	 *	case PXENV_UNDI_GET_STATE:
	 *		return "PXENV_UNDI_GET_STATE";
	 */
	case PXENV_UNDI_ISR:
		return "PXENV_UNDI_ISR";
	case PXENV_GET_CACHED_INFO:
		return "PXENV_GET_CACHED_INFO";
	default:
		return "UNKNOWN API CALL";
	}
}

/**
 * Determine applicable profiler pair (for debugging)
 *
 * @v function		API call number
 * @ret profiler	Profiler
 */
static struct undinet_profiler * undinet_profiler ( unsigned int function ) {

	/* Determine applicable profiler */
	switch ( function ) {
	case PXENV_UNDI_TRANSMIT:
		return &undinet_tx_profiler;
	case PXENV_UNDI_ISR:
		return &undinet_isr_profiler;
	case PXENV_UNKNOWN:
		return &undinet_unknown_profiler;
	default:
		return &undinet_misc_profiler;
	}
}

/**
 * Issue UNDI API call
 *
 * @v undinic		UNDI NIC
 * @v function		API call number
 * @v params		PXE parameter block
 * @v params_len	Length of PXE parameter block
 * @ret rc		Return status code
 */
static int undinet_call ( struct undi_nic *undinic, unsigned int function,
			  void *params, size_t params_len ) {
	struct undinet_profiler *profiler = undinet_profiler ( function );
	PXENV_EXIT_t exit;
	uint32_t before;
	uint32_t started;
	uint32_t stopped;
	uint32_t after;
	int discard_D;
	int rc;

	/* Copy parameter block and entry point */
	assert ( params_len <= sizeof ( undinet_params ) );
	memcpy ( &undinet_params, params, params_len );

	/* Call real-mode entry point.  This calling convention will
	 * work with both the !PXE and the PXENV+ entry points.
	 */
	profile_start ( &profiler->total );
	__asm__ __volatile__ ( REAL_CODE ( "pushl %%ebp\n\t" /* gcc bug */
					   RDTSC_IF_PROFILING
					   "pushl %%eax\n\t"
					   "pushw %%es\n\t"
					   "pushw %%di\n\t"
					   "pushw %%bx\n\t"
					   "lcall *undinet_entry_point\n\t"
					   "movw %%ax, %%bx\n\t"
					   RDTSC_IF_PROFILING
					   "addw $6, %%sp\n\t"
					   "popl %%edx\n\t"
					   "popl %%ebp\n\t" /* gcc bug */ )
			       : "=a" ( stopped ), "=d" ( started ),
				 "=b" ( exit ), "=D" ( discard_D )
			       : "b" ( function ),
			         "D" ( __from_data16 ( &undinet_params ) )
			       : "ecx", "esi" );
	profile_stop ( &profiler->total );
	before = profile_started ( &profiler->total );
	after = profile_stopped ( &profiler->total );
	profile_start_at ( &profiler->p2r, before );
	profile_stop_at ( &profiler->p2r, started );
	profile_start_at ( &profiler->ext, started );
	profile_stop_at ( &profiler->ext, stopped );
	profile_start_at ( &profiler->r2p, stopped );
	profile_stop_at ( &profiler->r2p, after );

	/* Determine return status code based on PXENV_EXIT and
	 * PXENV_STATUS
	 */
	rc = ( ( exit == PXENV_EXIT_SUCCESS ) ?
	       0 : -EPXECALL ( undinet_params.Status ) );

	/* If anything goes wrong, print as much debug information as
	 * it's possible to give.
	 */
	if ( rc != 0 ) {
		SEGOFF16_t rm_params = {
			.segment = rm_ds,
			.offset = __from_data16 ( &undinet_params ),
		};

		DBGC ( undinic, "UNDINIC %p %s failed: %s\n", undinic,
		       undinet_function_name ( function ), strerror ( rc ) );
		DBGC ( undinic, "UNDINIC %p parameters at %04x:%04x length "
		       "%#02zx, entry point at %04x:%04x\n", undinic,
		       rm_params.segment, rm_params.offset, params_len,
		       undinet_entry_point.segment,
		       undinet_entry_point.offset );
		DBGC ( undinic, "UNDINIC %p parameters provided:\n", undinic );
		DBGC_HDA ( undinic, rm_params, params, params_len );
		DBGC ( undinic, "UNDINIC %p parameters returned:\n", undinic );
		DBGC_HDA ( undinic, rm_params, &undinet_params, params_len );
	}

	/* Copy parameter block back */
	memcpy ( params, &undinet_params, params_len );

	return rc;
}

/*****************************************************************************
 *
 * UNDI interrupt service routine
 *
 *****************************************************************************
 */

/**
 * UNDI interrupt service routine
 *
 * The UNDI ISR increments a counter (@c trigger_count) and exits.
 */
extern void undiisr ( void );

/** IRQ number */
uint8_t __data16 ( undiisr_irq );
#define undiisr_irq __use_data16 ( undiisr_irq )

/** IRQ mask register */
uint16_t __data16 ( undiisr_imr );
#define undiisr_imr __use_data16 ( undiisr_imr )

/** IRQ mask bit */
uint8_t __data16 ( undiisr_bit );
#define undiisr_bit __use_data16 ( undiisr_bit )

/** IRQ rearm flag */
uint8_t __data16 ( undiisr_rearm );
#define undiisr_rearm __use_data16 ( undiisr_rearm )

/** IRQ chain vector */
struct segoff __data16 ( undiisr_next_handler );
#define undiisr_next_handler __use_data16 ( undiisr_next_handler )

/** IRQ trigger count */
volatile uint8_t __data16 ( undiisr_trigger_count ) = 0;
#define undiisr_trigger_count __use_data16 ( undiisr_trigger_count )

/** Last observed trigger count */
static unsigned int last_trigger_count = 0;

/**
 * Hook UNDI interrupt service routine
 *
 * @v irq		IRQ number
 */
static void undinet_hook_isr ( unsigned int irq ) {

	assert ( irq <= IRQ_MAX );
	assert ( undiisr_irq == 0 );

	undiisr_irq = irq;
	undiisr_imr = IMR_REG ( irq );
	undiisr_bit = IMR_BIT ( irq );
	undiisr_rearm = 0;
	hook_bios_interrupt ( IRQ_INT ( irq ), ( ( intptr_t ) undiisr ),
			      &undiisr_next_handler );
}

/**
 * Unhook UNDI interrupt service routine
 *
 * @v irq		IRQ number
 */
static void undinet_unhook_isr ( unsigned int irq ) {

	assert ( irq <= IRQ_MAX );

	unhook_bios_interrupt ( IRQ_INT ( irq ), ( ( intptr_t ) undiisr ),
				&undiisr_next_handler );
	undiisr_irq = 0;
}

/**
 * Test to see if UNDI ISR has been triggered
 *
 * @ret triggered	ISR has been triggered since last check
 */
static int undinet_isr_triggered ( void ) {
	unsigned int this_trigger_count;

	/* Read trigger_count.  Do this only once; it is volatile */
	this_trigger_count = undiisr_trigger_count;

	if ( this_trigger_count == last_trigger_count ) {
		/* Not triggered */
		return 0;
	} else {
		/* Triggered */
		last_trigger_count = this_trigger_count;
		return 1;
	}
}

/*****************************************************************************
 *
 * UNDI network device interface
 *
 *****************************************************************************
 */

/** UNDI transmit buffer descriptor */
static struct s_PXENV_UNDI_TBD __data16 ( undinet_tbd );
#define undinet_tbd __use_data16 ( undinet_tbd )

/** UNDI transmit destination address */
static uint8_t __data16_array ( undinet_destaddr, [ETH_ALEN] );
#define undinet_destaddr __use_data16 ( undinet_destaddr )

/**
 * Transmit packet
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int undinet_transmit ( struct net_device *netdev,
			      struct io_buffer *iobuf ) {
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_TRANSMIT undi_transmit;
	const void *ll_dest;
	const void *ll_source;
	uint16_t net_proto;
	unsigned int flags;
	uint8_t protocol;
	size_t len;
	int rc;

	/* Technically, we ought to make sure that the previous
	 * transmission has completed before we re-use the buffer.
	 * However, many PXE stacks (including at least some Intel PXE
	 * stacks and Etherboot 5.4) fail to generate TX completions.
	 * In practice this won't be a problem, since our TX datapath
	 * has a very low packet volume and we can get away with
	 * assuming that a TX will be complete by the time we want to
	 * transmit the next packet.
	 */

	/* Some PXE stacks are unable to cope with P_UNKNOWN, and will
	 * always try to prepend a link-layer header.  Work around
	 * these stacks by stripping the existing link-layer header
	 * and allowing the PXE stack to (re)construct the link-layer
	 * header itself.
	 */
	if ( ( rc = eth_pull ( netdev, iobuf, &ll_dest, &ll_source,
			       &net_proto, &flags ) ) != 0 ) {
		DBGC ( undinic, "UNDINIC %p could not strip Ethernet header: "
		       "%s\n", undinic, strerror ( rc ) );
		return rc;
	}
	memcpy ( undinet_destaddr, ll_dest, sizeof ( undinet_destaddr ) );
	switch ( net_proto ) {
	case htons ( ETH_P_IP ) :
		protocol = P_IP;
		break;
	case htons ( ETH_P_ARP ) :
		protocol = P_ARP;
		break;
	case htons ( ETH_P_RARP ) :
		protocol = P_RARP;
		break;
	default:
		/* Unknown protocol; restore the original link-layer header */
		iob_push ( iobuf, sizeof ( struct ethhdr ) );
		protocol = P_UNKNOWN;
		break;
	}

	/* Copy packet to UNDI I/O buffer */
	len = iob_len ( iobuf );
	if ( len > sizeof ( basemem_packet ) )
		len = sizeof ( basemem_packet );
	memcpy ( &basemem_packet, iobuf->data, len );

	/* Create PXENV_UNDI_TRANSMIT data structure */
	memset ( &undi_transmit, 0, sizeof ( undi_transmit ) );
	undi_transmit.Protocol = protocol;
	undi_transmit.XmitFlag = ( ( flags & LL_BROADCAST ) ?
				   XMT_BROADCAST : XMT_DESTADDR );
	undi_transmit.DestAddr.segment = rm_ds;
	undi_transmit.DestAddr.offset = __from_data16 ( &undinet_destaddr );
	undi_transmit.TBD.segment = rm_ds;
	undi_transmit.TBD.offset = __from_data16 ( &undinet_tbd );

	/* Create PXENV_UNDI_TBD data structure */
	undinet_tbd.ImmedLength = len;
	undinet_tbd.Xmit.segment = rm_ds;
	undinet_tbd.Xmit.offset = __from_data16 ( basemem_packet );

	/* Issue PXE API call */
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_TRANSMIT, &undi_transmit,
				   sizeof ( undi_transmit ) ) ) != 0 )
		goto done;

	/* Free I/O buffer */
	netdev_tx_complete ( netdev, iobuf );
 done:
	return rc;
}

/** 
 * Poll for received packets
 *
 * @v netdev		Network device
 *
 * Fun, fun, fun.  UNDI drivers don't use polling; they use
 * interrupts.  We therefore cheat and pretend that an interrupt has
 * occurred every time undinet_poll() is called.  This isn't too much
 * of a hack; PCI devices share IRQs and so the first thing that a
 * proper ISR should do is call PXENV_UNDI_ISR to determine whether or
 * not the UNDI NIC generated the interrupt; there is no harm done by
 * spurious calls to PXENV_UNDI_ISR.  Similarly, we wouldn't be
 * handling them any more rapidly than the usual rate of
 * undinet_poll() being called even if we did implement a full ISR.
 * So it should work.  Ha!
 *
 * Addendum (21/10/03).  Some cards don't play nicely with this trick,
 * so instead of doing it the easy way we have to go to all the hassle
 * of installing a genuine interrupt service routine and dealing with
 * the wonderful 8259 Programmable Interrupt Controller.  Joy.
 *
 * Addendum (10/07/07).  When doing things such as iSCSI boot, in
 * which we have to co-operate with a running OS, we can't get away
 * with the "ISR-just-increments-a-counter-and-returns" trick at all,
 * because it involves tying up the PIC for far too long, and other
 * interrupt-dependent components (e.g. local disks) start breaking.
 * We therefore implement a "proper" ISR which calls PXENV_UNDI_ISR
 * from within interrupt context in order to deassert the device
 * interrupt, and sends EOI if applicable.
 */
static void undinet_poll ( struct net_device *netdev ) {
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_ISR undi_isr;
	struct io_buffer *iobuf = NULL;
	unsigned int quota = UNDI_RX_QUOTA;
	size_t len;
	size_t reserve_len;
	size_t frag_len;
	size_t max_frag_len;
	int rc;

	if ( ! undinic->isr_processing ) {
		/* Allow interrupt to occur.  Do this even if
		 * interrupts are not known to be supported, since
		 * some cards erroneously report that they do not
		 * support interrupts.
		 */
		if ( ! undinet_isr_triggered() ) {

			/* Rearm interrupt if needed */
			if ( undiisr_rearm ) {
				undiisr_rearm = 0;
				assert ( undinic->irq != 0 );
				enable_irq ( undinic->irq );
			}

			/* Allow interrupt to occur */
			profile_start ( &undinet_irq_profiler );
			__asm__ __volatile__ ( "sti\n\t"
					       "nop\n\t"
					       "nop\n\t"
					       "cli\n\t" );
			profile_stop ( &undinet_irq_profiler );

			/* If interrupts are known to be supported,
			 * then do nothing on this poll; wait for the
			 * interrupt to be triggered.
			 */
			if ( undinic->irq_supported )
				return;
		}

		/* Start ISR processing */
		undinic->isr_processing = 1;
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_PROCESS;
	} else {
		/* Continue ISR processing */
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
	}

	/* Run through the ISR loop */
	while ( quota ) {
		if ( ( rc = undinet_call ( undinic, PXENV_UNDI_ISR, &undi_isr,
					   sizeof ( undi_isr ) ) ) != 0 ) {
			netdev_rx_err ( netdev, NULL, rc );
			break;
		}
		switch ( undi_isr.FuncFlag ) {
		case PXENV_UNDI_ISR_OUT_TRANSMIT:
			/* We don't care about transmit completions */
			break;
		case PXENV_UNDI_ISR_OUT_RECEIVE:
			/* Packet fragment received */
			profile_start ( &undinet_rx_profiler );
			len = undi_isr.FrameLength;
			frag_len = undi_isr.BufferLength;
			reserve_len = ( -undi_isr.FrameHeaderLength &
					( UNDI_RX_ALIGN - 1 ) );
			if ( ( len == 0 ) || ( len < frag_len ) ) {
				/* Don't laugh.  VMWare does it. */
				DBGC ( undinic, "UNDINIC %p reported insane "
				       "fragment (%zd of %zd bytes)\n",
				       undinic, frag_len, len );
				netdev_rx_err ( netdev, NULL, -EINVAL );
				break;
			}
			if ( ! iobuf ) {
				iobuf = alloc_iob ( reserve_len + len );
				if ( ! iobuf ) {
					DBGC ( undinic, "UNDINIC %p could not "
					       "allocate %zd bytes for RX "
					       "buffer\n", undinic, len );
					/* Fragment will be dropped */
					netdev_rx_err ( netdev, NULL, -ENOMEM );
					goto done;
				}
				iob_reserve ( iobuf, reserve_len );
			}
			max_frag_len = iob_tailroom ( iobuf );
			if ( frag_len > max_frag_len ) {
				DBGC ( undinic, "UNDINIC %p fragment too big "
				       "(%zd+%zd does not fit into %zd)\n",
				       undinic, iob_len ( iobuf ), frag_len,
				       ( iob_len ( iobuf ) + max_frag_len ) );
				frag_len = max_frag_len;
			}
			copy_from_real ( iob_put ( iobuf, frag_len ),
					 undi_isr.Frame.segment,
					 undi_isr.Frame.offset, frag_len );
			if ( iob_len ( iobuf ) == len ) {
				/* Whole packet received; deliver it */
				netdev_rx ( netdev, iob_disown ( iobuf ) );
				quota--;
				/* Etherboot 5.4 fails to return all packets
				 * under mild load; pretend it retriggered.
				 */
				if ( undinic->hacks & UNDI_HACK_EB54 )
					--last_trigger_count;
			}
			profile_stop ( &undinet_rx_profiler );
			break;
		case PXENV_UNDI_ISR_OUT_DONE:
			/* Processing complete */
			undinic->isr_processing = 0;
			goto done;
		default:
			/* Should never happen.  VMWare does it routinely. */
			DBGC ( undinic, "UNDINIC %p ISR returned invalid "
			       "FuncFlag %04x\n", undinic, undi_isr.FuncFlag );
			undinic->isr_processing = 0;
			goto done;
		}
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
	}

 done:
	if ( iobuf ) {
		DBGC ( undinic, "UNDINIC %p returned incomplete packet "
		       "(%zd of %zd)\n", undinic, iob_len ( iobuf ),
		       ( iob_len ( iobuf ) + iob_tailroom ( iobuf ) ) );
		netdev_rx_err ( netdev, iobuf, -EINVAL );
	}
}

/**
 * Open NIC
 *
 * @v netdev		Net device
 * @ret rc		Return status code
 */
static int undinet_open ( struct net_device *netdev ) {
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_SET_STATION_ADDRESS undi_set_address;
	struct s_PXENV_UNDI_OPEN undi_open;
	int rc;

	/* Hook interrupt service routine and enable interrupt if applicable */
	if ( undinic->irq ) {
		undinet_hook_isr ( undinic->irq );
		enable_irq ( undinic->irq );
		send_eoi ( undinic->irq );
	}

	/* Set station address.  Required for some PXE stacks; will
	 * spuriously fail on others.  Ignore failures.  We only ever
	 * use it to set the MAC address to the card's permanent value
	 * anyway.
	 */
	memcpy ( undi_set_address.StationAddress, netdev->ll_addr,
		 sizeof ( undi_set_address.StationAddress ) );
	undinet_call ( undinic, PXENV_UNDI_SET_STATION_ADDRESS,
		       &undi_set_address, sizeof ( undi_set_address ) );

	/* Open NIC.  We ask for promiscuous operation, since it's the
	 * only way to ask for all multicast addresses.  On any
	 * switched network, it shouldn't really make a difference to
	 * performance.
	 */
	memset ( &undi_open, 0, sizeof ( undi_open ) );
	undi_open.PktFilter = ( FLTR_DIRECTED | FLTR_BRDCST | FLTR_PRMSCS );
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_OPEN, &undi_open,
				   sizeof ( undi_open ) ) ) != 0 )
		goto err;

	DBGC ( undinic, "UNDINIC %p opened\n", undinic );
	return 0;

 err:
	undinet_close ( netdev );
	return rc;
}

/**
 * Close NIC
 *
 * @v netdev		Net device
 */
static void undinet_close ( struct net_device *netdev ) {
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_ISR undi_isr;
	struct s_PXENV_UNDI_CLOSE undi_close;
	int rc;

	/* Ensure ISR has exited cleanly */
	while ( undinic->isr_processing ) {
		undi_isr.FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
		if ( ( rc = undinet_call ( undinic, PXENV_UNDI_ISR, &undi_isr,
					   sizeof ( undi_isr ) ) ) != 0 )
			break;
		switch ( undi_isr.FuncFlag ) {
		case PXENV_UNDI_ISR_OUT_TRANSMIT:
		case PXENV_UNDI_ISR_OUT_RECEIVE:
			/* Continue draining */
			break;
		default:
			/* Stop processing */
			undinic->isr_processing = 0;
			break;
		}
	}

	/* Close NIC */
	undinet_call ( undinic, PXENV_UNDI_CLOSE, &undi_close,
		       sizeof ( undi_close ) );

	/* Disable interrupt and unhook ISR if applicable */
	if ( undinic->irq ) {
		disable_irq ( undinic->irq );
		undinet_unhook_isr ( undinic->irq );
	}

	DBGC ( undinic, "UNDINIC %p closed\n", undinic );
}

/**
 * Enable/disable interrupts
 *
 * @v netdev		Net device
 * @v enable		Interrupts should be enabled
 */
static void undinet_irq ( struct net_device *netdev, int enable ) {
	struct undi_nic *undinic = netdev->priv;

	/* Cannot support interrupts yet */
	DBGC ( undinic, "UNDINIC %p cannot %s interrupts\n",
	       undinic, ( enable ? "enable" : "disable" ) );
}

/** UNDI network device operations */
static struct net_device_operations undinet_operations = {
	.open		= undinet_open,
	.close		= undinet_close,
	.transmit	= undinet_transmit,
	.poll		= undinet_poll,
	.irq   		= undinet_irq,
};

/** A device with broken support for generating interrupts */
struct undinet_irq_broken {
	/** PCI vendor ID */
	uint16_t pci_vendor;
	/** PCI device ID */
	uint16_t pci_device;
	/** PCI subsystem vendor ID */
	uint16_t pci_subsys_vendor;
	/** PCI subsystem ID */
	uint16_t pci_subsys;
};

/**
 * List of devices with broken support for generating interrupts
 *
 * Some PXE stacks are known to claim that IRQs are supported, but
 * then never generate interrupts.  No satisfactory solution has been
 * found to this problem; the workaround is to add the PCI vendor and
 * device IDs to this list.  This is something of a hack, since it
 * will generate false positives for identical devices with a working
 * PXE stack (e.g. those that have been reflashed with iPXE), but it's
 * an improvement on the current situation.
 */
static const struct undinet_irq_broken undinet_irq_broken_list[] = {
	/* HP XX70x laptops */
	{ 0x8086, 0x1502, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x8086, 0x1503, PCI_ANY_ID, PCI_ANY_ID },
	/* HP 745 G3 laptop */
	{ 0x14e4, 0x1687, PCI_ANY_ID, PCI_ANY_ID },
	/* ASUSTeK KNPA-U16 server */
	{ 0x8086, 0x1521, 0x1043, PCI_ANY_ID },
};

/**
 * Check for devices with broken support for generating interrupts
 *
 * @v desc		Device description
 * @ret irq_is_broken	Interrupt support is broken; no interrupts are generated
 */
static int undinet_irq_is_broken ( struct device_description *desc ) {
	const struct undinet_irq_broken *broken;
	struct pci_device pci;
	uint16_t subsys_vendor;
	uint16_t subsys;
	unsigned int i;

	/* Ignore non-PCI devices */
	if ( desc->bus_type != BUS_TYPE_PCI )
		return 0;

	/* Read subsystem IDs */
	pci_init ( &pci, desc->location );
	pci_read_config_word ( &pci, PCI_SUBSYSTEM_VENDOR_ID, &subsys_vendor );
	pci_read_config_word ( &pci, PCI_SUBSYSTEM_ID, &subsys );

	/* Check for a match against the broken device list */
	for ( i = 0 ; i < ( sizeof ( undinet_irq_broken_list ) /
			    sizeof ( undinet_irq_broken_list[0] ) ) ; i++ ) {
		broken = &undinet_irq_broken_list[i];
		if ( ( broken->pci_vendor == desc->vendor ) &&
		     ( broken->pci_device == desc->device ) &&
		     ( ( broken->pci_subsys_vendor == subsys_vendor ) ||
		       ( broken->pci_subsys_vendor == PCI_ANY_ID ) ) &&
		     ( ( broken->pci_subsys == subsys ) ||
		       ( broken->pci_subsys == PCI_ANY_ID ) ) ) {
			return 1;
		}
	}
	return 0;
}

/**
 * Probe UNDI device
 *
 * @v undi		UNDI device
 * @v dev		Underlying generic device
 * @ret rc		Return status code
 */
int undinet_probe ( struct undi_device *undi, struct device *dev ) {
	struct net_device *netdev;
	struct undi_nic *undinic;
	struct s_PXENV_START_UNDI start_undi;
	struct s_PXENV_UNDI_STARTUP undi_startup;
	struct s_PXENV_UNDI_INITIALIZE undi_init;
	struct s_PXENV_UNDI_GET_INFORMATION undi_info;
	struct s_PXENV_UNDI_GET_IFACE_INFO undi_iface;
	struct s_PXENV_UNDI_SHUTDOWN undi_shutdown;
	struct s_PXENV_UNDI_CLEANUP undi_cleanup;
	struct s_PXENV_STOP_UNDI stop_undi;
	unsigned int retry;
	int rc;

	/* Allocate net device */
	netdev = alloc_etherdev ( sizeof ( *undinic ) );
	if ( ! netdev )
		return -ENOMEM;
	netdev_init ( netdev, &undinet_operations );
	undinic = netdev->priv;
	undi_set_drvdata ( undi, netdev );
	netdev->dev = dev;
	memset ( undinic, 0, sizeof ( *undinic ) );
	undinet_entry_point = undi->entry;
	DBGC ( undinic, "UNDINIC %p using UNDI %p\n", undinic, undi );

	/* Hook in UNDI stack */
	if ( ! ( undi->flags & UNDI_FL_STARTED ) ) {
		memset ( &start_undi, 0, sizeof ( start_undi ) );
		start_undi.AX = undi->pci_busdevfn;
		start_undi.BX = undi->isapnp_csn;
		start_undi.DX = undi->isapnp_read_port;
		start_undi.ES = BIOS_SEG;
		start_undi.DI = find_pnp_bios();
		if ( ( rc = undinet_call ( undinic, PXENV_START_UNDI,
					   &start_undi,
					   sizeof ( start_undi ) ) ) != 0 )
			goto err_start_undi;
	}
	undi->flags |= UNDI_FL_STARTED;

	/* Bring up UNDI stack */
	if ( ! ( undi->flags & UNDI_FL_INITIALIZED ) ) {
		memset ( &undi_startup, 0, sizeof ( undi_startup ) );
		if ( ( rc = undinet_call ( undinic, PXENV_UNDI_STARTUP,
					   &undi_startup,
					   sizeof ( undi_startup ) ) ) != 0 )
			goto err_undi_startup;
		/* On some PXE stacks, PXENV_UNDI_INITIALIZE may fail
		 * due to a transient condition (e.g. media test
		 * failing because the link has only just come out of
		 * reset).  We may therefore need to retry this call
		 * several times.
		 */
		for ( retry = 0 ; ; ) {
			memset ( &undi_init, 0, sizeof ( undi_init ) );
			if ( ( rc = undinet_call ( undinic,
						   PXENV_UNDI_INITIALIZE,
						   &undi_init,
						   sizeof ( undi_init ) ) ) ==0)
				break;
			if ( ++retry > UNDI_INITIALIZE_RETRY_MAX )
				goto err_undi_initialize;
			DBGC ( undinic, "UNDINIC %p retrying "
			       "PXENV_UNDI_INITIALIZE (retry %d)\n",
			       undinic, retry );
			/* Delay to allow link to settle if necessary */
			mdelay ( UNDI_INITIALIZE_RETRY_DELAY_MS );
		}
	}
	undi->flags |= UNDI_FL_INITIALIZED;

	/* Get device information */
	memset ( &undi_info, 0, sizeof ( undi_info ) );
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_GET_INFORMATION,
				   &undi_info, sizeof ( undi_info ) ) ) != 0 )
		goto err_undi_get_information;
	memcpy ( netdev->hw_addr, undi_info.PermNodeAddress, ETH_ALEN );
	memcpy ( netdev->ll_addr, undi_info.CurrentNodeAddress, ETH_ALEN );
	undinic->irq = undi_info.IntNumber;
	if ( undinic->irq > IRQ_MAX ) {
		DBGC ( undinic, "UNDINIC %p ignoring invalid IRQ %d\n",
		       undinic, undinic->irq );
		undinic->irq = 0;
	}
	DBGC ( undinic, "UNDINIC %p has MAC address %s and IRQ %d\n",
	       undinic, eth_ntoa ( netdev->hw_addr ), undinic->irq );
	if ( undinic->irq ) {
		/* Sanity check - prefix should have disabled the IRQ */
		assert ( ! irq_enabled ( undinic->irq ) );
	}

	/* Get interface information */
	memset ( &undi_iface, 0, sizeof ( undi_iface ) );
	if ( ( rc = undinet_call ( undinic, PXENV_UNDI_GET_IFACE_INFO,
				   &undi_iface, sizeof ( undi_iface ) ) ) != 0 )
		goto err_undi_get_iface_info;
	DBGC ( undinic, "UNDINIC %p has type %s, speed %d, flags %08x\n",
	       undinic, undi_iface.IfaceType, undi_iface.LinkSpeed,
	       undi_iface.ServiceFlags );
	if ( ( undi_iface.ServiceFlags & SUPPORTED_IRQ ) &&
	     ( undinic->irq != 0 ) ) {
		undinic->irq_supported = 1;
	}
	DBGC ( undinic, "UNDINIC %p using %s mode\n", undinic,
	       ( undinic->irq_supported ? "interrupt" : "polling" ) );
	if ( strncmp ( ( ( char * ) undi_iface.IfaceType ), "Etherboot",
		       sizeof ( undi_iface.IfaceType ) ) == 0 ) {
		DBGC ( undinic, "UNDINIC %p Etherboot 5.4 workaround enabled\n",
		       undinic );
		undinic->hacks |= UNDI_HACK_EB54;
	}
	if ( undinet_irq_is_broken ( &dev->desc ) ) {
		DBGC ( undinic, "UNDINIC %p forcing polling mode due to "
		       "broken interrupts\n", undinic );
		undinic->irq_supported = 0;
	}

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register;

	/* Mark as link up; we don't handle link state */
	netdev_link_up ( netdev );

	DBGC ( undinic, "UNDINIC %p added\n", undinic );
	return 0;

 err_register:
 err_undi_get_iface_info:
 err_undi_get_information:
 err_undi_initialize:
	/* Shut down UNDI stack */
	memset ( &undi_shutdown, 0, sizeof ( undi_shutdown ) );
	undinet_call ( undinic, PXENV_UNDI_SHUTDOWN, &undi_shutdown,
		       sizeof ( undi_shutdown ) );
	memset ( &undi_cleanup, 0, sizeof ( undi_cleanup ) );
	undinet_call ( undinic, PXENV_UNDI_CLEANUP, &undi_cleanup,
		       sizeof ( undi_cleanup ) );
	undi->flags &= ~UNDI_FL_INITIALIZED;
 err_undi_startup:
	/* Unhook UNDI stack */
	memset ( &stop_undi, 0, sizeof ( stop_undi ) );
	undinet_call ( undinic, PXENV_STOP_UNDI, &stop_undi,
		       sizeof ( stop_undi ) );
	undi->flags &= ~UNDI_FL_STARTED;
 err_start_undi:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
	undi_set_drvdata ( undi, NULL );
	return rc;
}

/**
 * Remove UNDI device
 *
 * @v undi		UNDI device
 */
void undinet_remove ( struct undi_device *undi ) {
	struct net_device *netdev = undi_get_drvdata ( undi );
	struct undi_nic *undinic = netdev->priv;
	struct s_PXENV_UNDI_SHUTDOWN undi_shutdown;
	struct s_PXENV_UNDI_CLEANUP undi_cleanup;
	struct s_PXENV_STOP_UNDI stop_undi;

	/* Unregister net device */
	unregister_netdev ( netdev );

	/* If we are preparing for an OS boot, or if we cannot exit
	 * via the PXE stack, then shut down the PXE stack.
	 */
	if ( ! ( undi->flags & UNDI_FL_KEEP_ALL ) ) {

		/* Shut down UNDI stack */
		memset ( &undi_shutdown, 0, sizeof ( undi_shutdown ) );
		undinet_call ( undinic, PXENV_UNDI_SHUTDOWN,
			       &undi_shutdown, sizeof ( undi_shutdown ) );
		memset ( &undi_cleanup, 0, sizeof ( undi_cleanup ) );
		undinet_call ( undinic, PXENV_UNDI_CLEANUP,
			       &undi_cleanup, sizeof ( undi_cleanup ) );
		undi->flags &= ~UNDI_FL_INITIALIZED;

		/* Unhook UNDI stack */
		memset ( &stop_undi, 0, sizeof ( stop_undi ) );
		undinet_call ( undinic, PXENV_STOP_UNDI, &stop_undi,
			       sizeof ( stop_undi ) );
		undi->flags &= ~UNDI_FL_STARTED;
	}

	/* Clear entry point */
	memset ( &undinet_entry_point, 0, sizeof ( undinet_entry_point ) );

	/* Free network device */
	netdev_nullify ( netdev );
	netdev_put ( netdev );

	DBGC ( undinic, "UNDINIC %p removed\n", undinic );
}
