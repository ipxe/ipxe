#ifndef PXE_API_H
#define PXE_API_H

/** @file
 *
 * Preboot eXecution Environment (PXE) API
 *
 */

#include "pxe_types.h"

/** @addtogroup pxe Preboot eXecution Environment (PXE) API
 *  @{
 */

/** @defgroup pxe_preboot_api PXE Preboot API
 *
 * General high-level functions: #PXENV_UNLOAD_STACK, #PXENV_START_UNDI etc.
 *
 * @{
 */

/** @defgroup pxenv_unload_stack PXENV_UNLOAD_STACK
 *
 *  UNLOAD BASE CODE STACK
 *
 *  @{
 */

/** PXE API function code for pxenv_unload_stack() */
#define	PXENV_UNLOAD_STACK		0x0070

/** Parameter block for pxenv_unload_stack() */
struct s_PXENV_UNLOAD_STACK {
	PXENV_STATUS_t Status;			/**< PXE status code */
	UINT8_t reserved[10];			/**< Must be zero */
} PACKED;

typedef struct s_PXENV_UNLOAD_STACK PXENV_UNLOAD_STACK_t;

extern PXENV_EXIT_t pxenv_unload_stack ( struct s_PXENV_UNLOAD_STACK
					 *unload_stack );

/** @} */ /* pxenv_unload_stack */

/** @defgroup pxenv_get_cached_info PXENV_GET_CACHED_INFO
 *
 *  GET CACHED INFO
 *
 *  @{
 */

/** PXE API function code for pxenv_get_cached_info() */
#define	PXENV_GET_CACHED_INFO		0x0071

/** The client's DHCPDISCOVER packet */
#define PXENV_PACKET_TYPE_DHCP_DISCOVER	1

/** The DHCP server's DHCPACK packet */
#define PXENV_PACKET_TYPE_DHCP_ACK	2

/** The Boot Server's Discover Reply packet
 *
 * This packet contains DHCP option 60 set to "PXEClient", a valid
 * boot file name, and may or may not contain MTFTP options.
 */
#define PXENV_PACKET_TYPE_CACHED_REPLY	3

/** Parameter block for pxenv_get_cached_info() */
struct s_PXENV_GET_CACHED_INFO {
	PXENV_STATUS_t Status;			/**< PXE status code */
	/** Packet type.
	 *
	 * Valid values are #PXENV_PACKET_TYPE_DHCP_DISCOVER,
	 * #PXENV_PACKET_TYPE_DHCP_ACK or #PXENV_PACKET_TYPE_CACHED_REPLY
	 */
	UINT16_t PacketType;
	UINT16_t BufferSize;			/**< Buffer size */
	SEGOFF16_t Buffer;			/**< Buffer address */
	UINT16_t BufferLimit			/**< Maximum buffer size */
} PACKED;

typedef struct s_PXENV_GET_CACHED_INFO PXENV_GET_CACHED_INFO_t;

extern PXENV_EXIT_t pxenv_get_cached_info ( struct s_PXENV_GET_CACHED_INFO
					    *get_cached_info );

/** @} */ /* pxenv_get_cached_info */

/** @defgroup pxenv_restart_tftp PXENV_RESTART_TFTP
 *
 *  RESTART TFTP
 *
 *  @{
 */

/** PXE API function code for pxenv_restart_tftp() */
#define	PXENV_RESTART_TFTP		0x0073

/** Parameter block for pxenv_restart_tftp() */
struct s_PXENV_RESTART_TFTP {
} PACKED;

typedef struct s_PXENV_RESTART_TFTP PXENV_RESTART_TFTP_t;

extern PXENV_EXIT_t pxenv_restart_tftp ( struct s_PXENV_TFTP_READ_FILE
					 *restart_tftp );

/** @} */ /* pxenv_restart_tftp */

/** @defgroup pxenv_start_undi PXENV_START_UNDI
 *
 *  START UNDI
 *
 *  @{
 */

/** PXE API function code for pxenv_start_undi() */
#define	PXENV_START_UNDI		0x0000

