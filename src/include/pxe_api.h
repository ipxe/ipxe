#ifndef PXE_API_H
#define PXE_API_H

/** @addtogroup pxe Preboot eXecution Environment (PXE) API
 * @{
 */

/** @file
 *
 * Preboot eXecution Environment (PXE) API
 *
 */

#include "pxe_types.h"

/** @defgroup pxe_code PXE API function codes
 * @{
 */

/** START UNDI (call function pxenv_start_undi())*/
#define	PXENV_START_UNDI		0x0000
/** UNDI STARTUP (call function pxenv_undi_startup()) */ 
#define	PXENV_UNDI_STARTUP		0x0001
/** UNDI CLEANUP (call function pxenv_undi_cleanup()) */
#define	PXENV_UNDI_CLEANUP		0x0002
/** UNDI INITIALIZE (call function pxenv_undi_initialize()) */
#define	PXENV_UNDI_INITIALIZE		0x0003
/** UNDI RESET ADAPTER (call function pxenv_undi_reset_adapter()) */
#define	PXENV_UNDI_RESET_ADAPTER	0x0004
/** UNDI SHUTDOWN (call function pxenv_undi_shutdown()) */
#define	PXENV_UNDI_SHUTDOWN		0x0005
/** UNDI OPEN (call function pxenv_undi_open()) */
#define	PXENV_UNDI_OPEN			0x0006
/** UNDI CLOSE (call function pxenv_undi_close()) */
#define	PXENV_UNDI_CLOSE		0x0007
/** UNDI TRANSMIT PACKET (call function pxenv_undi_transmit()) */
#define	PXENV_UNDI_TRANSMIT		0x0008
/** UNDI SET MULTICAST ADDRESS
 *  (call function pxenv_undi_set_mcast_address()) */
#define	PXENV_UNDI_SET_MCAST_ADDRESS	0x0009
/** UNDI SET STATION ADDRESS
 *  (call function pxenv_undi_set_station_address()) */
#define	PXENV_UNDI_SET_STATION_ADDRESS	0x000A
/** UNDI SET PACKET FILTER (call function pxenv_undi_set_packet_filter()) */
#define	PXENV_UNDI_SET_PACKET_FILTER	0x000B
/** UNDI GET INFORMATION (call function pxenv_undi_get_information()) */
#define	PXENV_UNDI_GET_INFORMATION	0x000C
/** UNDI GET STATISTICS (call function pxenv_undi_get_statistics()) */
#define	PXENV_UNDI_GET_STATISTICS	0x000D
/** UNDI CLEAR STATISTICS (call function pxenv_undi_get_statistics()) */
#define	PXENV_UNDI_CLEAR_STATISTICS	0x000E
/** UNDI INITIATE DIAGS (call function pxenv_undi_initiate_diags()) */
#define	PXENV_UNDI_INITIATE_DIAGS	0x000F
/** UNDI FORCE INTERRUPT (call function pxenv_undi_force_interrupt()) */
#define	PXENV_UNDI_FORCE_INTERRUPT	0x0010
/** UNDI GET MULTICAST ADDRESS
 *  (call function pxenv_undi_get_mcast_address()) */
#define	PXENV_UNDI_GET_MCAST_ADDRESS	0x0011
/** UNDI GET NIC TYPE (call function pxenv_undi_get_nic_type()) */
#define	PXENV_UNDI_GET_NIC_TYPE		0x0012
/** UNDI GET IFACE INFO (call function pxenv_undi_get_iface_info()) */
#define	PXENV_UNDI_GET_IFACE_INFO	0x0013
/** UNDI ISR (call function pxenv_undi_isr()) */
#define	PXENV_UNDI_ISR			0x0014
/** UNDI GET STATE (call function pxenv_undi_get_state()) */
#define PXENV_UNDI_GET_STATE		0x0015
/** STOP UNDI (call function pxenv_stop_undi()) */
#define	PXENV_STOP_UNDI			0x0015
/** TFTP OPEN (call function pxenv_tftp_open()) */
#define	PXENV_TFTP_OPEN			0x0020
/** TFTP CLOSE (call function pxenv_tftp_close()) */
#define	PXENV_TFTP_CLOSE		0x0021
/** TFTP READ (call function pxenv_tftp_read()) */
#define	PXENV_TFTP_READ			0x0022
/** TFTP/MTFTP READ FILE (call function pxenv_tftp_read_file()) */
#define	PXENV_TFTP_READ_FILE		0x0023
/** TFTP GET FILE SIZE (call function pxenv_tftp_get_fsize()) */
#define	PXENV_TFTP_GET_FSIZE		0x0025
/** UDP OPEN (call function pxenv_udp_open()) */
#define	PXENV_UDP_OPEN			0x0030
/** UDP CLOSE (call function pxenv_udp_close()) */
#define	PXENV_UDP_CLOSE			0x0031
/** UDP WRITE (call function pxenv_udp_write()) */
#define	PXENV_UDP_READ			0x0032
/** UDP READ (call function pxenv_udp_read()) */
#define	PXENV_UDP_WRITE			0x0033
/** UNLOAD BASE CODE STACK (call function pxenv_unload_stack()) */
#define	PXENV_UNLOAD_STACK		0x0070
/** GET CACHED INFO (call function pxenv_get_cached_info()) */
#define	PXENV_GET_CACHED_INFO		0x0071
/** RESTART TFTP (call function pxenv_restart_tftp()) */
#define	PXENV_RESTART_TFTP		0x0073
/** START BASE (call function pxenv_start_base()) */
#define	PXENV_START_BASE		0x0075
/** STOP BASE (call function pxenv_stop_base()) */
#define	PXENV_STOP_BASE			0x0076
/** @} */

/** @defgroup pxe_preboot PXE Preboot API
 * @{
 */
