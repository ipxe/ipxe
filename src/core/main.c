/**************************************************************************
Etherboot -  Network Bootstrap Program

Literature dealing with the network protocols:
	ARP - RFC826
	RARP - RFC903
	UDP - RFC768
	BOOTP - RFC951, RFC2132 (vendor extensions)
	DHCP - RFC2131, RFC2132 (options)
	TFTP - RFC1350, RFC2347 (options), RFC2348 (blocksize), RFC2349 (tsize)
	RPC - RFC1831, RFC1832 (XDR), RFC1833 (rpcbind/portmapper)
	NFS - RFC1094, RFC1813 (v3, useful for clarifications, not implemented)
	IGMP - RFC1112

**************************************************************************/

/* #define MDEBUG */

#include "etherboot.h"
#include "dev.h"
#include "nic.h"
#include "disk.h"
#include "timer.h"
#include "cpu.h"
#include "console.h"
#include <gpxe/init.h>
#include "image.h"
#include <stdarg.h>

#include <gpxe/device.h>
#include <gpxe/heap.h>
#include <gpxe/netdevice.h>

/* Linker symbols */
extern char _bss[], _ebss[];

jmp_buf	restart_etherboot;
int	url_port;		

char as_main_program = 1;

#if 0

static inline unsigned long ask_boot(unsigned *index)
{
	unsigned long order = DEFAULT_BOOT_ORDER;
	*index = DEFAULT_BOOT_INDEX;
#ifdef LINUXBIOS
	order = get_boot_order(order, index);
#endif
#if defined(ASK_BOOT)
#if ASK_BOOT >= 0
	while(1) {
		int c = 0;
		printf(ASK_PROMPT);
#if ASK_BOOT > 0
		{
			unsigned long time;
			for ( time = currticks() + ASK_BOOT*TICKS_PER_SEC;
			      !c && !iskey(); ) {
				if (currticks() > time) c = ANS_DEFAULT;
			}
		}
#endif /* ASK_BOOT > 0 */
		if ( !c ) c = getchar();
		if ((c >= 'a') && (c <= 'z')) c &= 0x5F;
		if ((c >= ' ') && (c <= '~')) putchar(c);
		putchar('\n');

		switch(c) {
		default:
			/* Nothing useful try again */
			continue;
		case ANS_QUIT:
			order = BOOT_NOTHING;
			*index = 0;
			break;
		case ANS_DEFAULT:
			/* Preserve the default boot order */
			break;
		case ANS_NETWORK:
			order = (BOOT_NIC     << (0*BOOT_BITS)) | 
				(BOOT_NOTHING << (1*BOOT_BITS));
			*index = 0;
			break;
		case ANS_DISK:
			order = (BOOT_DISK    << (0*BOOT_BITS)) | 
				(BOOT_NOTHING << (1*BOOT_BITS));
			*index = 0;
			break;
		case ANS_FLOPPY:
			order = (BOOT_FLOPPY  << (0*BOOT_BITS)) | 
				(BOOT_NOTHING << (1*BOOT_BITS));
			*index = 0;
			break;
		}
		break;
	}
	putchar('\n');
#endif /* ASK_BOOT >= 0 */
#endif /* defined(ASK_BOOT) */
	return order;
}

static inline void try_floppy_first(void)
{
#if (TRY_FLOPPY_FIRST > 0)
	int i;
	printf("Trying floppy");
	disk_init();
	for (i = TRY_FLOPPY_FIRST; i-- > 0; ) {
		putchar('.');
		if (pcbios_disk_read(0, 0, 0, 0, ((char *)FLOPPY_BOOT_LOCATION)) != 0x8000) {
			printf("using floppy\n");
			exit(0);
		}
	}
	printf("no floppy\n");
#endif /* TRY_FLOPPY_FIRST */	
}

static struct class_operations {
	struct dev *dev;
	int (*probe)(struct dev *dev);
	int (*load_configuration)(struct dev *dev);
	int (*load)(struct dev *dev);
}
operations[] = {
	{ &nic.dev,  eth_probe,  eth_load_configuration,  eth_load  },
	{ &disk.dev, disk_probe, disk_load_configuration, disk_load },
	{ &disk.dev, disk_probe, disk_load_configuration, disk_load },
};

#endif



static int main_loop(int state);
static int exit_ok;
static int exit_status;
static int initialized;


/**************************************************************************
MAIN - Kick off routine
**************************************************************************/
int main ( void ) {
	struct net_device *netdev;

	/* Call all registered initialisation functions */
	init_heap();
	call_init_fns ();
	probe_devices();

	netdev = next_netdev ();
	if ( netdev ) {
		test_dhcp ( netdev );
	} else {
		printf ( "No network device found\n" );
	}

	printf ( "Press any key to exit\n" );
	getchar();

	remove_devices();
	call_exit_fns ();

	return exit_status;
}