/** Parameter block for pxenv_start_undi() */
struct s_PXENV_START_UNDI {
} PACKED;

typedef struct s_PXENV_START_UNDI PXENV_START_UNDI_t;

extern PXENV_EXIT_t pxenv_start_undi ( struct s_PXENV_START_UNDI *start_undi );

/** @} */ /* pxenv_start_undi */

/** @defgroup pxenv_stop_undi PXENV_STOP_UNDI
 *
 *  STOP UNDI
 *
 *  @{
 */

/** PXE API function code for pxenv_stop_undi() */
#define	PXENV_STOP_UNDI			0x0015

/** Parameter block for pxenv_stop_undi() */
struct s_PXENV_STOP_UNDI {
} PACKED;

typedef struct s_PXENV_STOP_UNDI PXENV_STOP_UNDI_t;

extern PXENV_EXIT_t pxenv_stop_undi ( struct s_PXENV_STOP_UNDI *stop_undi );

/** @} */ /* pxenv_stop_undi */

/** @defgroup pxenv_start_base PXENV_START_BASE
 *
 *  START BASE
 *
 *  @{
 */

/** PXE API function code for pxenv_start_base() */
#define	PXENV_START_BASE		0x0075

/** Parameter block for pxenv_start_base() */
struct s_PXENV_START_BASE {
} PACKED;

typedef struct s_PXENV_START_BASE PXENV_START_BASE_t;

extern PXENV_EXIT_t pxenv_start_base ( struct s_PXENV_START_BASE *start_base );

/** @} */ /* pxenv_start_base */

/** @defgroup pxenv_stop_base PXENV_STOP_BASE
 *
 *  STOP BASE
 *
 *  @{
 */

/** PXE API function code for pxenv_stop_base() */
#define	PXENV_STOP_BASE			0x0076

/** Parameter block for pxenv_stop_base() */
struct s_PXENV_STOP_BASE {
} PACKED;

typedef struct s_PXENV_STOP_BASE PXENV_STOP_BASE_t;

extern PXENV_EXIT_t pxenv_stop_base ( struct s_PXENV_STOP_BASE *stop_base );

/** @} */ /* pxenv_stop_base */

/** @} */ /* pxe_preboot_api */

/** @defgroup pxe_tftp_api PXE TFTP API
 *
 * Download files via TFTP or MTFTP
 *
 * @{
 */

/** @defgroup pxenv_tftp_open PXENV_TFTP_OPEN
 *
 *  TFTP OPEN
 *
 *  @{
 */

/** PXE API function code for pxenv_tftp_open() */
#define	PXENV_TFTP_OPEN			0x0020

/** Parameter block for pxenv_tftp_open() */
struct s_PXENV_TFTP_OPEN {
} PACKED;

typedef struct s_PXENV_TFTP_OPEN PXENV_TFTP_OPEN_t;

extern PXENV_EXIT_t pxenv_tftp_open ( struct s_PXENV_TFTP_OPEN *tftp_open );

/** @} */ /* pxenv_tftp_open */

/** @defgroup pxenv_tftp_close PXENV_TFTP_CLOSE
 *
 *  TFTP CLOSE
 *
 *  @{
 */

/** PXE API function code for pxenv_tftp_close() */
#define	PXENV_TFTP_CLOSE		0x0021

/** Parameter block for pxenv_tftp_close() */
struct s_PXENV_TFTP_CLOSE {
} PACKED;

typedef struct s_PXENV_TFTP_CLOSE PXENV_TFTP_CLOSE_t;

extern PXENV_EXIT_t pxenv_tftp_close ( struct s_PXENV_TFTP_CLOSE *tftp_close );

/** @} */ /* pxenv_tftp_close */

/** @defgroup pxenv_tftp_read PXENV_TFTP_READ
 *
 *  TFTP READ
 *
 *  @{
 */

/** PXE API function code for pxenv_tftp_read() */
#define	PXENV_TFTP_READ			0x0022

