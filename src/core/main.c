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
#include "http.h"
#include "timer.h"
#include "cpu.h"
#include <stdarg.h>

#ifdef CONSOLE_BTEXT
#include "btext.h"
#endif

#ifdef CONFIG_FILO
#include <lib.h>
#endif

jmp_buf	restart_etherboot;
int	url_port;		

char as_main_program = 1;

#ifdef	IMAGE_FREEBSD
int freebsd_howto = 0;
char freebsd_kernel_env[FREEBSD_KERNEL_ENV_SIZE];
#endif

/* in_call(): the entry point to Etherboot.  Generally called from
 * arch_in_call(), which in turn will have been invoked from
 * platform-specific assembly code.
 */
int in_call ( in_call_data_t *data, uint32_t opcode, va_list params ) {
	int old_as_main_program = as_main_program;
	int ret = 0;

	/* Set flat to indicate that we are not running as the main
	 * program (i.e. we are something like a PXE stack).
	 */
	as_main_program = 0;

	/* NOTE: params will cease to be valid if we relocate, since
	 * it represents a virtual address
	 */
	switch ( EB_OPCODE(opcode) ) {
		
	case EB_OPCODE_CHECK:
		/* Installation check
		 */
		ret = EB_CHECK_RESULT;
		break;
	case EB_OPCODE_MAIN:
		/* Start up Etherboot as a standalone program. */
		as_main_program = 1;
		ret = main ( data, params );
		break;
#ifdef PXE_EXPORT
	case EB_OPCODE_PXE:
		/* !PXE API call */
		ret = pxe_in_call ( data, params );
		break;
#endif
	default:
		printf ( "Unsupported API \"%c%c\"\n",
			 EB_OPCODE(opcode) >> 8, EB_OPCODE(opcode) & 0xff );
		ret = -1;
		break;
	}

	as_main_program = old_as_main_program;
	return ret;
}

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

void console_init(void)
{
#ifdef	CONSOLE_SERIAL
	(void)serial_init();
#endif
#ifdef 	CONSOLE_DIRECT_VGA
       	video_init();
#endif
#ifdef	CONSOLE_BTEXT
	map_boot_text();
#endif
}

