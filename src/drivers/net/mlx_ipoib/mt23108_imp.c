typedef uint32_t __u32;
typedef uint16_t __u16;
typedef uint8_t __u8;

static int verbose_messages=0;
static int print_info=0;
static int fatal_condition=0;
static int fw_fatal;

#define tprintf(fmt, a...) \
		 do {    \
			if ( verbose_messages ) { \
				printf("%s:%d: " fmt "\n", __func__, __LINE__,  ##a); \
			} \
		 } \
		 while(0)

#define eprintf(fmt, a...) \
		 printf("%s:%d: " fmt "\n", __func__, __LINE__,  ##a)

static void cpu_to_be_buf(void *buf, int size)
{
	int dw_sz = size >> 2, i;

	for (i = 0; i < dw_sz; ++i) {
		((__u32 *) buf)[i] = cpu_to_be32(((__u32 *) buf)[i]);
	}
}

static void be_to_cpu_buf(void *buf, int size)
{
	int dw_sz = size >> 2, i;
	u32 *p = buf;

	for (i = 0; i < dw_sz; ++i) {
		p[i] = be32_to_cpu(p[i]);
	}
}

#include "timer.h"
#include "cmdif_mt23108.c"
#include "cmdif_comm.c"
#include "ib_mt23108.c"
#include "ib_mad.c"
#include "ib_driver.c"
#include "ipoib.c"

static int probe_imp(struct pci_device *pci, struct nic *nic)
{
	int rc;

	if (0 && nic) {		/* just to supress warning */
		return 0;
	}

	fatal_condition= 0;
	fw_fatal= 0;

	tprintf("");
	rc = ipoib_init(pci);
	if (rc)
		return rc;

	tprintf("");

	return rc;
}

static int disable_imp(void)
{
	int rc;

	rc = ipoib_close(fw_fatal);

	return rc;
}

static int transmit_imp(const char *dest,	/* Destination */
			unsigned int type,	/* Type */
			const char *packet,	/* Packet */
			unsigned int size)
{				/* size */
	int rc;

	if (fatal_condition) {
		/* since the transmit function does not return a value
		   we return success but do nothing to suppress error messages */
		return 0;
	}

	rc = ipoib_send_packet(dest, type, packet, size);
	if (rc) {
		printf("*** ERROR IN SEND FLOW ***\n");
		printf("restarting Etherboot\n");
		sleep(1);
		longjmp(restart_etherboot, -1);
		/* we should not be here ... */
		return -1; 
	}

	return rc;
}

static void hd(void *where, int n)
{
	int i;

	while (n > 0) {
		printf("%X ", where);
		for (i = 0; i < ((n > 16) ? 16 : n); i++)
			printf(" %hhX", ((char *)where)[i]);
		printf("\n");
		n -= 16;
		where += 16;
	}
}

static int poll_imp(struct nic *nic, int retrieve, unsigned int *size_p)
{
	static char packet[2048];
	static char *last_packet_p = NULL;
	static unsigned long last_packet_size;
	char *packet_p;
	const int eth_header_len = 14;
	unsigned int packet_len;
	int is_bcast = 0;
	__u16 prot, *ptr;
	int rc;

	if (0 && nic) {		/* just to supress warning */
		return -1;
	}

	if (fatal_condition) {
		*size_p = 0;
		return 0;
	}

	if (poll_error_buf()) {
		fatal_condition= 1;
		fw_fatal= 1;
		printf("\n *** DEVICE FATAL ERROR ***\n");
		goto fatal_handling;
	}
	else if (drain_eq()) {
		fatal_condition= 1;
		printf("\n *** FATAL ERROR ***\n");
		goto fatal_handling;
	}


	if (retrieve) {
		/* we actually want to read the packet */
		if (last_packet_p) {
			eprintf("");
			/* there is already a packet that was previously read */
			memcpy(nic->packet, last_packet_p, last_packet_size);
			*size_p = last_packet_size;
			last_packet_p = NULL;
			return 0;
		}
		packet_p = nic->packet;
	} else {
		/* we don't want to read the packet,
		   just know if there is one. so we
		   read the packet to a local buffer and
		   we will return that buffer when the ip layer wants
		   another packet */
		if (last_packet_p) {
			/* there is already a packet that
			   was not consumend */
			eprintf("overflow receive packets");
			return -1;
		}
		packet_p = packet;
	}

	rc = ipoib_read_packet(&prot, packet_p + eth_header_len, &packet_len,
			       &is_bcast);
	if (rc) {
		printf("*** FATAL IN RECEIVE FLOW ****\n");
		goto fatal_handling;
	}

	if (packet_len == 0) {
		*size_p = 0;
		return 0;
	}

	if (is_bcast) {
		int i;
		for (i = 0; i < 6; ++i) {
			packet_p[i] = 0xff;
		}
	} else {
		packet_p[0] = MLX_ETH_BYTE0;
		packet_p[1] = MLX_ETH_BYTE1;
		packet_p[2] = MLX_ETH_BYTE2;
		packet_p[3] = 0;
		packet_p[4] = 0;
		packet_p[5] = 0;
	}

	memset(packet_p + 6, 0, 6);

	ptr = (__u16 *) (packet_p + 12);
	*ptr = htons(prot);

	if (!retrieve) {
		last_packet_p = packet;
		last_packet_size = packet_len + eth_header_len;
		*size_p = 0;
	}

	*size_p = packet_len + eth_header_len;
	tprintf("packet size=%d, prot=%x\n", *size_p, prot);
	if (0) {
		hd(nic->packet, 42);
	}

	return 0;

fatal_handling:
	printf("restarting Etherboot\n");
	sleep(1);
	longjmp(restart_etherboot, -1);
	/* we should not be here ... */
	return -1; 
	
}