/** Parameter block for pxenv_tftp_read() */
struct s_PXENV_TFTP_READ {
} PACKED;

typedef struct s_PXENV_TFTP_READ PXENV_TFTP_READ_t;

extern PXENV_EXIT_t pxenv_tftp_read ( struct s_PXENV_TFTP_READ *tftp_read );

/** @} */ /* pxenv_tftp_read */

/** @defgroup pxenv_tftp_read_file PXENV_TFTP_READ_FILE
 *
 *  TFTP/MTFTP READ FILE
 *
 *  @{
 */

/** PXE API function code for pxenv_tftp_read_file() */
#define	PXENV_TFTP_READ_FILE		0x0023

/** Parameter block for pxenv_tftp_read_file() */
struct s_PXENV_TFTP_READ_FILE {
} PACKED;

typedef struct s_PXENV_TFTP_READ_FILE PXENV_TFTP_READ_FILE_t;

extern PXENV_EXIT_t pxenv_tftp_read_file ( struct s_PXENV_TFTP_READ_FILE
					   *tftp_read_file );

/** @} */ /* pxenv_tftp_read_file */

/** @defgroup pxenv_tftp_get_fsize PXENV_TFTP_GET_FSIZE
 *
 *  TFTP GET FILE SIZE
 *
 *  @{
 */

/** PXE API function code for pxenv_tftp_get_fsize() */
#define	PXENV_TFTP_GET_FSIZE		0x0025

/** Parameter block for pxenv_tftp_get_fsize() */
struct s_PXENV_TFTP_GET_FSIZE {
} PACKED;

typedef struct s_PXENV_TFTP_GET_FSIZE PXENV_TFTP_GET_FSIZE_t;

extern PXENV_EXIT_t pxenv_tftp_get_fsize ( struct s_PXENV_TFTP_GET_FSIZE
					   *get_fsize );

/** @} */ /* pxenv_tftp_get_fsize */

/** @} */ /* pxe_tftp_api */

/** @defgroup pxe_udp_api PXE UDP API
 *
 * Transmit and receive UDP packets
 *
 * @{
 */

/** @defgroup pxenv_udp_open PXENV_UDP_OPEN
 *
 *  UDP OPEN
 *
 *  @{
 */

/** PXE API function code for pxenv_udp_open() */
#define	PXENV_UDP_OPEN			0x0030

/** Parameter block for pxenv_udp_open() */
struct s_PXENV_UDP_OPEN {
} PACKED;

typedef struct s_PXENV_UDP_OPEN PXENV_UDP_OPEN_t;

extern PXENV_EXIT_t pxenv_udp_open ( struct s_PXENV_UDP_OPEN *udp_open );

/** @} */ /* pxenv_udp_open */

/** @defgroup pxenv_udp_close PXENV_UDP_CLOSE
 *
 *  UDP CLOSE
 *
 *  @{
 */

/** PXE API function code for pxenv_udp_close() */
#define	PXENV_UDP_CLOSE			0x0031

/** Parameter block for pxenv_udp_close() */
struct s_PXENV_UDP_CLOSE {
} PACKED;

typedef struct s_PXENV_UDP_CLOSE PXENV_UDP_CLOSE_t;

extern PXENV_EXIT_t pxenv_udp_close ( struct s_PXENV_UDP_CLOSE *udp_close );

/** @} */ /* pxenv_udp_close */

/** @defgroup pxenv_udp_write PXENV_UDP_WRITE
 *
 *  UDP WRITE
 *
 *  @{
 */

/** PXE API function code for pxenv_udp_write() */
#define	PXENV_UDP_WRITE			0x0033

/** Parameter block for pxenv_udp_write() */
struct s_PXENV_UDP_WRITE {
} PACKED;

typedef struct s_PXENV_UDP_WRITE PXENV_UDP_WRITE_t;

extern PXENV_EXIT_t pxenv_udp_write ( struct s_PXENV_UDP_WRITE *udp_write );

/** @} */ /* pxenv_udp_write */