#if 0

void exit(int status)
{
	while(!exit_ok)
		;
	exit_status = status;
	longjmp(restart_etherboot, 255);
}



static int main_loop(int state)
{
	/* Splitting main into 2 pieces makes the semantics of 
	 * which variables are preserved across a longjmp clean
	 * and predictable.
	 */
	static unsigned long order;
	static unsigned boot_index;
	static struct dev * dev = 0;
	static struct class_operations *ops;
	static int type;
	static int i;

	if (!initialized) {
		initialized = 1;
		if (dev && (state >= 1) && (state <= 2)) {
			dev->how_probe = PROBE_AWAKE;
			dev->how_probe = ops->probe(dev);
			if (dev->how_probe == PROBE_FAILED) {
				state = -1;
			}
			if (state == 1) {
				/* The bootp reply might have been changed, re-parse.  */
				decode_rfc1533(bootp_data.bootp_reply.bp_vend, 0,
#ifdef	NO_DHCP_SUPPORT
				               BOOTP_VENDOR_LEN + MAX_BOOTP_EXTLEN, 
#else
				               DHCP_OPT_LEN + MAX_BOOTP_EXTLEN, 
#endif	/* NO_DHCP_SUPPORT */
				               1);
			}
		}
	}
	switch(state) {
	case 0:
	{
		static int firsttime = 1;
		/* First time through */
		if (firsttime) {
			cleanup();
			firsttime = 0;
		} 
#ifdef	EXIT_IF_NO_OFFER
		else {
			cleanup();
			exit(0);
		}
#endif
		i = -1;
		state = 4;
		dev = 0;

		/* We just called setjmp ... */
		order = ask_boot(&boot_index);
		try_floppy_first();
		break;
	}
	case 4:
		cleanup();
		call_reset_fns();
		/* Find a dev entry to probe with */
		if (!dev) {
			int boot;
			int failsafe;

			/* Advance to the next device type */
			i++;
			boot = (order >> (i * BOOT_BITS)) & BOOT_MASK;
			type = boot & BOOT_TYPE_MASK;
			failsafe = (boot & BOOT_FAILSAFE) != 0;
			if (i >= MAX_BOOT_ENTRIES) {
				type = BOOT_NOTHING;
			}
			if ((i == 0) && (type == BOOT_NOTHING)) {
				/* Return to caller */
				exit(0);
			}
			if (type >= BOOT_NOTHING) {
				interruptible_sleep(2);
				state = 0;
				break;
			}
			ops = &operations[type];
			dev = ops->dev;
			dev->how_probe = PROBE_FIRST;
			dev->type = type;
			dev->failsafe = failsafe;
			dev->type_index = 0;
		} else {
			/* Advance to the next device of the same type */
			dev->how_probe = PROBE_NEXT;
		}
		state = 3;
		break;
	case 3:
		state = -1;
		/* Removed the following line because it was causing
		 * heap.o to be dragged in unnecessarily.  It's also
		 * slightly puzzling: by resetting heap_base, doesn't
		 * this mean that we permanently leak memory?
		 */
		/* heap_base = allot(0); */
		dev->how_probe = ops->probe(dev);
		if (dev->how_probe == PROBE_FAILED) {
			dev = 0;
			state = 4;
		} else if (boot_index && (i == 0) && (boot_index != (unsigned)dev->type_index)) {
			printf("Wrong index\n");
			state = 4;
		}
		else {
			state = 2;
		}
		break;
	case 2:
		state = -1;
		if (ops->load_configuration(dev) >= 0) {
			state = 1;
		}
		break;
	case 1:
		/* Any return from load is a failure */
		ops->load(dev);
		state = -1;
		break;
	case 256:
		state = 0;
		break;
	case -3:
		i = MAX_BOOT_ENTRIES;
		type = BOOT_NOTHING;
		/* fall through */
	default:
		printf("<abort>\n");
		state = 4;
		/* At the end goto state 0 */
		if ((type >= BOOT_NOTHING) || (i >= MAX_BOOT_ENTRIES)) {
			state = 0;
		}
		break;
	}
	return state;
}


#endif


/**************************************************************************
LOADKERNEL - Try to load kernel image
**************************************************************************/
#if 0
/* To be split out into individual files */
static const struct proto protos[] = {
	{ "x-tftm", url_tftm },
	{ "x-slam", url_slam },
	{ "nfs", nfs },
	{ "file", url_file },
	{ "tftp", tftp },
        { "http", http },
};
#endif


/**************************************************************************
CLEANUP - shut down networking and console so that the OS may be called 
**************************************************************************/
void cleanup(void)
{
	/* Stop receiving packets */
	disable ( &dev );
	initialized = 0;
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
