/* Header for pxe_export.c
 */

#ifndef PXE_EXPORT_H
#define PXE_EXPORT_H

#include "pxe.h"

/* Function prototypes */
extern int ensure_pxe_state ( pxe_stack_state_t wanted );

extern PXENV_EXIT_t pxenv_start_undi ( t_PXENV_START_UNDI * );
extern PXENV_EXIT_t pxenv_undi_startup ( t_PXENV_UNDI_STARTUP * );
extern PXENV_EXIT_t pxenv_undi_cleanup ( t_PXENV_UNDI_CLEANUP * );
extern PXENV_EXIT_t pxenv_undi_initialize ( t_PXENV_UNDI_INITIALIZE * );
extern PXENV_EXIT_t pxenv_undi_reset_adapter ( t_PXENV_UNDI_RESET_ADAPTER * );
extern PXENV_EXIT_t pxenv_undi_shutdown ( t_PXENV_UNDI_SHUTDOWN * );
extern PXENV_EXIT_t pxenv_undi_open ( t_PXENV_UNDI_OPEN * );
extern PXENV_EXIT_t pxenv_undi_close ( t_PXENV_UNDI_CLOSE * );
extern PXENV_EXIT_t pxenv_undi_transmit ( t_PXENV_UNDI_TRANSMIT * );
extern PXENV_EXIT_t pxenv_undi_set_mcast_address (
					    t_PXENV_UNDI_SET_MCAST_ADDRESS * );
extern PXENV_EXIT_t pxenv_undi_set_station_address (
					  t_PXENV_UNDI_SET_STATION_ADDRESS * );
extern PXENV_EXIT_t pxenv_undi_set_packet_filter (
					    t_PXENV_UNDI_SET_PACKET_FILTER * );
extern PXENV_EXIT_t pxenv_undi_get_information (
					      t_PXENV_UNDI_GET_INFORMATION * );
extern PXENV_EXIT_t pxenv_undi_get_statistics ( t_PXENV_UNDI_GET_STATISTICS* );
extern PXENV_EXIT_t pxenv_undi_clear_statistics (
					     t_PXENV_UNDI_CLEAR_STATISTICS * );
extern PXENV_EXIT_t pxenv_undi_initiate_diags ( t_PXENV_UNDI_INITIATE_DIAGS* );
extern PXENV_EXIT_t pxenv_undi_force_interrupt (
					      t_PXENV_UNDI_FORCE_INTERRUPT * );
extern PXENV_EXIT_t pxenv_undi_get_mcast_address (
					    t_PXENV_UNDI_GET_MCAST_ADDRESS * );
extern PXENV_EXIT_t pxenv_undi_get_nic_type ( t_PXENV_UNDI_GET_NIC_TYPE * );
extern PXENV_EXIT_t pxenv_undi_get_iface_info ( t_PXENV_UNDI_GET_IFACE_INFO *);
extern PXENV_EXIT_t pxenv_undi_isr ( t_PXENV_UNDI_ISR * );
extern PXENV_EXIT_t pxenv_stop_undi ( t_PXENV_STOP_UNDI * );
extern PXENV_EXIT_t pxenv_tftp_open ( t_PXENV_TFTP_OPEN * );
extern PXENV_EXIT_t pxenv_tftp_close ( t_PXENV_TFTP_CLOSE * );
extern PXENV_EXIT_t pxenv_tftp_read ( t_PXENV_TFTP_READ * );
extern PXENV_EXIT_t pxenv_tftp_read_file ( t_PXENV_TFTP_READ_FILE * );
extern PXENV_EXIT_t pxenv_tftp_get_fsize ( t_PXENV_TFTP_GET_FSIZE * );
extern PXENV_EXIT_t pxenv_udp_open ( t_PXENV_UDP_OPEN * );
extern PXENV_EXIT_t pxenv_udp_close ( t_PXENV_UDP_CLOSE * );
extern PXENV_EXIT_t pxenv_udp_read ( t_PXENV_UDP_READ * );
extern PXENV_EXIT_t pxenv_udp_write ( t_PXENV_UDP_WRITE * );
extern PXENV_EXIT_t pxenv_unload_stack ( t_PXENV_UNLOAD_STACK * );
extern PXENV_EXIT_t pxenv_get_cached_info ( t_PXENV_GET_CACHED_INFO * );
extern PXENV_EXIT_t pxenv_restart_tftp ( t_PXENV_RESTART_TFTP * );
extern PXENV_EXIT_t pxenv_start_base ( t_PXENV_START_BASE * );
extern PXENV_EXIT_t pxenv_stop_base ( t_PXENV_STOP_BASE * );

extern PXENV_EXIT_t pxe_api_call ( int opcode, t_PXENV_ANY *params );

/* Static variables */
extern pxe_stack_t *pxe_stack;

#endif /* PXE_EXPORT_H */