/** @defgroup pxenv_udp_read PXENV_UDP_READ
 *
 *  UDP READ
 *
 *  @{
 */

/** PXE API function code for pxenv_udp_read() */
#define	PXENV_UDP_READ			0x0032

/** Parameter block for pxenv_udp_read() */
struct s_PXENV_UDP_READ {
} PACKED;

typedef struct s_PXENV_UDP_READ PXENV_UDP_READ_t;

extern PXENV_EXIT_t pxenv_udp_read ( struct s_PXENV_UDP_READ *udp_read );

/** @} */ /* pxenv_udp_read */

/** @} */ /* pxe_udp_api */

/** @defgroup pxe_undi_api PXE UNDI API
 *
 * Direct control of the network interface card
 *
 * @{
 */

/** @defgroup pxenv_undi_startup PXENV_UNDI_STARTUP
 *
 *  UNDI STARTUP
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_startup() */
#define	PXENV_UNDI_STARTUP		0x0001

/** Parameter block for pxenv_undi_startup() */
struct s_PXENV_UNDI_STARTUP {
} PACKED;

typedef struct s_PXENV_UNDI_STARTUP PXENV_UNDI_STARTUP_t;

extern PXENV_EXIT_t pxenv_undi_startup ( struct s_PXENV_UNDI_STARTUP
					 *undi_startup );

/** @} */ /* pxenv_undi_startup */

/** @defgroup pxenv_undi_cleanup PXENV_UNDI_CLEANUP
 *
 *  UNDI CLEANUP
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_cleanup() */
#define	PXENV_UNDI_CLEANUP		0x0002

/** Parameter block for pxenv_undi_cleanup() */
struct s_PXENV_UNDI_CLEANUP {
} PACKED;

typedef struct s_PXENV_UNDI_CLEANUP PXENV_UNDI_CLEANUP_t;

extern PXENV_EXIT_t pxenv_undi_cleanup ( struct s_PXENV_UNDI_CLEANUP
					 *undi_cleanup );

/** @} */ /* pxenv_undi_cleanup */

/** @defgroup pxenv_undi_initialize PXENV_UNDI_INITIALIZE
 *
 *  UNDI INITIALIZE
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_initialize() */
#define	PXENV_UNDI_INITIALIZE		0x0003

/** Parameter block for pxenv_undi_initialize() */
struct s_PXENV_UNDI_INITIALIZE {
} PACKED;

typedef struct s_PXENV_UNDI_INITIALIZE PXENV_UNDI_INITIALIZE_t;

extern PXENV_EXIT_t pxenv_undi_initialize ( struct s_PXENV_UNDI_INITIALIZE
					    *undi_initialize );

/** @} */ /* pxenv_undi_initialize */

/** @defgroup pxenv_undi_reset_adapter PXENV_UNDI_RESET_ADAPTER
 *
 *  UNDI RESET ADAPTER
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_reset_adapter() */
#define	PXENV_UNDI_RESET_ADAPTER	0x0004

/** Parameter block for pxenv_undi_reset_adapter() */
struct s_PXENV_UNDI_RESET_ADAPTER {
} PACKED;

typedef struct s_PXENV_UNDI_RESET_ADAPTER PXENV_UNDI_RESET_ADAPTER_t;

extern PXENV_EXIT_t pxenv_undi_reset_adapter ( struct s_PXENV_UNDI_RESET
					       *undi_reset_adapter );

/** @} */ /* pxenv_undi_reset_adapter */

/** @defgroup pxenv_undi_shutdown PXENV_UNDI_SHUTDOWN
 *
 *  UNDI SHUTDOWN
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_shutdown() */
#define	PXENV_UNDI_SHUTDOWN		0x0005

/** Parameter block for pxenv_undi_shutdown() */
struct s_PXENV_UNDI_SHUTDOWN {
} PACKED;

typedef struct s_PXENV_UNDI_SHUTDOWN PXENV_UNDI_SHUTDOWN_t;

extern PXENV_EXIT_t pxenv_undi_shutdown ( struct s_PXENV_UNDI_SHUTDOWN
					  *undi_shutdown );