extern PXENV_EXIT_t pxenv_unload_stack ( struct s_PXENV_UNLOAD_STACK
					 *unload_stack );
extern PXENV_EXIT_t pxenv_get_cached_info ( struct s_PXENV_GET_CACHED_INFO
					    *get_cached_info );
extern PXENV_EXIT_t pxenv_restart_tftp ( struct s_PXENV_TFTP_READ_FILE
					 *restart_tftp );
extern PXENV_EXIT_t pxenv_start_undi ( struct s_PXENV_START_UNDI *start_undi );
extern PXENV_EXIT_t pxenv_stop_undi ( struct s_PXENV_STOP_UNDI *stop_undi );
extern PXENV_EXIT_t pxenv_start_base ( struct s_PXENV_START_BASE *start_base );
extern PXENV_EXIT_t pxenv_stop_base ( struct s_PXENV_STOP_BASE *stop_base );
/** @} */

/** @defgroup pxe_tftp PXE TFTP API
 * @{
 */
extern PXENV_EXIT_t pxenv_tftp_open ( struct s_PXENV_TFTP_OPEN *tftp_open );
extern PXENV_EXIT_t pxenv_tftp_close ( struct s_PXENV_TFTP_CLOSE *tftp_close );
extern PXENV_EXIT_t pxenv_tftp_read ( struct s_PXENV_TFTP_READ *tftp_read );
extern PXENV_EXIT_t pxenv_tftp_read_file ( struct s_PXENV_TFTP_READ_FILE
					   *tftp_read_file );
extern PXENV_EXIT_t pxenv_tftp_get_fsize ( struct s_PXENV_TFTP_GET_FSIZE
					   *get_fsize );
/** @} */

/** @defgroup pxe_udp PXE UDP API
 * @{
 */
extern PXENV_EXIT_t pxenv_udp_open ( struct s_PXENV_UDP_OPEN *udp_open );
extern PXENV_EXIT_t pxenv_udp_close ( struct s_PXENV_UDP_CLOSE *udp_close );
extern PXENV_EXIT_t pxenv_udp_write ( struct s_PXENV_UDP_WRITE *udp_write );
extern PXENV_EXIT_t pxenv_udp_read ( struct s_PXENV_UDP_READ *udp_read );
/** @} */

/** @defgroup pxe_undi PXE UNDI API
 * @{
 */
extern PXENV_EXIT_t pxenv_undi_startup ( struct s_PXENV_UNDI_STARTUP
					 *undi_startup );
extern PXENV_EXIT_t pxenv_undi_cleanup ( struct s_PXENV_UNDI_CLEANUP
					 *undi_cleanup );
extern PXENV_EXIT_t pxenv_undi_initialize ( struct s_PXENV_UNDI_INITIALIZE
					    *undi_initialize );
extern PXENV_EXIT_t pxenv_undi_reset_adapter ( struct s_PXENV_UNDI_RESET
					       *undi_reset_adapter );
extern PXENV_EXIT_t pxenv_undi_shutdown ( struct s_PXENV_UNDI_SHUTDOWN
					  *undi_shutdown );
extern PXENV_EXIT_t pxenv_undi_open ( struct s_PXENV_UNDI_OPEN *undi_open );
extern PXENV_EXIT_t pxenv_undi_close ( struct s_PXENV_UNDI_CLOSE *undi_close );
extern PXENV_EXIT_t pxenv_undi_transmit ( struct s_PXENV_UNDI_TRANSMIT
					  *undi_transmit );
extern PXENV_EXIT_t pxenv_undi_set_mcast_address (
	       struct s_PXENV_UNDI_SET_MCAST_ADDRESS *undi_set_mcast_address );
extern PXENV_EXIT_t pxenv_undi_set_station_address (
	   struct s_PXENV_UNDI_SET_STATION_ADDRESS *undi_set_station_address );
extern PXENV_EXIT_t pxenv_undi_set_packet_filter (
	       struct s_PXENV_UNDI_SET_PACKET_FILTER *undi_set_packet_filter );
extern PXENV_EXIT_t pxenv_undi_get_information (
		   struct s_PXENV_UNDI_GET_INFORMATION *undi_get_information );
extern PXENV_EXIT_t pxenv_undi_get_statistics (
		     struct s_PXENV_UNDI_GET_STATISTICS *undi_get_statistics );
extern PXENV_EXIT_t pxenv_undi_clear_statistics (
		 struct s_PXENV_UNDI_CLEAR_STATISTICS *undi_clear_statistics );
extern PXENV_EXIT_t pxenv_undi_initiate_diags (
		     struct s_PXENV_UNDI_INITIATE_DIAGS *undi_initiate_diags );
extern PXENV_EXIT_t pxenv_undi_force_interrupt (
		   struct s_PXENV_UNDI_FORCE_INTERRUPT *undi_force_interrupt );
extern PXENV_EXIT_t pxenv_undi_get_mcast_address (
	       struct s_PXENV_UNDI_GET_MCAST_ADDRESS *undi_get_mcast_address );
extern PXENV_EXIT_t pxenv_undi_get_nic_type ( 
			 struct s_PXENV_UNDI_GET_NIC_TYPE *undi_get_nic_type );
extern PXENV_EXIT_t pxenv_undi_get_iface_info (
		     struct s_PXENV_UNDI_GET_IFACE_INFO *undi_get_iface_info );
extern PXENV_EXIT_t pxenv_undi_get_state ( struct s_PXENV_UNDI_GET_STATE
					   *undi_get_state );
extern PXENV_EXIT_t pxenv_undi_isr ( struct s_PXENV_UNDI_ISR *undi_isr );
/** @} */

/** @} */ /* addtogroup */

#endif /* PXE_API_H */
