#ifndef PXE_H
#define PXE_H

#include "pxe_types.h"
#include "pxe_api.h"

/* Union used for PXE API calls; we don't know the type of the
 * structure until we interpret the opcode.  Also, Status is available
 * in the same location for any opcode, and it's convenient to have
 * non-specific access to it.
 */
union u_PXENV_ANY {
	/* Make it easy to read status for any operation */
	PXENV_STATUS_t				Status;
	struct s_PXENV_UNLOAD_STACK		unload_stack;
	struct s_PXENV_GET_CACHED_INFO		get_cached_info;
	struct s_PXENV_RESTART_TFTP		restart_tftp;
	struct s_PXENV_START_UNDI		start_undi;
	struct s_PXENV_STOP_UNDI		stop_undi;
	struct s_PXENV_START_BASE		start_base;
	struct s_PXENV_STOP_BASE		stop_base;
	struct s_PXENV_TFTP_OPEN		tftp_open;
	struct s_PXENV_TFTP_CLOSE		tftp_close;
	struct s_PXENV_TFTP_READ		tftp_read;
	struct s_PXENV_TFTP_READ_FILE		tftp_read_file;
	struct s_PXENV_TFTP_GET_FSIZE		tftp_get_fsize;
	struct s_PXENV_UDP_OPEN			udp_open;
	struct s_PXENV_UDP_CLOSE		udp_close;
	struct s_PXENV_UDP_WRITE		udp_write;
	struct s_PXENV_UDP_READ			udp_read;
	struct s_PXENV_UNDI_STARTUP		undi_startup;
	struct s_PXENV_UNDI_CLEANUP		undi_cleanup;
	struct s_PXENV_UNDI_INITIALIZE		undi_initialize;
	struct s_PXENV_UNDI_RESET_ADAPTER	undi_reset_adapter;
	struct s_PXENV_UNDI_SHUTDOWN		undi_shutdown;
	struct s_PXENV_UNDI_OPEN		undi_open;
	struct s_PXENV_UNDI_CLOSE		undi_close;
	struct s_PXENV_UNDI_TRANSMIT		undi_transmit;
	struct s_PXENV_UNDI_SET_MCAST_ADDRESS	undi_set_mcast_address;
	struct s_PXENV_UNDI_SET_STATION_ADDRESS undi_set_station_address;
	struct s_PXENV_UNDI_SET_PACKET_FILTER	undi_set_packet_filter;
	struct s_PXENV_UNDI_GET_INFORMATION	undi_get_information;
	struct s_PXENV_UNDI_GET_STATISTICS	undi_get_statistics;
	struct s_PXENV_UNDI_CLEAR_STATISTICS	undi_clear_statistics;
	struct s_PXENV_UNDI_INITIATE_DIAGS	undi_initiate_diags;
	struct s_PXENV_UNDI_FORCE_INTERRUPT	undi_force_interrupt;
	struct s_PXENV_UNDI_GET_MCAST_ADDRESS	undi_get_mcast_address;
	struct s_PXENV_UNDI_GET_NIC_TYPE	undi_get_nic_type;
	struct s_PXENV_UNDI_GET_IFACE_INFO	undi_get_iface_info;
	struct s_PXENV_UNDI_GET_STATE		undi_get_state;
	struct s_PXENV_UNDI_ISR			undi_isr;
};

typedef union u_PXENV_ANY PXENV_ANY_t;

/* PXE stack status indicator.  See pxe_export.c for further
 * explanation.
 */
typedef enum {
	CAN_UNLOAD = 0,
	MIDWAY,
	READY
} pxe_stack_state_t;

/* Data structures installed as part of a PXE stack.  Architectures
 * will have extra information to append to the end of this.
 */
#define PXE_TFTP_MAGIC_COOKIE ( ( 'P'<<24 ) | ( 'x'<<16 ) | ( 'T'<<8 ) | 'f' )
typedef struct {
	pxe_t		pxe	__attribute__ ((aligned(16)));
	pxenv_t		pxenv	__attribute__ ((aligned(16)));
	pxe_stack_state_t state;
	union {
		BOOTPLAYER	cached_info;
		char		packet[ETH_FRAME_LEN];
		struct {
			uint32_t magic_cookie;
			unsigned int len;
			int eof;
			char data[TFTP_MAX_PACKET];
		} tftpdata;
		struct {
			char *buffer;
			uint32_t offset;
			uint32_t bufferlen;
		} readfile;
	};
	struct {}	arch_data __attribute__ ((aligned(16)));
} pxe_stack_t;

extern PXENV_EXIT_t pxe_api_call ( int opcode, union u_PXENV_ANY *any );

#endif /* PXE_H */