/** @} */ /* pxenv_undi_shutdown */

/** @defgroup pxenv_undi_open PXENV_UNDI_OPEN
 *
 *  UNDI OPEN
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_open() */
#define	PXENV_UNDI_OPEN			0x0006

/** Parameter block for pxenv_undi_open() */
struct s_PXENV_UNDI_OPEN {
} PACKED;

typedef struct s_PXENV_UNDI_OPEN PXENV_UNDI_OPEN_t;

extern PXENV_EXIT_t pxenv_undi_open ( struct s_PXENV_UNDI_OPEN *undi_open );

/** @} */ /* pxenv_undi_open */

/** @defgroup pxenv_undi_close PXENV_UNDI_CLOSE
 *
 *  UNDI CLOSE
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_close() */
#define	PXENV_UNDI_CLOSE		0x0007

/** Parameter block for pxenv_undi_close() */
struct s_PXENV_UNDI_CLOSE {
} PACKED;

typedef struct s_PXENV_UNDI_CLOSE PXENV_UNDI_CLOSE_t;

extern PXENV_EXIT_t pxenv_undi_close ( struct s_PXENV_UNDI_CLOSE *undi_close );

/** @} */ /* pxenv_undi_close */

/** @defgroup pxenv_undi_transmit PXENV_UNDI_TRANSMIT
 *
 *  UNDI TRANSMIT PACKET
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_transmit() */
#define	PXENV_UNDI_TRANSMIT		0x0008

/** Parameter block for pxenv_undi_transmit() */
struct s_PXENV_UNDI_TRANSMIT {
} PACKED;

typedef struct s_PXENV_UNDI_TRANSMIT PXENV_UNDI_TRANSMIT_t;

extern PXENV_EXIT_t pxenv_undi_transmit ( struct s_PXENV_UNDI_TRANSMIT
					  *undi_transmit );

/** @} */ /* pxenv_undi_transmit */

/** @defgroup pxenv_undi_set_mcast_address PXENV_UNDI_SET_MCAST_ADDRESS
 *
 *  UNDI SET MULTICAST ADDRESS
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_set_mcast_address() */
#define	PXENV_UNDI_SET_MCAST_ADDRESS	0x0009

/** Parameter block for pxenv_undi_set_mcast_address() */
struct s_PXENV_UNDI_SET_MCAST_ADDRESS {
} PACKED;

typedef struct s_PXENV_UNDI_SET_MCAST_ADDRESS PXENV_UNDI_SET_MCAST_ADDRESS_t;

extern PXENV_EXIT_t pxenv_undi_set_mcast_address (
	       struct s_PXENV_UNDI_SET_MCAST_ADDRESS *undi_set_mcast_address );

/** @} */ /* pxenv_undi_set_mcast_address */

/** @defgroup pxenv_undi_set_station_address PXENV_UNDI_SET_STATION_ADDRESS
 *
 *  UNDI SET STATION ADDRESS
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_set_station_address() */
#define	PXENV_UNDI_SET_STATION_ADDRESS	0x000a

/** Parameter block for pxenv_undi_set_station_address() */
struct s_PXENV_UNDI_SET_STATION_ADDRESS {
} PACKED;

typedef struct s_PXENV_UNDI_SET_STATION_ADDRESS PXENV_UNDI_SET_STATION_ADDRESS_t;

extern PXENV_EXIT_t pxenv_undi_set_station_address (
	   struct s_PXENV_UNDI_SET_STATION_ADDRESS *undi_set_station_address );

/** @} */ /* pxenv_undi_set_station_address */

/** @defgroup pxenv_undi_set_packet_filter PXENV_UNDI_SET_PACKET_FILTER
 *
 *  UNDI SET PACKET FILTER
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_set_packet_filter() */
#define	PXENV_UNDI_SET_PACKET_FILTER	0x000b

/** Parameter block for pxenv_undi_set_packet_filter() */
struct s_PXENV_UNDI_SET_PACKET_FILTER {
} PACKED;