static void console_fini(void)
{
#ifdef	CONSOLE_SERIAL
	(void)serial_fini();
#endif
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


static int main_loop(int state);
static int exit_ok;
static int exit_status;
static int initialized;

/**************************************************************************
MAIN - Kick off routine
**************************************************************************/
int main(in_call_data_t *data, va_list params)
{
	char *p;
	int state;

	for (p = _bss; p < _ebss; p++)
		*p = 0;	/* Zero BSS */

	console_init();
	arch_main(data,params);

#if 0
#ifdef  CONSOLE_BTEXT
        btext_init();
        map_boot_text();
        btext_clearscreen();
#endif
#endif 

	if ( rom.rom_segment ) {
		printf ( "ROM segment %#hx length %#hx reloc %#x\n",
			 rom.rom_segment, rom.rom_length, _text );
	}

	cpu_setup();
	setup_timers();
	gateA20_set();
	print_config();
	get_memsizes();
	cleanup();

#ifdef CONFIG_PCMCIA
	pcmcia_init_all();
#endif

	/* -1:	timeout or ESC
	   -2:	error return from loader
	   -3:  finish the current run.
	   0:	retry booting bootp and tftp
	   1:   retry tftp with possibly modified bootp reply
	   2:   retry bootp and tftp
	   3:   retry probe bootp and tftp
	   4:   start with the next device and retry from there...
	   255: exit Etherboot
	   256: retry after relocation
	*/
	state = setjmp(restart_etherboot);
	exit_ok = 1;
	for(;state != 255;) {
		state = main_loop(state);
	}
	arch_on_exit(exit_status);
#ifdef CONFIG_PCMCIA
	pcmcia_shutdown_all();
#endif
	return exit_status;
}

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
	static void *heap_base;
	static int type;
	static int i;

	if (!initialized) {
		initialized = 1;
		console_init();
		if (dev && (state >= 1) && (state <= 2)) {
			dev->how_probe = PROBE_AWAKE;
			dev->how_probe = ops->probe(dev);
			if (dev->how_probe == PROBE_FAILED) {
				state = -1;
			}
		}
	}
	switch(state) {
	case 0:
	{
		static int firsttime = 1;
		/* First time through */
		if (firsttime) {
			relocate();
			cleanup();
			console_init();
			init_heap();
#ifdef  CONSOLE_BTEXT
			//I need to all allot
		        btext_init(); 
		        map_boot_text();
		        btext_clearscreen();
#else
	#ifdef CONFIG_FILO
			pci_init();
	#endif
#endif

			firsttime = 0;
		} 
#ifdef	EXIT_IF_NO_OFFER
		else {
			cleanup();
			exit(0);
		}
#endif
		heap_base = allot(0);
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
		console_init();
		forget(heap_base);
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
		heap_base = allot(0);
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


/**************************************************************************
LOADKERNEL - Try to load kernel image
**************************************************************************/
struct proto {
	char *name;
	int (*load)(const char *name,
		int (*fnc)(unsigned char *, unsigned int, unsigned int, int));
};
static const struct proto protos[] = {
#ifdef DOWNLOAD_PROTO_TFTM
	{ "x-tftm", url_tftm },
#endif
#ifdef DOWNLOAD_PROTO_SLAM
	{ "x-slam", url_slam },
#endif
#ifdef DOWNLOAD_PROTO_NFS
	{ "nfs", nfs },
#endif
#ifdef DOWNLOAD_PROTO_DISK
	{ "file", url_file },
#endif
#ifdef DOWNLOAD_PROTO_TFTP
	{ "tftp", tftp },
#endif
#ifdef DOWNLOAD_PROTO_HTTP
        { "http", http },
#endif
};

int loadkernel(const char *fname)
{
	static const struct proto * const last_proto = 
		&protos[sizeof(protos)/sizeof(protos[0])];
	const struct proto *proto;
	in_addr ip;
	int len;
	const char *name;
#ifdef	DNS_RESOLVER
	const char *resolvt;
#endif
	ip.s_addr = arptable[ARP_SERVER].ipaddr.s_addr;
	name = fname;
	url_port = -1;
	len = 0;
	while(fname[len] && fname[len] != ':') {
		len++;
	}
	for(proto = &protos[0]; proto < last_proto; proto++) {
		if (memcmp(name, proto->name, len) == 0) {
			break;
		}
	}
	if ((proto < last_proto) && (memcmp(fname + len, "://", 3) == 0)) {
		name += len + 3;
		if (name[0] != '/') {
#ifdef DNS_RESOLVER
			resolvt = dns_resolver ( name );
			if ( NULL != resolvt ) {
				//printf ("Resolved host name [%s] to [%s]\n",
				//	name, resolvt );
				inet_aton(resolvt, &ip);
				while ( ( '/' != name[0] ) && ( 0 != name[0]))
					++name;
			} else
#endif	/* DNS_RESOLVER */
			name += inet_aton(name, &ip);
			if (name[0] == ':') {
				name++;
				url_port = strtoul(name, &name, 10);
			}
		}
		if (name[0] == '/') {
			arptable[ARP_SERVER].ipaddr.s_addr = ip.s_addr;
			printf( "Loading %s ", fname );
			return proto->load(name + 1, load_block);
		}
	}
	printf("Loading %@:%s ", arptable[ARP_SERVER].ipaddr, fname);
#ifdef	DEFAULT_PROTO_NFS
	return nfs(fname, load_block);
#else
	return tftp(fname, load_block);
#endif
}


/**************************************************************************
CLEANUP - shut down networking and console so that the OS may be called 
**************************************************************************/
void cleanup(void)
{
#ifdef	DOWNLOAD_PROTO_NFS
	nfs_umountall(ARP_SERVER);
#endif
	/* Stop receiving packets */
	eth_disable();
	disk_disable();
	console_fini();
	initialized = 0;
}

/*
 * Local variables:
 *  c-basic-offset: 8
 * End:
 */