typedef struct s_PXENV_UNDI_SET_PACKET_FILTER PXENV_UNDI_SET_PACKET_FILTER_t;

extern PXENV_EXIT_t pxenv_undi_set_packet_filter (
	       struct s_PXENV_UNDI_SET_PACKET_FILTER *undi_set_packet_filter );

/** @} */ /* pxenv_undi_set_packet_filter */

/** @defgroup pxenv_undi_get_information PXENV_UNDI_GET_INFORMATION
 *
 *  UNDI GET INFORMATION
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_get_information() */
#define	PXENV_UNDI_GET_INFORMATION	0x000c

/** Parameter block for pxenv_undi_get_information() */
struct s_PXENV_UNDI_GET_INFORMATION {
} PACKED;

typedef struct s_PXENV_UNDI_GET_INFORMATION PXENV_UNDI_GET_INFORMATION_t;

extern PXENV_EXIT_t pxenv_undi_get_information (
		   struct s_PXENV_UNDI_GET_INFORMATION *undi_get_information );

/** @} */ /* pxenv_undi_get_information */

/** @defgroup pxenv_undi_get_statistics PXENV_UNDI_GET_STATISTICS
 *
 *  UNDI GET STATISTICS
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_get_statistics() */
#define	PXENV_UNDI_GET_STATISTICS	0x000d

/** Parameter block for pxenv_undi_get_statistics() */
struct s_PXENV_UNDI_GET_STATISTICS {
} PACKED;

typedef struct s_PXENV_UNDI_GET_STATISTICS PXENV_UNDI_GET_STATISTICS_t;

extern PXENV_EXIT_t pxenv_undi_get_statistics (
		     struct s_PXENV_UNDI_GET_STATISTICS *undi_get_statistics );

/** @} */ /* pxenv_undi_get_statistics */

/** @defgroup pxenv_undi_clear_statistics PXENV_UNDI_CLEAR_STATISTICS
 *
 *  UNDI CLEAR STATISTICS
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_clear_statistics() */
#define	PXENV_UNDI_CLEAR_STATISTICS	0x000e

/** Parameter block for pxenv_undi_clear_statistics() */
struct s_PXENV_UNDI_CLEAR_STATISTICS {
} PACKED;

typedef struct s_PXENV_UNDI_CLEAR_STATISTICS PXENV_UNDI_CLEAR_STATISTICS_t;

extern PXENV_EXIT_t pxenv_undi_clear_statistics (
		 struct s_PXENV_UNDI_CLEAR_STATISTICS *undi_clear_statistics );

/** @} */ /* pxenv_undi_clear_statistics */

/** @defgroup pxenv_undi_initiate_diags PXENV_UNDI_INITIATE_DIAGS
 *
 *  UNDI INITIATE DIAGS
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_initiate_diags() */
#define	PXENV_UNDI_INITIATE_DIAGS	0x000f

/** Parameter block for pxenv_undi_initiate_diags() */
struct s_PXENV_UNDI_INITIATE_DIAGS {
} PACKED;

typedef struct s_PXENV_UNDI_INITIATE_DIAGS PXENV_UNDI_INITIATE_DIAGS_t;

extern PXENV_EXIT_t pxenv_undi_initiate_diags (
		     struct s_PXENV_UNDI_INITIATE_DIAGS *undi_initiate_diags );

/** @} */ /* pxenv_undi_initiate_diags */

/** @defgroup pxenv_undi_force_interrupt PXENV_UNDI_FORCE_INTERRUPT
 *
 *  UNDI FORCE INTERRUPT
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_force_interrupt() */
#define	PXENV_UNDI_FORCE_INTERRUPT	0x0010

/** Parameter block for pxenv_undi_force_interrupt() */
struct s_PXENV_UNDI_FORCE_INTERRUPT {
} PACKED;

typedef struct s_PXENV_UNDI_FORCE_INTERRUPT PXENV_UNDI_FORCE_INTERRUPT_t;

extern PXENV_EXIT_t pxenv_undi_force_interrupt (
		   struct s_PXENV_UNDI_FORCE_INTERRUPT *undi_force_interrupt );

/** @} */ /* pxenv_undi_force_interrupt */

/** @defgroup pxenv_undi_get_mcast_address PXENV_UNDI_GET_MCAST_ADDRESS
 *
 *  UNDI GET MULTICAST ADDRESS
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_get_mcast_address() */
#define	PXENV_UNDI_GET_MCAST_ADDRESS	0x0011

/** Parameter block for pxenv_undi_get_mcast_address() */
struct s_PXENV_UNDI_GET_MCAST_ADDRESS {
} PACKED;

typedef struct s_PXENV_UNDI_GET_MCAST_ADDRESS PXENV_UNDI_GET_MCAST_ADDRESS_t;

extern PXENV_EXIT_t pxenv_undi_get_mcast_address (
	       struct s_PXENV_UNDI_GET_MCAST_ADDRESS *undi_get_mcast_address );

/** @} */ /* pxenv_undi_get_mcast_address */

/** @defgroup pxenv_undi_get_nic_type PXENV_UNDI_GET_NIC_TYPE
 *
 *  UNDI GET NIC TYPE
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_get_nic_type() */
#define	PXENV_UNDI_GET_NIC_TYPE		0x0012

/** Parameter block for pxenv_undi_get_nic_type() */
struct s_PXENV_UNDI_GET_NIC_TYPE {
} PACKED;

typedef struct s_PXENV_UNDI_GET_NIC_TYPE PXENV_UNDI_GET_NIC_TYPE_t;

extern PXENV_EXIT_t pxenv_undi_get_nic_type ( 
			 struct s_PXENV_UNDI_GET_NIC_TYPE *undi_get_nic_type );

/** @} */ /* pxenv_undi_get_nic_type */

/** @defgroup pxenv_undi_get_iface_info PXENV_UNDI_GET_IFACE_INFO
 *
 *  UNDI GET IFACE INFO
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_get_iface_info() */
#define	PXENV_UNDI_GET_IFACE_INFO	0x0013

/** Parameter block for pxenv_undi_get_iface_info() */
struct s_PXENV_UNDI_GET_IFACE_INFO {
} PACKED;

typedef struct s_PXENV_UNDI_GET_IFACE_INFO PXENV_UNDI_GET_IFACE_INFO_t;

extern PXENV_EXIT_t pxenv_undi_get_iface_info (
		     struct s_PXENV_UNDI_GET_IFACE_INFO *undi_get_iface_info );

/** @} */ /* pxenv_undi_get_iface_info */

/** @defgroup pxenv_undi_get_state PXENV_UNDI_GET_STATE
 *
 *  UNDI GET STATE
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_get_state() */
#define PXENV_UNDI_GET_STATE		0x0015

/** Parameter block for pxenv_undi_get_state() */
struct s_PXENV_UNDI_GET_STATE {
} PACKED;

typedef struct s_PXENV_UNDI_GET_STATE PXENV_UNDI_GET_STATE_t;

extern PXENV_EXIT_t pxenv_undi_get_state ( struct s_PXENV_UNDI_GET_STATE
					   *undi_get_state );

/** @} */ /* pxenv_undi_get_state */

/** @defgroup pxenv_undi_isr PXENV_UNDI_ISR
 *
 *  UNDI ISR
 *
 *  @{
 */

/** PXE API function code for pxenv_undi_isr() */
#define	PXENV_UNDI_ISR			0x0014

/** Parameter block for pxenv_undi_isr() */
struct s_PXENV_UNDI_ISR {
} PACKED;

typedef struct s_PXENV_UNDI_ISR PXENV_UNDI_ISR_t;

extern PXENV_EXIT_t pxenv_undi_isr ( struct s_PXENV_UNDI_ISR *undi_isr );

/** @} */ /* pxenv_undi_isr */

/** @} */ /* pxe_undi_api */

/** @} */ /* pxe */

#endif /* PXE_API_H */
