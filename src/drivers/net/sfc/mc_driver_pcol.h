/****************************************************************************
 * Driver for Solarflare network controllers and boards
 * Copyright 2012-2017 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */
#ifndef SFC_MCDI_PCOL_H
#define SFC_MCDI_PCOL_H

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** \file mc_driver_pcol.h
 * This file is a subset of the MCDI headers generated from the yml files.
 */

/* The current version of the MCDI protocol.
 *
 * Note that the ROM burnt into the card only talks V0, so at the very
 * least every driver must support version 0 and MCDI_PCOL_VERSION
 */
#ifdef WITH_MCDI_V2
#define MCDI_PCOL_VERSION 2
#else
#define MCDI_PCOL_VERSION 1
#endif

/* Unused commands: 0x23, 0x27, 0x30, 0x31 */

/* MCDI version 1
 *
 * Each MCDI request starts with an MCDI_HEADER, which is a 32bit
 * structure, filled in by the client.
 *
 *       0       7  8     16    20     22  23  24    31
 *      | CODE | R | LEN | SEQ | Rsvd | E | R | XFLAGS |
 *               |                      |   |
 *               |                      |   \--- Response
 *               |                      \------- Error
 *               \------------------------------ Resync (always set)
 *
 * The client writes it's request into MC shared memory, and rings the
 * doorbell. Each request is completed by either by the MC writing
 * back into shared memory, or by writing out an event.
 *
 * All MCDI commands support completion by shared memory response. Each
 * request may also contain additional data (accounted for by HEADER.LEN),
 * and some response's may also contain additional data (again, accounted
 * for by HEADER.LEN).
 *
 * Some MCDI commands support completion by event, in which any associated
 * response data is included in the event.
 *
 * The protocol requires one response to be delivered for every request, a
 * request should not be sent unless the response for the previous request
 * has been received (either by polling shared memory, or by receiving
 * an event).
 */

/** Request/Response structure */
#define MCDI_HEADER_OFST 0
#define MCDI_HEADER_CODE_LBN 0
#define MCDI_HEADER_CODE_WIDTH 7
#define MCDI_HEADER_RESYNC_LBN 7
#define MCDI_HEADER_RESYNC_WIDTH 1
#define MCDI_HEADER_DATALEN_LBN 8
#define MCDI_HEADER_DATALEN_WIDTH 8
#define MCDI_HEADER_SEQ_LBN 16
#define MCDI_HEADER_SEQ_WIDTH 4
#define MCDI_HEADER_RSVD_LBN 20
#define MCDI_HEADER_RSVD_WIDTH 1
#define MCDI_HEADER_NOT_EPOCH_LBN 21
#define MCDI_HEADER_NOT_EPOCH_WIDTH 1
#define MCDI_HEADER_ERROR_LBN 22
#define MCDI_HEADER_ERROR_WIDTH 1
#define MCDI_HEADER_RESPONSE_LBN 23
#define MCDI_HEADER_RESPONSE_WIDTH 1
#define MCDI_HEADER_XFLAGS_LBN 24
#define MCDI_HEADER_XFLAGS_WIDTH 8
/* Request response using event */
#define MCDI_HEADER_XFLAGS_EVREQ 0x01
/* Request (and signal) early doorbell return */
#define MCDI_HEADER_XFLAGS_DBRET 0x02

/* Maximum number of payload bytes */
#define MCDI_CTL_SDU_LEN_MAX_V1 0xfc
#define MCDI_CTL_SDU_LEN_MAX_V2 0x400

#ifdef WITH_MCDI_V2
#define MCDI_CTL_SDU_LEN_MAX MCDI_CTL_SDU_LEN_MAX_V2
#else
#define MCDI_CTL_SDU_LEN_MAX MCDI_CTL_SDU_LEN_MAX_V1
#endif


/* The MC can generate events for two reasons:
 *   - To advance a shared memory request if XFLAGS_EVREQ was set
 *   - As a notification (link state, i2c event), controlled
 *     via MC_CMD_LOG_CTRL
 *
 * Both events share a common structure:
 *
 *  0      32     33      36    44     52     60
 * | Data | Cont | Level | Src | Code | Rsvd |
 *           |
 *           \ There is another event pending in this notification
 *
 * If Code==CMDDONE, then the fields are further interpreted as:
 *
 *   - LEVEL==INFO    Command succeeded
 *   - LEVEL==ERR     Command failed
 *
 *    0     8         16      24     32
 *   | Seq | Datalen | Errno | Rsvd |
 *
 *   These fields are taken directly out of the standard MCDI header, i.e.,
 *   LEVEL==ERR, Datalen == 0 => Reboot
 *
 * Events can be squirted out of the UART (using LOG_CTRL) without a
 * MCDI header.  An event can be distinguished from a MCDI response by
 * examining the first byte which is 0xc0.  This corresponds to the
 * non-existent MCDI command MC_CMD_DEBUG_LOG.
 *
 *      0         7        8
 *     | command | Resync |     = 0xc0
 *
 * Since the event is written in big-endian byte order, this works
 * providing bits 56-63 of the event are 0xc0.
 *
 *      56     60  63
 *     | Rsvd | Code |    = 0xc0
 *
 * Which means for convenience the event code is 0xc for all MC
 * generated events.
 */
#define FSE_AZ_EV_CODE_MCDI_EVRESPONSE 0xc


/* Operation not permitted. */
#define MC_CMD_ERR_EPERM 1
/* Non-existent command target */
#define MC_CMD_ERR_ENOENT 2
/* assert() has killed the MC */
#define MC_CMD_ERR_EINTR 4
/* I/O failure */
#define MC_CMD_ERR_EIO 5
/* Already exists */
#define MC_CMD_ERR_EEXIST 6
/* Try again */
#define MC_CMD_ERR_EAGAIN 11
/* Out of memory */
#define MC_CMD_ERR_ENOMEM 12
/* Caller does not hold required locks */
#define MC_CMD_ERR_EACCES 13
/* Resource is currently unavailable (e.g. lock contention) */
#define MC_CMD_ERR_EBUSY 16
/* No such device */
#define MC_CMD_ERR_ENODEV 19
/* Invalid argument to target */
#define MC_CMD_ERR_EINVAL 22
/* Broken pipe */
#define MC_CMD_ERR_EPIPE 32
/* Read-only */
#define MC_CMD_ERR_EROFS 30
/* Out of range */
#define MC_CMD_ERR_ERANGE 34
/* Non-recursive resource is already acquired */
#define MC_CMD_ERR_EDEADLK 35
/* Operation not implemented */
#define MC_CMD_ERR_ENOSYS 38
/* Operation timed out */
#define MC_CMD_ERR_ETIME 62
/* Link has been severed */
#define MC_CMD_ERR_ENOLINK 67
/* Protocol error */
#define MC_CMD_ERR_EPROTO 71
/* Operation not supported */
#define MC_CMD_ERR_ENOTSUP 95
/* Address not available */
#define MC_CMD_ERR_EADDRNOTAVAIL 99
/* Not connected */
#define MC_CMD_ERR_ENOTCONN 107
/* Operation already in progress */
#define MC_CMD_ERR_EALREADY 114

/* Resource allocation failed. */
#define MC_CMD_ERR_ALLOC_FAIL  0x1000
/* V-adaptor not found. */
#define MC_CMD_ERR_NO_VADAPTOR 0x1001
/* EVB port not found. */
#define MC_CMD_ERR_NO_EVB_PORT 0x1002
/* V-switch not found. */
#define MC_CMD_ERR_NO_VSWITCH  0x1003
/* Too many VLAN tags. */
#define MC_CMD_ERR_VLAN_LIMIT  0x1004
/* Bad PCI function number. */
#define MC_CMD_ERR_BAD_PCI_FUNC 0x1005
/* Invalid VLAN mode. */
#define MC_CMD_ERR_BAD_VLAN_MODE 0x1006
/* Invalid v-switch type. */
#define MC_CMD_ERR_BAD_VSWITCH_TYPE 0x1007
/* Invalid v-port type. */
#define MC_CMD_ERR_BAD_VPORT_TYPE 0x1008
/* MAC address exists. */
#define MC_CMD_ERR_MAC_EXIST 0x1009
/* Slave core not present */
#define MC_CMD_ERR_SLAVE_NOT_PRESENT 0x100a
/* The datapath is disabled. */
#define MC_CMD_ERR_DATAPATH_DISABLED 0x100b
/* The requesting client is not a function */
#define MC_CMD_ERR_CLIENT_NOT_FN  0x100c
/* The requested operation might require the
 * command to be passed between MCs, and the
 * transport doesn't support that.  Should
 * only ever been seen over the UART.
 */
#define MC_CMD_ERR_TRANSPORT_NOPROXY 0x100d
/* VLAN tag(s) exists */
#define MC_CMD_ERR_VLAN_EXIST 0x100e
/* No MAC address assigned to an EVB port */
#define MC_CMD_ERR_NO_MAC_ADDR 0x100f
/* Notifies the driver that the request has been relayed
 * to an admin function for authorization. The driver should
 * wait for a PROXY_RESPONSE event and then resend its request.
 * This error code is followed by a 32-bit handle that
 * helps matching it with the respective PROXY_RESPONSE event.
 */
#define MC_CMD_ERR_PROXY_PENDING 0x1010
#define MC_CMD_ERR_PROXY_PENDING_HANDLE_OFST 4
/* The request cannot be passed for authorization because
 * another request from the same function is currently being
 * authorized. The drvier should try again later.
 */
#define MC_CMD_ERR_PROXY_INPROGRESS 0x1011
/* Returned by MC_CMD_PROXY_COMPLETE if the caller is not the function
 * that has enabled proxying or BLOCK_INDEX points to a function that
 * doesn't await an authorization.
 */
#define MC_CMD_ERR_PROXY_UNEXPECTED 0x1012
/* This code is currently only used internally in FW. Its meaning is that
 * an operation failed due to lack of SR-IOV privilege.
 * Normally it is translated to EPERM by send_cmd_err(),
 * but it may also be used to trigger some special mechanism
 * for handling such case, e.g. to relay the failed request
 * to a designated admin function for authorization.
 */
#define MC_CMD_ERR_NO_PRIVILEGE 0x1013
/* Workaround 26807 could not be turned on/off because some functions
 * have already installed filters. See the comment at
 * MC_CMD_WORKAROUND_BUG26807.
 */
#define MC_CMD_ERR_FILTERS_PRESENT 0x1014
/* The clock whose frequency you've attempted to set set
 * doesn't exist on this NIC
 */
#define MC_CMD_ERR_NO_CLOCK 0x1015
/* Returned by MC_CMD_TESTASSERT if the action that should
 * have caused an assertion failed to do so.
 */
#define MC_CMD_ERR_UNREACHABLE 0x1016
/* This command needs to be processed in the background but there were no
 * resources to do so. Send it again after a command has completed.
 */
#define MC_CMD_ERR_QUEUE_FULL 0x1017

#define MC_CMD_ERR_CODE_OFST 0


#ifdef WITH_MCDI_V2

/* Version 2 adds an optional argument to error returns: the errno value
 * may be followed by the (0-based) number of the first argument that
 * could not be processed.
 */
#define MC_CMD_ERR_ARG_OFST 4

/* No space */
#define MC_CMD_ERR_ENOSPC 28

#endif

/* MCDI_EVENT structuredef */
#define    MCDI_EVENT_LEN 8
#define       MCDI_EVENT_CONT_LBN 32
#define       MCDI_EVENT_CONT_WIDTH 1
#define       MCDI_EVENT_LEVEL_LBN 33
#define       MCDI_EVENT_LEVEL_WIDTH 3
/* enum: Info. */
#define          MCDI_EVENT_LEVEL_INFO  0x0
/* enum: Warning. */
#define          MCDI_EVENT_LEVEL_WARN 0x1
/* enum: Error. */
#define          MCDI_EVENT_LEVEL_ERR 0x2
/* enum: Fatal. */
#define          MCDI_EVENT_LEVEL_FATAL 0x3
#define       MCDI_EVENT_DATA_OFST 0
#define        MCDI_EVENT_CMDDONE_SEQ_LBN 0
#define        MCDI_EVENT_CMDDONE_SEQ_WIDTH 8
#define        MCDI_EVENT_CMDDONE_DATALEN_LBN 8
#define        MCDI_EVENT_CMDDONE_DATALEN_WIDTH 8
#define        MCDI_EVENT_CMDDONE_ERRNO_LBN 16
#define        MCDI_EVENT_CMDDONE_ERRNO_WIDTH 8
#define        MCDI_EVENT_LINKCHANGE_LP_CAP_LBN 0
#define        MCDI_EVENT_LINKCHANGE_LP_CAP_WIDTH 16
#define        MCDI_EVENT_LINKCHANGE_SPEED_LBN 16
#define        MCDI_EVENT_LINKCHANGE_SPEED_WIDTH 4
/* enum: 100Mbs */
#define          MCDI_EVENT_LINKCHANGE_SPEED_100M  0x1
/* enum: 1Gbs */
#define          MCDI_EVENT_LINKCHANGE_SPEED_1G  0x2
/* enum: 10Gbs */
#define          MCDI_EVENT_LINKCHANGE_SPEED_10G  0x3
/* enum: 40Gbs */
#define          MCDI_EVENT_LINKCHANGE_SPEED_40G  0x4
#define        MCDI_EVENT_LINKCHANGE_FCNTL_LBN 20
#define        MCDI_EVENT_LINKCHANGE_FCNTL_WIDTH 4
#define        MCDI_EVENT_LINKCHANGE_LINK_FLAGS_LBN 24
#define        MCDI_EVENT_LINKCHANGE_LINK_FLAGS_WIDTH 8
#define        MCDI_EVENT_SENSOREVT_MONITOR_LBN 0
#define        MCDI_EVENT_SENSOREVT_MONITOR_WIDTH 8
#define        MCDI_EVENT_SENSOREVT_STATE_LBN 8
#define        MCDI_EVENT_SENSOREVT_STATE_WIDTH 8
#define        MCDI_EVENT_SENSOREVT_VALUE_LBN 16
#define        MCDI_EVENT_SENSOREVT_VALUE_WIDTH 16
#define        MCDI_EVENT_FWALERT_DATA_LBN 8
#define        MCDI_EVENT_FWALERT_DATA_WIDTH 24
#define        MCDI_EVENT_FWALERT_REASON_LBN 0
#define        MCDI_EVENT_FWALERT_REASON_WIDTH 8
/* enum: SRAM Access. */
#define          MCDI_EVENT_FWALERT_REASON_SRAM_ACCESS 0x1
#define        MCDI_EVENT_FLR_VF_LBN 0
#define        MCDI_EVENT_FLR_VF_WIDTH 8
#define        MCDI_EVENT_TX_ERR_TXQ_LBN 0
#define        MCDI_EVENT_TX_ERR_TXQ_WIDTH 12
#define        MCDI_EVENT_TX_ERR_TYPE_LBN 12
#define        MCDI_EVENT_TX_ERR_TYPE_WIDTH 4
/* enum: Descriptor loader reported failure */
#define          MCDI_EVENT_TX_ERR_DL_FAIL 0x1
/* enum: Descriptor ring empty and no EOP seen for packet */
#define          MCDI_EVENT_TX_ERR_NO_EOP 0x2
/* enum: Overlength packet */
#define          MCDI_EVENT_TX_ERR_2BIG 0x3
/* enum: Malformed option descriptor */
#define          MCDI_EVENT_TX_BAD_OPTDESC 0x5
/* enum: Option descriptor part way through a packet */
#define          MCDI_EVENT_TX_OPT_IN_PKT 0x8
/* enum: DMA or PIO data access error */
#define          MCDI_EVENT_TX_ERR_BAD_DMA_OR_PIO 0x9
#define        MCDI_EVENT_TX_ERR_INFO_LBN 16
#define        MCDI_EVENT_TX_ERR_INFO_WIDTH 16
#define        MCDI_EVENT_TX_FLUSH_TO_DRIVER_LBN 12
#define        MCDI_EVENT_TX_FLUSH_TO_DRIVER_WIDTH 1
#define        MCDI_EVENT_TX_FLUSH_TXQ_LBN 0
#define        MCDI_EVENT_TX_FLUSH_TXQ_WIDTH 12
#define        MCDI_EVENT_PTP_ERR_TYPE_LBN 0
#define        MCDI_EVENT_PTP_ERR_TYPE_WIDTH 8
/* enum: PLL lost lock */
#define          MCDI_EVENT_PTP_ERR_PLL_LOST 0x1
/* enum: Filter overflow (PDMA) */
#define          MCDI_EVENT_PTP_ERR_FILTER 0x2
/* enum: FIFO overflow (FPGA) */
#define          MCDI_EVENT_PTP_ERR_FIFO 0x3
/* enum: Merge queue overflow */
#define          MCDI_EVENT_PTP_ERR_QUEUE 0x4
#define        MCDI_EVENT_AOE_ERR_TYPE_LBN 0
#define        MCDI_EVENT_AOE_ERR_TYPE_WIDTH 8
/* enum: AOE failed to load - no valid image? */
#define          MCDI_EVENT_AOE_NO_LOAD 0x1
/* enum: AOE FC reported an exception */
#define          MCDI_EVENT_AOE_FC_ASSERT 0x2
/* enum: AOE FC watchdogged */
#define          MCDI_EVENT_AOE_FC_WATCHDOG 0x3
/* enum: AOE FC failed to start */
#define          MCDI_EVENT_AOE_FC_NO_START 0x4
/* enum: Generic AOE fault - likely to have been reported via other means too
 * but intended for use by aoex driver.
 */
#define          MCDI_EVENT_AOE_FAULT 0x5
/* enum: Results of reprogramming the CPLD (status in AOE_ERR_DATA) */
#define          MCDI_EVENT_AOE_CPLD_REPROGRAMMED 0x6
/* enum: AOE loaded successfully */
#define          MCDI_EVENT_AOE_LOAD 0x7
/* enum: AOE DMA operation completed (LSB of HOST_HANDLE in AOE_ERR_DATA) */
#define          MCDI_EVENT_AOE_DMA 0x8
/* enum: AOE byteblaster connected/disconnected (Connection status in
 * AOE_ERR_DATA)
 */
#define          MCDI_EVENT_AOE_BYTEBLASTER 0x9
/* enum: DDR ECC status update */
#define          MCDI_EVENT_AOE_DDR_ECC_STATUS 0xa
/* enum: PTP status update */
#define          MCDI_EVENT_AOE_PTP_STATUS 0xb
/* enum: FPGA header incorrect */
#define          MCDI_EVENT_AOE_FPGA_LOAD_HEADER_ERR 0xc
/* enum: FPGA Powered Off due to error in powering up FPGA */
#define          MCDI_EVENT_AOE_FPGA_POWER_OFF 0xd
/* enum: AOE FPGA load failed due to MC to MUM communication failure */
#define          MCDI_EVENT_AOE_FPGA_LOAD_FAILED 0xe
/* enum: Notify that invalid flash type detected */
#define          MCDI_EVENT_AOE_INVALID_FPGA_FLASH_TYPE 0xf
/* enum: Notify that the attempt to run FPGA Controller firmware timedout */
#define          MCDI_EVENT_AOE_FC_RUN_TIMEDOUT 0x10
/* enum: Failure to probe one or more FPGA boot flash chips */
#define          MCDI_EVENT_AOE_FPGA_BOOT_FLASH_INVALID 0x11
/* enum: FPGA boot-flash contains an invalid image header */
#define          MCDI_EVENT_AOE_FPGA_BOOT_FLASH_HDR_INVALID 0x12
/* enum: Failed to program clocks required by the FPGA */
#define          MCDI_EVENT_AOE_FPGA_CLOCKS_PROGRAM_FAILED 0x13
/* enum: Notify that FPGA Controller is alive to serve MCDI requests */
#define          MCDI_EVENT_AOE_FC_RUNNING 0x14
#define        MCDI_EVENT_AOE_ERR_DATA_LBN 8
#define        MCDI_EVENT_AOE_ERR_DATA_WIDTH 8
#define        MCDI_EVENT_AOE_ERR_FC_ASSERT_INFO_LBN 8
#define        MCDI_EVENT_AOE_ERR_FC_ASSERT_INFO_WIDTH 8
/* enum: FC Assert happened, but the register information is not available */
#define          MCDI_EVENT_AOE_ERR_FC_ASSERT_SEEN 0x0
/* enum: The register information for FC Assert is ready for readinng by driver
 */
#define          MCDI_EVENT_AOE_ERR_FC_ASSERT_DATA_READY 0x1
#define        MCDI_EVENT_AOE_ERR_CODE_FPGA_HEADER_VERIFY_FAILED_LBN 8
#define        MCDI_EVENT_AOE_ERR_CODE_FPGA_HEADER_VERIFY_FAILED_WIDTH 8
/* enum: Reading from NV failed */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_NV_READ_FAIL 0x0
/* enum: Invalid Magic Number if FPGA header */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_MAGIC_FAIL 0x1
/* enum: Invalid Silicon type detected in header */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_SILICON_TYPE 0x2
/* enum: Unsupported VRatio */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_VRATIO 0x3
/* enum: Unsupported DDR Type */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_DDR_TYPE 0x4
/* enum: DDR Voltage out of supported range */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_DDR_VOLTAGE 0x5
/* enum: Unsupported DDR speed */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_DDR_SPEED 0x6
/* enum: Unsupported DDR size */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_DDR_SIZE 0x7
/* enum: Unsupported DDR rank */
#define          MCDI_EVENT_AOE_ERR_FPGA_HEADER_DDR_RANK 0x8
#define        MCDI_EVENT_AOE_ERR_CODE_INVALID_FPGA_FLASH_TYPE_INFO_LBN 8
#define        MCDI_EVENT_AOE_ERR_CODE_INVALID_FPGA_FLASH_TYPE_INFO_WIDTH 8
/* enum: Primary boot flash */
#define          MCDI_EVENT_AOE_FLASH_TYPE_BOOT_PRIMARY 0x0
/* enum: Secondary boot flash */
#define          MCDI_EVENT_AOE_FLASH_TYPE_BOOT_SECONDARY 0x1
#define        MCDI_EVENT_AOE_ERR_CODE_FPGA_POWER_OFF_LBN 8
#define        MCDI_EVENT_AOE_ERR_CODE_FPGA_POWER_OFF_WIDTH 8
#define        MCDI_EVENT_AOE_ERR_CODE_FPGA_LOAD_FAILED_LBN 8
#define        MCDI_EVENT_AOE_ERR_CODE_FPGA_LOAD_FAILED_WIDTH 8
#define        MCDI_EVENT_RX_ERR_RXQ_LBN 0
#define        MCDI_EVENT_RX_ERR_RXQ_WIDTH 12
#define        MCDI_EVENT_RX_ERR_TYPE_LBN 12
#define        MCDI_EVENT_RX_ERR_TYPE_WIDTH 4
#define        MCDI_EVENT_RX_ERR_INFO_LBN 16
#define        MCDI_EVENT_RX_ERR_INFO_WIDTH 16
#define        MCDI_EVENT_RX_FLUSH_TO_DRIVER_LBN 12
#define        MCDI_EVENT_RX_FLUSH_TO_DRIVER_WIDTH 1
#define        MCDI_EVENT_RX_FLUSH_RXQ_LBN 0
#define        MCDI_EVENT_RX_FLUSH_RXQ_WIDTH 12
#define        MCDI_EVENT_MC_REBOOT_COUNT_LBN 0
#define        MCDI_EVENT_MC_REBOOT_COUNT_WIDTH 16
#define        MCDI_EVENT_MUM_ERR_TYPE_LBN 0
#define        MCDI_EVENT_MUM_ERR_TYPE_WIDTH 8
/* enum: MUM failed to load - no valid image? */
#define          MCDI_EVENT_MUM_NO_LOAD 0x1
/* enum: MUM f/w reported an exception */
#define          MCDI_EVENT_MUM_ASSERT 0x2
/* enum: MUM not kicking watchdog */
#define          MCDI_EVENT_MUM_WATCHDOG 0x3
#define        MCDI_EVENT_MUM_ERR_DATA_LBN 8
#define        MCDI_EVENT_MUM_ERR_DATA_WIDTH 8
#define       MCDI_EVENT_DATA_LBN 0
#define       MCDI_EVENT_DATA_WIDTH 32
#define       MCDI_EVENT_SRC_LBN 36
#define       MCDI_EVENT_SRC_WIDTH 8
#define       MCDI_EVENT_EV_CODE_LBN 60
#define       MCDI_EVENT_EV_CODE_WIDTH 4
#define       MCDI_EVENT_CODE_LBN 44
#define       MCDI_EVENT_CODE_WIDTH 8
/* enum: Event generated by host software */
#define          MCDI_EVENT_SW_EVENT 0x0
/* enum: Bad assert. */
#define          MCDI_EVENT_CODE_BADSSERT 0x1
/* enum: PM Notice. */
#define          MCDI_EVENT_CODE_PMNOTICE 0x2
/* enum: Command done. */
#define          MCDI_EVENT_CODE_CMDDONE 0x3
/* enum: Link change. */
#define          MCDI_EVENT_CODE_LINKCHANGE 0x4
/* enum: Sensor Event. */
#define          MCDI_EVENT_CODE_SENSOREVT 0x5
/* enum: Schedule error. */
#define          MCDI_EVENT_CODE_SCHEDERR 0x6
/* enum: Reboot. */
#define          MCDI_EVENT_CODE_REBOOT 0x7
/* enum: Mac stats DMA. */
#define          MCDI_EVENT_CODE_MAC_STATS_DMA 0x8
/* enum: Firmware alert. */
#define          MCDI_EVENT_CODE_FWALERT 0x9
/* enum: Function level reset. */
#define          MCDI_EVENT_CODE_FLR 0xa
/* enum: Transmit error */
#define          MCDI_EVENT_CODE_TX_ERR 0xb
/* enum: Tx flush has completed */
#define          MCDI_EVENT_CODE_TX_FLUSH  0xc
/* enum: PTP packet received timestamp */
#define          MCDI_EVENT_CODE_PTP_RX  0xd
/* enum: PTP NIC failure */
#define          MCDI_EVENT_CODE_PTP_FAULT  0xe
/* enum: PTP PPS event */
#define          MCDI_EVENT_CODE_PTP_PPS  0xf
/* enum: Rx flush has completed */
#define          MCDI_EVENT_CODE_RX_FLUSH  0x10
/* enum: Receive error */
#define          MCDI_EVENT_CODE_RX_ERR 0x11
/* enum: AOE fault */
#define          MCDI_EVENT_CODE_AOE  0x12
/* enum: Network port calibration failed (VCAL). */
#define          MCDI_EVENT_CODE_VCAL_FAIL  0x13
/* enum: HW PPS event */
#define          MCDI_EVENT_CODE_HW_PPS  0x14
/* enum: The MC has rebooted (huntington and later, siena uses CODE_REBOOT and
 * a different format)
 */
#define          MCDI_EVENT_CODE_MC_REBOOT 0x15
/* enum: the MC has detected a parity error */
#define          MCDI_EVENT_CODE_PAR_ERR 0x16
/* enum: the MC has detected a correctable error */
#define          MCDI_EVENT_CODE_ECC_CORR_ERR 0x17
/* enum: the MC has detected an uncorrectable error */
#define          MCDI_EVENT_CODE_ECC_FATAL_ERR 0x18
/* enum: The MC has entered offline BIST mode */
#define          MCDI_EVENT_CODE_MC_BIST 0x19
/* enum: PTP tick event providing current NIC time */
#define          MCDI_EVENT_CODE_PTP_TIME 0x1a
/* enum: MUM fault */
#define          MCDI_EVENT_CODE_MUM 0x1b
/* enum: notify the designated PF of a new authorization request */
#define          MCDI_EVENT_CODE_PROXY_REQUEST 0x1c
/* enum: notify a function that awaits an authorization that its request has
 * been processed and it may now resend the command
 */
#define          MCDI_EVENT_CODE_PROXY_RESPONSE 0x1d
/* enum: Artificial event generated by host and posted via MC for test
 * purposes.
 */
#define          MCDI_EVENT_CODE_TESTGEN  0xfa
#define       MCDI_EVENT_CMDDONE_DATA_OFST 0
#define       MCDI_EVENT_CMDDONE_DATA_LBN 0
#define       MCDI_EVENT_CMDDONE_DATA_WIDTH 32
#define       MCDI_EVENT_LINKCHANGE_DATA_OFST 0
#define       MCDI_EVENT_LINKCHANGE_DATA_LBN 0
#define       MCDI_EVENT_LINKCHANGE_DATA_WIDTH 32
#define       MCDI_EVENT_SENSOREVT_DATA_OFST 0
#define       MCDI_EVENT_SENSOREVT_DATA_LBN 0
#define       MCDI_EVENT_SENSOREVT_DATA_WIDTH 32
#define       MCDI_EVENT_MAC_STATS_DMA_GENERATION_OFST 0
#define       MCDI_EVENT_MAC_STATS_DMA_GENERATION_LBN 0
#define       MCDI_EVENT_MAC_STATS_DMA_GENERATION_WIDTH 32
#define       MCDI_EVENT_TX_ERR_DATA_OFST 0
#define       MCDI_EVENT_TX_ERR_DATA_LBN 0
#define       MCDI_EVENT_TX_ERR_DATA_WIDTH 32
/* For CODE_PTP_RX, CODE_PTP_PPS and CODE_HW_PPS events the seconds field of
 * timestamp
 */
#define       MCDI_EVENT_PTP_SECONDS_OFST 0
#define       MCDI_EVENT_PTP_SECONDS_LBN 0
#define       MCDI_EVENT_PTP_SECONDS_WIDTH 32
/* For CODE_PTP_RX, CODE_PTP_PPS and CODE_HW_PPS events the major field of
 * timestamp
 */
#define       MCDI_EVENT_PTP_MAJOR_OFST 0
#define       MCDI_EVENT_PTP_MAJOR_LBN 0
#define       MCDI_EVENT_PTP_MAJOR_WIDTH 32
/* For CODE_PTP_RX, CODE_PTP_PPS and CODE_HW_PPS events the nanoseconds field
 * of timestamp
 */
#define       MCDI_EVENT_PTP_NANOSECONDS_OFST 0
#define       MCDI_EVENT_PTP_NANOSECONDS_LBN 0
#define       MCDI_EVENT_PTP_NANOSECONDS_WIDTH 32
/* For CODE_PTP_RX, CODE_PTP_PPS and CODE_HW_PPS events the minor field of
 * timestamp
 */
#define       MCDI_EVENT_PTP_MINOR_OFST 0
#define       MCDI_EVENT_PTP_MINOR_LBN 0
#define       MCDI_EVENT_PTP_MINOR_WIDTH 32
/* For CODE_PTP_RX events, the lowest four bytes of sourceUUID from PTP packet
 */
#define       MCDI_EVENT_PTP_UUID_OFST 0
#define       MCDI_EVENT_PTP_UUID_LBN 0
#define       MCDI_EVENT_PTP_UUID_WIDTH 32
#define       MCDI_EVENT_RX_ERR_DATA_OFST 0
#define       MCDI_EVENT_RX_ERR_DATA_LBN 0
#define       MCDI_EVENT_RX_ERR_DATA_WIDTH 32
#define       MCDI_EVENT_PAR_ERR_DATA_OFST 0
#define       MCDI_EVENT_PAR_ERR_DATA_LBN 0
#define       MCDI_EVENT_PAR_ERR_DATA_WIDTH 32
#define       MCDI_EVENT_ECC_CORR_ERR_DATA_OFST 0
#define       MCDI_EVENT_ECC_CORR_ERR_DATA_LBN 0
#define       MCDI_EVENT_ECC_CORR_ERR_DATA_WIDTH 32
#define       MCDI_EVENT_ECC_FATAL_ERR_DATA_OFST 0
#define       MCDI_EVENT_ECC_FATAL_ERR_DATA_LBN 0
#define       MCDI_EVENT_ECC_FATAL_ERR_DATA_WIDTH 32
/* For CODE_PTP_TIME events, the major value of the PTP clock */
#define       MCDI_EVENT_PTP_TIME_MAJOR_OFST 0
#define       MCDI_EVENT_PTP_TIME_MAJOR_LBN 0
#define       MCDI_EVENT_PTP_TIME_MAJOR_WIDTH 32
/* For CODE_PTP_TIME events, bits 19-26 of the minor value of the PTP clock */
#define       MCDI_EVENT_PTP_TIME_MINOR_26_19_LBN 36
#define       MCDI_EVENT_PTP_TIME_MINOR_26_19_WIDTH 8
/* For CODE_PTP_TIME events where report sync status is enabled, indicates
 * whether the NIC clock has ever been set
 */
#define       MCDI_EVENT_PTP_TIME_NIC_CLOCK_VALID_LBN 36
#define       MCDI_EVENT_PTP_TIME_NIC_CLOCK_VALID_WIDTH 1
/* For CODE_PTP_TIME events where report sync status is enabled, indicates
 * whether the NIC and System clocks are in sync
 */
#define       MCDI_EVENT_PTP_TIME_HOST_NIC_IN_SYNC_LBN 37
#define       MCDI_EVENT_PTP_TIME_HOST_NIC_IN_SYNC_WIDTH 1
/* For CODE_PTP_TIME events where report sync status is enabled, bits 21-26 of
 * the minor value of the PTP clock
 */
#define       MCDI_EVENT_PTP_TIME_MINOR_26_21_LBN 38
#define       MCDI_EVENT_PTP_TIME_MINOR_26_21_WIDTH 6
#define       MCDI_EVENT_PROXY_REQUEST_BUFF_INDEX_OFST 0
#define       MCDI_EVENT_PROXY_REQUEST_BUFF_INDEX_LBN 0
#define       MCDI_EVENT_PROXY_REQUEST_BUFF_INDEX_WIDTH 32
#define       MCDI_EVENT_PROXY_RESPONSE_HANDLE_OFST 0
#define       MCDI_EVENT_PROXY_RESPONSE_HANDLE_LBN 0
#define       MCDI_EVENT_PROXY_RESPONSE_HANDLE_WIDTH 32
/* Zero means that the request has been completed or authorized, and the driver
 * should resend it. A non-zero value means that the authorization has been
 * denied, and gives the reason. Typically it will be EPERM.
 */
#define       MCDI_EVENT_PROXY_RESPONSE_RC_LBN 36
#define       MCDI_EVENT_PROXY_RESPONSE_RC_WIDTH 8

/* EVB_PORT_ID structuredef */
#define    EVB_PORT_ID_LEN 4
#define       EVB_PORT_ID_PORT_ID_OFST 0
/* enum: An invalid port handle. */
#define          EVB_PORT_ID_NULL  0x0
/* enum: The port assigned to this function.. */
#define          EVB_PORT_ID_ASSIGNED  0x1000000
/* enum: External network port 0 */
#define          EVB_PORT_ID_MAC0  0x2000000
/* enum: External network port 1 */
#define          EVB_PORT_ID_MAC1  0x2000001
/* enum: External network port 2 */
#define          EVB_PORT_ID_MAC2  0x2000002
/* enum: External network port 3 */
#define          EVB_PORT_ID_MAC3  0x2000003
#define       EVB_PORT_ID_PORT_ID_LBN 0
#define       EVB_PORT_ID_PORT_ID_WIDTH 32


/***********************************/
/* MC_CMD_DRV_ATTACH
 * Inform MCPU that this port is managed on the host (i.e. driver active). For
 * Huntington, also request the preferred datapath firmware to use if possible
 * (it may not be possible for this request to be fulfilled; the driver must
 * issue a subsequent MC_CMD_GET_CAPABILITIES command to determine which
 * features are actually available). The FIRMWARE_ID field is ignored by older
 * platforms.
 */
#define MC_CMD_DRV_ATTACH 0x1c
#undef MC_CMD_0x1c_PRIVILEGE_CTG

#define MC_CMD_0x1c_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_DRV_ATTACH_IN msgrequest */
#define    MC_CMD_DRV_ATTACH_IN_LEN 12
/* new state to set if UPDATE=1 */
#define       MC_CMD_DRV_ATTACH_IN_NEW_STATE_OFST 0
#define        MC_CMD_DRV_ATTACH_LBN 0
#define        MC_CMD_DRV_ATTACH_WIDTH 1
#define        MC_CMD_DRV_PREBOOT_LBN 1
#define        MC_CMD_DRV_PREBOOT_WIDTH 1
/* 1 to set new state, or 0 to just report the existing state */
#define       MC_CMD_DRV_ATTACH_IN_UPDATE_OFST 4
/* preferred datapath firmware (for Huntington; ignored for Siena) */
#define       MC_CMD_DRV_ATTACH_IN_FIRMWARE_ID_OFST 8
/* enum: Prefer to use full featured firmware */
#define          MC_CMD_FW_FULL_FEATURED 0x0
/* enum: Prefer to use firmware with fewer features but lower latency */
#define          MC_CMD_FW_LOW_LATENCY 0x1
/* enum: Prefer to use firmware for SolarCapture packed stream mode */
#define          MC_CMD_FW_PACKED_STREAM 0x2
/* enum: Prefer to use firmware with fewer features and simpler TX event
 * batching but higher TX packet rate
 */
#define          MC_CMD_FW_HIGH_TX_RATE 0x3
/* enum: Reserved value */
#define          MC_CMD_FW_PACKED_STREAM_HASH_MODE_1 0x4
/* enum: Prefer to use firmware with additional "rules engine" filtering
 * support
 */
#define          MC_CMD_FW_RULES_ENGINE 0x5
/* enum: Only this option is allowed for non-admin functions */
#define          MC_CMD_FW_DONT_CARE  0xffffffff

/* MC_CMD_DRV_ATTACH_OUT msgresponse */
#define    MC_CMD_DRV_ATTACH_OUT_LEN 4
/* previous or existing state, see the bitmask at NEW_STATE */
#define       MC_CMD_DRV_ATTACH_OUT_OLD_STATE_OFST 0

/* MC_CMD_DRV_ATTACH_EXT_OUT msgresponse */
#define    MC_CMD_DRV_ATTACH_EXT_OUT_LEN 8
/* previous or existing state, see the bitmask at NEW_STATE */
#define       MC_CMD_DRV_ATTACH_EXT_OUT_OLD_STATE_OFST 0
/* Flags associated with this function */
#define       MC_CMD_DRV_ATTACH_EXT_OUT_FUNC_FLAGS_OFST 4
/* enum: Labels the lowest-numbered function visible to the OS */
#define          MC_CMD_DRV_ATTACH_EXT_OUT_FLAG_PRIMARY 0x0
/* enum: The function can control the link state of the physical port it is
 * bound to.
 */
#define          MC_CMD_DRV_ATTACH_EXT_OUT_FLAG_LINKCTRL 0x1
/* enum: The function can perform privileged operations */
#define          MC_CMD_DRV_ATTACH_EXT_OUT_FLAG_TRUSTED 0x2
/* enum: The function does not have an active port associated with it. The port
 * refers to the Sorrento external FPGA port.
 */
#define          MC_CMD_DRV_ATTACH_EXT_OUT_FLAG_NO_ACTIVE_PORT 0x3


/***********************************/
/* MC_CMD_ENTITY_RESET
 * Generic per-resource reset. There is no equivalent for per-board reset.
 * Locks required: None; Return code: 0, ETIME. NOTE: This command is an
 * extended version of the deprecated MC_CMD_PORT_RESET with added fields.
 */
#define MC_CMD_ENTITY_RESET 0x20
/*      MC_CMD_0x20_PRIVILEGE_CTG SRIOV_CTG_GENERAL */

/* MC_CMD_ENTITY_RESET_IN msgrequest */
#define    MC_CMD_ENTITY_RESET_IN_LEN 4
/* Optional flags field. Omitting this will perform a "legacy" reset action
 * (TBD).
 */
#define       MC_CMD_ENTITY_RESET_IN_FLAG_OFST 0
#define        MC_CMD_ENTITY_RESET_IN_FUNCTION_RESOURCE_RESET_LBN 0
#define        MC_CMD_ENTITY_RESET_IN_FUNCTION_RESOURCE_RESET_WIDTH 1

/* MC_CMD_ENTITY_RESET_OUT msgresponse */
#define    MC_CMD_ENTITY_RESET_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_PHY_CFG
 * Report PHY configuration. This guarantees to succeed even if the PHY is in a
 * 'zombie' state. Locks required: None
 */
#define MC_CMD_GET_PHY_CFG 0x24
#undef MC_CMD_0x24_PRIVILEGE_CTG

#define MC_CMD_0x24_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_GET_PHY_CFG_IN msgrequest */
#define    MC_CMD_GET_PHY_CFG_IN_LEN 0

/* MC_CMD_GET_PHY_CFG_OUT msgresponse */
#define    MC_CMD_GET_PHY_CFG_OUT_LEN 72
/* flags */
#define       MC_CMD_GET_PHY_CFG_OUT_FLAGS_OFST 0
#define        MC_CMD_GET_PHY_CFG_OUT_PRESENT_LBN 0
#define        MC_CMD_GET_PHY_CFG_OUT_PRESENT_WIDTH 1
#define        MC_CMD_GET_PHY_CFG_OUT_BIST_CABLE_SHORT_LBN 1
#define        MC_CMD_GET_PHY_CFG_OUT_BIST_CABLE_SHORT_WIDTH 1
#define        MC_CMD_GET_PHY_CFG_OUT_BIST_CABLE_LONG_LBN 2
#define        MC_CMD_GET_PHY_CFG_OUT_BIST_CABLE_LONG_WIDTH 1
#define        MC_CMD_GET_PHY_CFG_OUT_LOWPOWER_LBN 3
#define        MC_CMD_GET_PHY_CFG_OUT_LOWPOWER_WIDTH 1
#define        MC_CMD_GET_PHY_CFG_OUT_POWEROFF_LBN 4
#define        MC_CMD_GET_PHY_CFG_OUT_POWEROFF_WIDTH 1
#define        MC_CMD_GET_PHY_CFG_OUT_TXDIS_LBN 5
#define        MC_CMD_GET_PHY_CFG_OUT_TXDIS_WIDTH 1
#define        MC_CMD_GET_PHY_CFG_OUT_BIST_LBN 6
#define        MC_CMD_GET_PHY_CFG_OUT_BIST_WIDTH 1
/* ?? */
#define       MC_CMD_GET_PHY_CFG_OUT_TYPE_OFST 4
/* Bitmask of supported capabilities */
#define       MC_CMD_GET_PHY_CFG_OUT_SUPPORTED_CAP_OFST 8
#define        MC_CMD_PHY_CAP_10HDX_LBN 1
#define        MC_CMD_PHY_CAP_10HDX_WIDTH 1
#define        MC_CMD_PHY_CAP_10FDX_LBN 2
#define        MC_CMD_PHY_CAP_10FDX_WIDTH 1
#define        MC_CMD_PHY_CAP_100HDX_LBN 3
#define        MC_CMD_PHY_CAP_100HDX_WIDTH 1
#define        MC_CMD_PHY_CAP_100FDX_LBN 4
#define        MC_CMD_PHY_CAP_100FDX_WIDTH 1
#define        MC_CMD_PHY_CAP_1000HDX_LBN 5
#define        MC_CMD_PHY_CAP_1000HDX_WIDTH 1
#define        MC_CMD_PHY_CAP_1000FDX_LBN 6
#define        MC_CMD_PHY_CAP_1000FDX_WIDTH 1
#define        MC_CMD_PHY_CAP_10000FDX_LBN 7
#define        MC_CMD_PHY_CAP_10000FDX_WIDTH 1
#define        MC_CMD_PHY_CAP_PAUSE_LBN 8
#define        MC_CMD_PHY_CAP_PAUSE_WIDTH 1
#define        MC_CMD_PHY_CAP_ASYM_LBN 9
#define        MC_CMD_PHY_CAP_ASYM_WIDTH 1
#define        MC_CMD_PHY_CAP_AN_LBN 10
#define        MC_CMD_PHY_CAP_AN_WIDTH 1
#define        MC_CMD_PHY_CAP_40000FDX_LBN 11
#define        MC_CMD_PHY_CAP_40000FDX_WIDTH 1
#define        MC_CMD_PHY_CAP_DDM_LBN 12
#define        MC_CMD_PHY_CAP_DDM_WIDTH 1
#define        MC_CMD_PHY_CAP_100000FDX_LBN 13
#define        MC_CMD_PHY_CAP_100000FDX_WIDTH 1
#define        MC_CMD_PHY_CAP_25000FDX_LBN 14
#define        MC_CMD_PHY_CAP_25000FDX_WIDTH 1
#define        MC_CMD_PHY_CAP_50000FDX_LBN 15
#define        MC_CMD_PHY_CAP_50000FDX_WIDTH 1
#define        MC_CMD_PHY_CAP_BASER_FEC_LBN 16
#define        MC_CMD_PHY_CAP_BASER_FEC_WIDTH 1
#define        MC_CMD_PHY_CAP_BASER_FEC_REQ_LBN 17
#define        MC_CMD_PHY_CAP_BASER_FEC_REQ_WIDTH 1
#define        MC_CMD_PHY_CAP_RS_FEC_LBN 17
#define        MC_CMD_PHY_CAP_RS_FEC_WIDTH 1
#define        MC_CMD_PHY_CAP_RS_FEC_REQ_LBN 18
#define        MC_CMD_PHY_CAP_RS_FEC_REQ_WIDTH 1
/* ?? */
#define       MC_CMD_GET_PHY_CFG_OUT_CHANNEL_OFST 12
/* ?? */
#define       MC_CMD_GET_PHY_CFG_OUT_PRT_OFST 16
/* ?? */
#define       MC_CMD_GET_PHY_CFG_OUT_STATS_MASK_OFST 20
/* ?? */
#define       MC_CMD_GET_PHY_CFG_OUT_NAME_OFST 24
#define       MC_CMD_GET_PHY_CFG_OUT_NAME_LEN 20
/* ?? */
#define       MC_CMD_GET_PHY_CFG_OUT_MEDIA_TYPE_OFST 44
/* enum: Xaui. */
#define          MC_CMD_MEDIA_XAUI 0x1
/* enum: CX4. */
#define          MC_CMD_MEDIA_CX4 0x2
/* enum: KX4. */
#define          MC_CMD_MEDIA_KX4 0x3
/* enum: XFP Far. */
#define          MC_CMD_MEDIA_XFP 0x4
/* enum: SFP+. */
#define          MC_CMD_MEDIA_SFP_PLUS 0x5
/* enum: 10GBaseT. */
#define          MC_CMD_MEDIA_BASE_T 0x6
/* enum: QSFP+. */
#define          MC_CMD_MEDIA_QSFP_PLUS 0x7
#define       MC_CMD_GET_PHY_CFG_OUT_MMD_MASK_OFST 48
/* enum: Native clause 22 */
#define          MC_CMD_MMD_CLAUSE22 0x0
#define          MC_CMD_MMD_CLAUSE45_PMAPMD 0x1 /* enum */
#define          MC_CMD_MMD_CLAUSE45_WIS 0x2 /* enum */
#define          MC_CMD_MMD_CLAUSE45_PCS 0x3 /* enum */
#define          MC_CMD_MMD_CLAUSE45_PHYXS 0x4 /* enum */
#define          MC_CMD_MMD_CLAUSE45_DTEXS 0x5 /* enum */
#define          MC_CMD_MMD_CLAUSE45_TC 0x6 /* enum */
#define          MC_CMD_MMD_CLAUSE45_AN 0x7 /* enum */
/* enum: Clause22 proxied over clause45 by PHY. */
#define          MC_CMD_MMD_CLAUSE45_C22EXT 0x1d
#define          MC_CMD_MMD_CLAUSE45_VEND1 0x1e /* enum */
#define          MC_CMD_MMD_CLAUSE45_VEND2 0x1f /* enum */
#define       MC_CMD_GET_PHY_CFG_OUT_REVISION_OFST 52
#define       MC_CMD_GET_PHY_CFG_OUT_REVISION_LEN 20


/***********************************/
/* MC_CMD_GET_LINK
 * Read the unified MAC/PHY link state. Locks required: None Return code: 0,
 * ETIME.
 */
#define MC_CMD_GET_LINK 0x29
#undef MC_CMD_0x29_PRIVILEGE_CTG

#define MC_CMD_0x29_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_GET_LINK_IN msgrequest */
#define    MC_CMD_GET_LINK_IN_LEN 0

/* MC_CMD_GET_LINK_OUT msgresponse */
#define    MC_CMD_GET_LINK_OUT_LEN 28
/* near-side advertised capabilities */
#define       MC_CMD_GET_LINK_OUT_CAP_OFST 0
/* link-partner advertised capabilities */
#define       MC_CMD_GET_LINK_OUT_LP_CAP_OFST 4
/* Autonegotiated speed in mbit/s. The link may still be down even if this
 * reads non-zero.
 */
#define       MC_CMD_GET_LINK_OUT_LINK_SPEED_OFST 8
/* Current loopback setting. */
#define       MC_CMD_GET_LINK_OUT_LOOPBACK_MODE_OFST 12
/*            Enum values, see field(s): */
/*               MC_CMD_GET_LOOPBACK_MODES/MC_CMD_GET_LOOPBACK_MODES_OUT/100M */
#define       MC_CMD_GET_LINK_OUT_FLAGS_OFST 16
#define        MC_CMD_GET_LINK_OUT_LINK_UP_LBN 0
#define        MC_CMD_GET_LINK_OUT_LINK_UP_WIDTH 1
#define        MC_CMD_GET_LINK_OUT_FULL_DUPLEX_LBN 1
#define        MC_CMD_GET_LINK_OUT_FULL_DUPLEX_WIDTH 1
#define        MC_CMD_GET_LINK_OUT_BPX_LINK_LBN 2
#define        MC_CMD_GET_LINK_OUT_BPX_LINK_WIDTH 1
#define        MC_CMD_GET_LINK_OUT_PHY_LINK_LBN 3
#define        MC_CMD_GET_LINK_OUT_PHY_LINK_WIDTH 1
#define        MC_CMD_GET_LINK_OUT_LINK_FAULT_RX_LBN 6
#define        MC_CMD_GET_LINK_OUT_LINK_FAULT_RX_WIDTH 1
#define        MC_CMD_GET_LINK_OUT_LINK_FAULT_TX_LBN 7
#define        MC_CMD_GET_LINK_OUT_LINK_FAULT_TX_WIDTH 1
/* This returns the negotiated flow control value. */
#define       MC_CMD_GET_LINK_OUT_FCNTL_OFST 20
/*            Enum values, see field(s): */
/*               MC_CMD_SET_MAC/MC_CMD_SET_MAC_IN/FCNTL */
#define       MC_CMD_GET_LINK_OUT_MAC_FAULT_OFST 24
#define        MC_CMD_MAC_FAULT_XGMII_LOCAL_LBN 0
#define        MC_CMD_MAC_FAULT_XGMII_LOCAL_WIDTH 1
#define        MC_CMD_MAC_FAULT_XGMII_REMOTE_LBN 1
#define        MC_CMD_MAC_FAULT_XGMII_REMOTE_WIDTH 1
#define        MC_CMD_MAC_FAULT_SGMII_REMOTE_LBN 2
#define        MC_CMD_MAC_FAULT_SGMII_REMOTE_WIDTH 1
#define        MC_CMD_MAC_FAULT_PENDING_RECONFIG_LBN 3
#define        MC_CMD_MAC_FAULT_PENDING_RECONFIG_WIDTH 1


/***********************************/
/* MC_CMD_SET_MAC
 * Set MAC configuration. Locks required: None. Return code: 0, EINVAL
 */
#define MC_CMD_SET_MAC 0x2c
#undef MC_CMD_0x2c_PRIVILEGE_CTG

#define MC_CMD_0x2c_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_SET_MAC_IN msgrequest */
#define    MC_CMD_SET_MAC_IN_LEN 28
/* The MTU is the MTU programmed directly into the XMAC/GMAC (inclusive of
 * EtherII, VLAN, bug16011 padding).
 */
#define       MC_CMD_SET_MAC_IN_MTU_OFST 0
#define       MC_CMD_SET_MAC_IN_DRAIN_OFST 4
#define       MC_CMD_SET_MAC_IN_ADDR_OFST 8
#define       MC_CMD_SET_MAC_IN_ADDR_LEN 8
#define       MC_CMD_SET_MAC_IN_ADDR_LO_OFST 8
#define       MC_CMD_SET_MAC_IN_ADDR_HI_OFST 12
#define       MC_CMD_SET_MAC_IN_REJECT_OFST 16
#define        MC_CMD_SET_MAC_IN_REJECT_UNCST_LBN 0
#define        MC_CMD_SET_MAC_IN_REJECT_UNCST_WIDTH 1
#define        MC_CMD_SET_MAC_IN_REJECT_BRDCST_LBN 1
#define        MC_CMD_SET_MAC_IN_REJECT_BRDCST_WIDTH 1
#define       MC_CMD_SET_MAC_IN_FCNTL_OFST 20
/* enum: Flow control is off. */
#define          MC_CMD_FCNTL_OFF 0x0
/* enum: Respond to flow control. */
#define          MC_CMD_FCNTL_RESPOND 0x1
/* enum: Respond to and Issue flow control. */
#define          MC_CMD_FCNTL_BIDIR 0x2
/* enum: Auto neg flow control. */
#define          MC_CMD_FCNTL_AUTO 0x3
/* enum: Priority flow control (eftest builds only). */
#define          MC_CMD_FCNTL_QBB 0x4
/* enum: Issue flow control. */
#define          MC_CMD_FCNTL_GENERATE 0x5
#define       MC_CMD_SET_MAC_IN_FLAGS_OFST 24
#define        MC_CMD_SET_MAC_IN_FLAG_INCLUDE_FCS_LBN 0
#define        MC_CMD_SET_MAC_IN_FLAG_INCLUDE_FCS_WIDTH 1

/* MC_CMD_SET_MAC_EXT_IN msgrequest */
#define    MC_CMD_SET_MAC_EXT_IN_LEN 32
/* The MTU is the MTU programmed directly into the XMAC/GMAC (inclusive of
 * EtherII, VLAN, bug16011 padding).
 */
#define       MC_CMD_SET_MAC_EXT_IN_MTU_OFST 0
#define       MC_CMD_SET_MAC_EXT_IN_DRAIN_OFST 4
#define       MC_CMD_SET_MAC_EXT_IN_ADDR_OFST 8
#define       MC_CMD_SET_MAC_EXT_IN_ADDR_LEN 8
#define       MC_CMD_SET_MAC_EXT_IN_ADDR_LO_OFST 8
#define       MC_CMD_SET_MAC_EXT_IN_ADDR_HI_OFST 12
#define       MC_CMD_SET_MAC_EXT_IN_REJECT_OFST 16
#define        MC_CMD_SET_MAC_EXT_IN_REJECT_UNCST_LBN 0
#define        MC_CMD_SET_MAC_EXT_IN_REJECT_UNCST_WIDTH 1
#define        MC_CMD_SET_MAC_EXT_IN_REJECT_BRDCST_LBN 1
#define        MC_CMD_SET_MAC_EXT_IN_REJECT_BRDCST_WIDTH 1
#define       MC_CMD_SET_MAC_EXT_IN_FCNTL_OFST 20
/* enum: Flow control is off. */
/*               MC_CMD_FCNTL_OFF 0x0 */
/* enum: Respond to flow control. */
/*               MC_CMD_FCNTL_RESPOND 0x1 */
/* enum: Respond to and Issue flow control. */
/*               MC_CMD_FCNTL_BIDIR 0x2 */
/* enum: Auto neg flow control. */
/*               MC_CMD_FCNTL_AUTO 0x3 */
/* enum: Priority flow control (eftest builds only). */
/*               MC_CMD_FCNTL_QBB 0x4 */
/* enum: Issue flow control. */
/*               MC_CMD_FCNTL_GENERATE 0x5 */
#define       MC_CMD_SET_MAC_EXT_IN_FLAGS_OFST 24
#define        MC_CMD_SET_MAC_EXT_IN_FLAG_INCLUDE_FCS_LBN 0
#define        MC_CMD_SET_MAC_EXT_IN_FLAG_INCLUDE_FCS_WIDTH 1
/* Select which parameters to configure. A parameter will only be modified if
 * the corresponding control flag is set. If SET_MAC_ENHANCED is not set in
 * capabilities then this field is ignored (and all flags are assumed to be
 * set).
 */
#define       MC_CMD_SET_MAC_EXT_IN_CONTROL_OFST 28
#define        MC_CMD_SET_MAC_EXT_IN_CFG_MTU_LBN 0
#define        MC_CMD_SET_MAC_EXT_IN_CFG_MTU_WIDTH 1
#define        MC_CMD_SET_MAC_EXT_IN_CFG_DRAIN_LBN 1
#define        MC_CMD_SET_MAC_EXT_IN_CFG_DRAIN_WIDTH 1
#define        MC_CMD_SET_MAC_EXT_IN_CFG_REJECT_LBN 2
#define        MC_CMD_SET_MAC_EXT_IN_CFG_REJECT_WIDTH 1
#define        MC_CMD_SET_MAC_EXT_IN_CFG_FCNTL_LBN 3
#define        MC_CMD_SET_MAC_EXT_IN_CFG_FCNTL_WIDTH 1
#define        MC_CMD_SET_MAC_EXT_IN_CFG_FCS_LBN 4
#define        MC_CMD_SET_MAC_EXT_IN_CFG_FCS_WIDTH 1

/* MC_CMD_SET_MAC_OUT msgresponse */
#define    MC_CMD_SET_MAC_OUT_LEN 0

/* MC_CMD_SET_MAC_V2_OUT msgresponse */
#define    MC_CMD_SET_MAC_V2_OUT_LEN 4
/* MTU as configured after processing the request. See comment at
 * MC_CMD_SET_MAC_IN/MTU. To query MTU without doing any changes, set CONTROL
 * to 0.
 */
#define       MC_CMD_SET_MAC_V2_OUT_MTU_OFST 0


/***********************************/
/* MC_CMD_REBOOT
 * Reboot the MC.
 *
 * The AFTER_ASSERTION flag is intended to be used when the driver notices an
 * assertion failure (at which point it is expected to perform a complete tear
 * down and reinitialise), to allow both ports to reset the MC once in an
 * atomic fashion.
 *
 * Production mc firmwares are generally compiled with REBOOT_ON_ASSERT=1,
 * which means that they will automatically reboot out of the assertion
 * handler, so this is in practise an optional operation. It is still
 * recommended that drivers execute this to support custom firmwares with
 * REBOOT_ON_ASSERT=0.
 *
 * Locks required: NONE Returns: Nothing. You get back a response with ERR=1,
 * DATALEN=0
 */
#define MC_CMD_REBOOT 0x3d
#undef MC_CMD_0x3d_PRIVILEGE_CTG

#define MC_CMD_0x3d_PRIVILEGE_CTG SRIOV_CTG_ADMIN

/* MC_CMD_REBOOT_IN msgrequest */
#define    MC_CMD_REBOOT_IN_LEN 4
#define       MC_CMD_REBOOT_IN_FLAGS_OFST 0
#define          MC_CMD_REBOOT_FLAGS_AFTER_ASSERTION 0x1 /* enum */

/* MC_CMD_REBOOT_OUT msgresponse */
#define    MC_CMD_REBOOT_OUT_LEN 0


/***********************************/
/* MC_CMD_REBOOT_MODE
 * Set the mode for the next MC reboot. Locks required: NONE. Sets the reboot
 * mode to the specified value. Returns the old mode.
 */
#define MC_CMD_REBOOT_MODE 0x3f
#undef MC_CMD_0x3f_PRIVILEGE_CTG

#define MC_CMD_0x3f_PRIVILEGE_CTG SRIOV_CTG_ADMIN

/* MC_CMD_REBOOT_MODE_IN msgrequest */
#define    MC_CMD_REBOOT_MODE_IN_LEN 4
#define       MC_CMD_REBOOT_MODE_IN_VALUE_OFST 0
/* enum: Normal. */
#define          MC_CMD_REBOOT_MODE_NORMAL 0x0
/* enum: Power-on Reset. */
#define          MC_CMD_REBOOT_MODE_POR 0x2
/* enum: Snapper. */
#define          MC_CMD_REBOOT_MODE_SNAPPER 0x3
/* enum: snapper fake POR */
#define          MC_CMD_REBOOT_MODE_SNAPPER_POR 0x4
#define        MC_CMD_REBOOT_MODE_IN_FAKE_LBN 7
#define        MC_CMD_REBOOT_MODE_IN_FAKE_WIDTH 1

/* MC_CMD_REBOOT_MODE_OUT msgresponse */
#define    MC_CMD_REBOOT_MODE_OUT_LEN 4
#define       MC_CMD_REBOOT_MODE_OUT_VALUE_OFST 0


/***********************************/
/* MC_CMD_WORKAROUND
 * Enable/Disable a given workaround. The mcfw will return EINVAL if it doesn't
 * understand the given workaround number - which should not be treated as a
 * hard error by client code. This op does not imply any semantics about each
 * workaround, that's between the driver and the mcfw on a per-workaround
 * basis. Locks required: None. Returns: 0, EINVAL .
 */
#define MC_CMD_WORKAROUND 0x4a
#undef MC_CMD_0x4a_PRIVILEGE_CTG

#define MC_CMD_0x4a_PRIVILEGE_CTG SRIOV_CTG_ADMIN

/* MC_CMD_WORKAROUND_IN msgrequest */
#define    MC_CMD_WORKAROUND_IN_LEN 8
/* The enums here must correspond with those in MC_CMD_GET_WORKAROUND. */
#define       MC_CMD_WORKAROUND_IN_TYPE_OFST 0
/* enum: Bug 17230 work around. */
#define          MC_CMD_WORKAROUND_BUG17230 0x1
/* enum: Bug 35388 work around (unsafe EVQ writes). */
#define          MC_CMD_WORKAROUND_BUG35388 0x2
/* enum: Bug35017 workaround (A64 tables must be identity map) */
#define          MC_CMD_WORKAROUND_BUG35017 0x3
/* enum: Bug 41750 present (MC_CMD_TRIGGER_INTERRUPT won't work) */
#define          MC_CMD_WORKAROUND_BUG41750 0x4
/* enum: Bug 42008 present (Interrupts can overtake associated events). Caution
 * - before adding code that queries this workaround, remember that there's
 * released Monza firmware that doesn't understand MC_CMD_WORKAROUND_BUG42008,
 * and will hence (incorrectly) report that the bug doesn't exist.
 */
#define          MC_CMD_WORKAROUND_BUG42008 0x5
/* enum: Bug 26807 features present in firmware (multicast filter chaining)
 * This feature cannot be turned on/off while there are any filters already
 * present. The behaviour in such case depends on the acting client's privilege
 * level. If the client has the admin privilege, then all functions that have
 * filters installed will be FLRed and the FLR_DONE flag will be set. Otherwise
 * the command will fail with MC_CMD_ERR_FILTERS_PRESENT.
 */
#define          MC_CMD_WORKAROUND_BUG26807 0x6
/* enum: Bug 61265 work around (broken EVQ TMR writes). */
#define          MC_CMD_WORKAROUND_BUG61265 0x7
/* 0 = disable the workaround indicated by TYPE; any non-zero value = enable
 * the workaround
 */
#define       MC_CMD_WORKAROUND_IN_ENABLED_OFST 4

/* MC_CMD_WORKAROUND_OUT msgresponse */
#define    MC_CMD_WORKAROUND_OUT_LEN 0

/* MC_CMD_WORKAROUND_EXT_OUT msgresponse: This response format will be used
 * when (TYPE == MC_CMD_WORKAROUND_BUG26807)
 */
#define    MC_CMD_WORKAROUND_EXT_OUT_LEN 4
#define       MC_CMD_WORKAROUND_EXT_OUT_FLAGS_OFST 0
#define        MC_CMD_WORKAROUND_EXT_OUT_FLR_DONE_LBN 0
#define        MC_CMD_WORKAROUND_EXT_OUT_FLR_DONE_WIDTH 1


/***********************************/
/* MC_CMD_GET_MAC_ADDRESSES
 * Returns the base MAC, count and stride for the requesting function
 */
#define MC_CMD_GET_MAC_ADDRESSES 0x55
#undef MC_CMD_0x55_PRIVILEGE_CTG

#define MC_CMD_0x55_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_GET_MAC_ADDRESSES_IN msgrequest */
#define    MC_CMD_GET_MAC_ADDRESSES_IN_LEN 0

/* MC_CMD_GET_MAC_ADDRESSES_OUT msgresponse */
#define    MC_CMD_GET_MAC_ADDRESSES_OUT_LEN 16
/* Base MAC address */
#define       MC_CMD_GET_MAC_ADDRESSES_OUT_MAC_ADDR_BASE_OFST 0
#define       MC_CMD_GET_MAC_ADDRESSES_OUT_MAC_ADDR_BASE_LEN 6
/* Padding */
#define       MC_CMD_GET_MAC_ADDRESSES_OUT_RESERVED_OFST 6
#define       MC_CMD_GET_MAC_ADDRESSES_OUT_RESERVED_LEN 2
/* Number of allocated MAC addresses */
#define       MC_CMD_GET_MAC_ADDRESSES_OUT_MAC_COUNT_OFST 8
/* Spacing of allocated MAC addresses */
#define       MC_CMD_GET_MAC_ADDRESSES_OUT_MAC_STRIDE_OFST 12


/***********************************/
/* MC_CMD_GET_WORKAROUNDS
 * Read the list of all implemented and all currently enabled workarounds. The
 * enums here must correspond with those in MC_CMD_WORKAROUND.
 */
#define MC_CMD_GET_WORKAROUNDS 0x59
#undef MC_CMD_0x59_PRIVILEGE_CTG

#define MC_CMD_0x59_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_GET_WORKAROUNDS_OUT msgresponse */
#define    MC_CMD_GET_WORKAROUNDS_OUT_LEN 8
/* Each workaround is represented by a single bit according to the enums below.
 */
#define       MC_CMD_GET_WORKAROUNDS_OUT_IMPLEMENTED_OFST 0
#define       MC_CMD_GET_WORKAROUNDS_OUT_ENABLED_OFST 4
/* enum: Bug 17230 work around. */
#define          MC_CMD_GET_WORKAROUNDS_OUT_BUG17230 0x2
/* enum: Bug 35388 work around (unsafe EVQ writes). */
#define          MC_CMD_GET_WORKAROUNDS_OUT_BUG35388 0x4
/* enum: Bug35017 workaround (A64 tables must be identity map) */
#define          MC_CMD_GET_WORKAROUNDS_OUT_BUG35017 0x8
/* enum: Bug 41750 present (MC_CMD_TRIGGER_INTERRUPT won't work) */
#define          MC_CMD_GET_WORKAROUNDS_OUT_BUG41750 0x10
/* enum: Bug 42008 present (Interrupts can overtake associated events). Caution
 * - before adding code that queries this workaround, remember that there's
 * released Monza firmware that doesn't understand MC_CMD_WORKAROUND_BUG42008,
 * and will hence (incorrectly) report that the bug doesn't exist.
 */
#define          MC_CMD_GET_WORKAROUNDS_OUT_BUG42008 0x20
/* enum: Bug 26807 features present in firmware (multicast filter chaining) */
#define          MC_CMD_GET_WORKAROUNDS_OUT_BUG26807 0x40
/* enum: Bug 61265 work around (broken EVQ TMR writes). */
#define          MC_CMD_GET_WORKAROUNDS_OUT_BUG61265 0x80


/***********************************/
/* MC_CMD_V2_EXTN
 * Encapsulation for a v2 extended command
 */
#define MC_CMD_V2_EXTN 0x7f

/* MC_CMD_V2_EXTN_IN msgrequest */
#define    MC_CMD_V2_EXTN_IN_LEN 4
/* the extended command number */
#define       MC_CMD_V2_EXTN_IN_EXTENDED_CMD_LBN 0
#define       MC_CMD_V2_EXTN_IN_EXTENDED_CMD_WIDTH 15
#define       MC_CMD_V2_EXTN_IN_UNUSED_LBN 15
#define       MC_CMD_V2_EXTN_IN_UNUSED_WIDTH 1
/* the actual length of the encapsulated command (which is not in the v1
 * header)
 */
#define       MC_CMD_V2_EXTN_IN_ACTUAL_LEN_LBN 16
#define       MC_CMD_V2_EXTN_IN_ACTUAL_LEN_WIDTH 10
#define       MC_CMD_V2_EXTN_IN_UNUSED2_LBN 26
#define       MC_CMD_V2_EXTN_IN_UNUSED2_WIDTH 2
/* Type of command/response */
#define       MC_CMD_V2_EXTN_IN_MESSAGE_TYPE_LBN 28
#define       MC_CMD_V2_EXTN_IN_MESSAGE_TYPE_WIDTH 4
/* enum: MCDI command directed to or response originating from the MC. */
#define          MC_CMD_V2_EXTN_IN_MCDI_MESSAGE_TYPE_MC  0x0
/* enum: MCDI command directed to a TSA controller. MCDI responses of this type
 * are not defined.
 */
#define          MC_CMD_V2_EXTN_IN_MCDI_MESSAGE_TYPE_TSA  0x1


/***********************************/
/* MC_CMD_INIT_EVQ
 * Set up an event queue according to the supplied parameters. The IN arguments
 * end with an address for each 4k of host memory required to back the EVQ.
 */
#define MC_CMD_INIT_EVQ 0x80
#undef MC_CMD_0x80_PRIVILEGE_CTG

#define MC_CMD_0x80_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_INIT_EVQ_IN msgrequest */
#define    MC_CMD_INIT_EVQ_IN_LENMIN 44
#define    MC_CMD_INIT_EVQ_IN_LENMAX 548
#define    MC_CMD_INIT_EVQ_IN_LEN(num) (36+8*(num))
/* Size, in entries */
#define       MC_CMD_INIT_EVQ_IN_SIZE_OFST 0
/* Desired instance. Must be set to a specific instance, which is a function
 * local queue index.
 */
#define       MC_CMD_INIT_EVQ_IN_INSTANCE_OFST 4
/* The initial timer value. The load value is ignored if the timer mode is DIS.
 */
#define       MC_CMD_INIT_EVQ_IN_TMR_LOAD_OFST 8
/* The reload value is ignored in one-shot modes */
#define       MC_CMD_INIT_EVQ_IN_TMR_RELOAD_OFST 12
/* tbd */
#define       MC_CMD_INIT_EVQ_IN_FLAGS_OFST 16
#define        MC_CMD_INIT_EVQ_IN_FLAG_INTERRUPTING_LBN 0
#define        MC_CMD_INIT_EVQ_IN_FLAG_INTERRUPTING_WIDTH 1
#define        MC_CMD_INIT_EVQ_IN_FLAG_RPTR_DOS_LBN 1
#define        MC_CMD_INIT_EVQ_IN_FLAG_RPTR_DOS_WIDTH 1
#define        MC_CMD_INIT_EVQ_IN_FLAG_INT_ARMD_LBN 2
#define        MC_CMD_INIT_EVQ_IN_FLAG_INT_ARMD_WIDTH 1
#define        MC_CMD_INIT_EVQ_IN_FLAG_CUT_THRU_LBN 3
#define        MC_CMD_INIT_EVQ_IN_FLAG_CUT_THRU_WIDTH 1
#define        MC_CMD_INIT_EVQ_IN_FLAG_RX_MERGE_LBN 4
#define        MC_CMD_INIT_EVQ_IN_FLAG_RX_MERGE_WIDTH 1
#define        MC_CMD_INIT_EVQ_IN_FLAG_TX_MERGE_LBN 5
#define        MC_CMD_INIT_EVQ_IN_FLAG_TX_MERGE_WIDTH 1
#define        MC_CMD_INIT_EVQ_IN_FLAG_USE_TIMER_LBN 6
#define        MC_CMD_INIT_EVQ_IN_FLAG_USE_TIMER_WIDTH 1
#define       MC_CMD_INIT_EVQ_IN_TMR_MODE_OFST 20
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_IN_TMR_MODE_DIS 0x0
/* enum: Immediate */
#define          MC_CMD_INIT_EVQ_IN_TMR_IMMED_START 0x1
/* enum: Triggered */
#define          MC_CMD_INIT_EVQ_IN_TMR_TRIG_START 0x2
/* enum: Hold-off */
#define          MC_CMD_INIT_EVQ_IN_TMR_INT_HLDOFF 0x3
/* Target EVQ for wakeups if in wakeup mode. */
#define       MC_CMD_INIT_EVQ_IN_TARGET_EVQ_OFST 24
/* Target interrupt if in interrupting mode (note union with target EVQ). Use
 * MC_CMD_RESOURCE_INSTANCE_ANY unless a specific one required for test
 * purposes.
 */
#define       MC_CMD_INIT_EVQ_IN_IRQ_NUM_OFST 24
/* Event Counter Mode. */
#define       MC_CMD_INIT_EVQ_IN_COUNT_MODE_OFST 28
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_IN_COUNT_MODE_DIS 0x0
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_IN_COUNT_MODE_RX 0x1
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_IN_COUNT_MODE_TX 0x2
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_IN_COUNT_MODE_RXTX 0x3
/* Event queue packet count threshold. */
#define       MC_CMD_INIT_EVQ_IN_COUNT_THRSHLD_OFST 32
/* 64-bit address of 4k of 4k-aligned host memory buffer */
#define       MC_CMD_INIT_EVQ_IN_DMA_ADDR_OFST 36
#define       MC_CMD_INIT_EVQ_IN_DMA_ADDR_LEN 8
#define       MC_CMD_INIT_EVQ_IN_DMA_ADDR_LO_OFST 36
#define       MC_CMD_INIT_EVQ_IN_DMA_ADDR_HI_OFST 40
#define       MC_CMD_INIT_EVQ_IN_DMA_ADDR_MINNUM 1
#define       MC_CMD_INIT_EVQ_IN_DMA_ADDR_MAXNUM 64

/* MC_CMD_INIT_EVQ_OUT msgresponse */
#define    MC_CMD_INIT_EVQ_OUT_LEN 4
/* Only valid if INTRFLAG was true */
#define       MC_CMD_INIT_EVQ_OUT_IRQ_OFST 0

/* MC_CMD_INIT_EVQ_V2_IN msgrequest */
#define    MC_CMD_INIT_EVQ_V2_IN_LENMIN 44
#define    MC_CMD_INIT_EVQ_V2_IN_LENMAX 548
#define    MC_CMD_INIT_EVQ_V2_IN_LEN(num) (36+8*(num))
/* Size, in entries */
#define       MC_CMD_INIT_EVQ_V2_IN_SIZE_OFST 0
/* Desired instance. Must be set to a specific instance, which is a function
 * local queue index.
 */
#define       MC_CMD_INIT_EVQ_V2_IN_INSTANCE_OFST 4
/* The initial timer value. The load value is ignored if the timer mode is DIS.
 */
#define       MC_CMD_INIT_EVQ_V2_IN_TMR_LOAD_OFST 8
/* The reload value is ignored in one-shot modes */
#define       MC_CMD_INIT_EVQ_V2_IN_TMR_RELOAD_OFST 12
/* tbd */
#define       MC_CMD_INIT_EVQ_V2_IN_FLAGS_OFST 16
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_INTERRUPTING_LBN 0
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_INTERRUPTING_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_RPTR_DOS_LBN 1
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_RPTR_DOS_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_INT_ARMD_LBN 2
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_INT_ARMD_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_CUT_THRU_LBN 3
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_CUT_THRU_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_RX_MERGE_LBN 4
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_RX_MERGE_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_TX_MERGE_LBN 5
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_TX_MERGE_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_USE_TIMER_LBN 6
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_USE_TIMER_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_LBN 7
#define        MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_WIDTH 4
/* enum: All initialisation flags specified by host. */
#define          MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_MANUAL 0x0
/* enum: MEDFORD only. Certain initialisation flags specified by host may be
 * over-ridden by firmware based on licenses and firmware variant in order to
 * provide the lowest latency achievable. See
 * MC_CMD_INIT_EVQ_V2/MC_CMD_INIT_EVQ_V2_OUT/FLAGS for list of affected flags.
 */
#define          MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_LOW_LATENCY 0x1
/* enum: MEDFORD only. Certain initialisation flags specified by host may be
 * over-ridden by firmware based on licenses and firmware variant in order to
 * provide the best throughput achievable. See
 * MC_CMD_INIT_EVQ_V2/MC_CMD_INIT_EVQ_V2_OUT/FLAGS for list of affected flags.
 */
#define          MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_THROUGHPUT 0x2
/* enum: MEDFORD only. Certain initialisation flags may be over-ridden by
 * firmware based on licenses and firmware variant. See
 * MC_CMD_INIT_EVQ_V2/MC_CMD_INIT_EVQ_V2_OUT/FLAGS for list of affected flags.
 */
#define          MC_CMD_INIT_EVQ_V2_IN_FLAG_TYPE_AUTO 0x3
#define       MC_CMD_INIT_EVQ_V2_IN_TMR_MODE_OFST 20
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_V2_IN_TMR_MODE_DIS 0x0
/* enum: Immediate */
#define          MC_CMD_INIT_EVQ_V2_IN_TMR_IMMED_START 0x1
/* enum: Triggered */
#define          MC_CMD_INIT_EVQ_V2_IN_TMR_TRIG_START 0x2
/* enum: Hold-off */
#define          MC_CMD_INIT_EVQ_V2_IN_TMR_INT_HLDOFF 0x3
/* Target EVQ for wakeups if in wakeup mode. */
#define       MC_CMD_INIT_EVQ_V2_IN_TARGET_EVQ_OFST 24
/* Target interrupt if in interrupting mode (note union with target EVQ). Use
 * MC_CMD_RESOURCE_INSTANCE_ANY unless a specific one required for test
 * purposes.
 */
#define       MC_CMD_INIT_EVQ_V2_IN_IRQ_NUM_OFST 24
/* Event Counter Mode. */
#define       MC_CMD_INIT_EVQ_V2_IN_COUNT_MODE_OFST 28
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_V2_IN_COUNT_MODE_DIS 0x0
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_V2_IN_COUNT_MODE_RX 0x1
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_V2_IN_COUNT_MODE_TX 0x2
/* enum: Disabled */
#define          MC_CMD_INIT_EVQ_V2_IN_COUNT_MODE_RXTX 0x3
/* Event queue packet count threshold. */
#define       MC_CMD_INIT_EVQ_V2_IN_COUNT_THRSHLD_OFST 32
/* 64-bit address of 4k of 4k-aligned host memory buffer */
#define       MC_CMD_INIT_EVQ_V2_IN_DMA_ADDR_OFST 36
#define       MC_CMD_INIT_EVQ_V2_IN_DMA_ADDR_LEN 8
#define       MC_CMD_INIT_EVQ_V2_IN_DMA_ADDR_LO_OFST 36
#define       MC_CMD_INIT_EVQ_V2_IN_DMA_ADDR_HI_OFST 40
#define       MC_CMD_INIT_EVQ_V2_IN_DMA_ADDR_MINNUM 1
#define       MC_CMD_INIT_EVQ_V2_IN_DMA_ADDR_MAXNUM 64

/* MC_CMD_INIT_EVQ_V2_OUT msgresponse */
#define    MC_CMD_INIT_EVQ_V2_OUT_LEN 8
/* Only valid if INTRFLAG was true */
#define       MC_CMD_INIT_EVQ_V2_OUT_IRQ_OFST 0
/* Actual configuration applied on the card */
#define       MC_CMD_INIT_EVQ_V2_OUT_FLAGS_OFST 4
#define        MC_CMD_INIT_EVQ_V2_OUT_FLAG_CUT_THRU_LBN 0
#define        MC_CMD_INIT_EVQ_V2_OUT_FLAG_CUT_THRU_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_OUT_FLAG_RX_MERGE_LBN 1
#define        MC_CMD_INIT_EVQ_V2_OUT_FLAG_RX_MERGE_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_OUT_FLAG_TX_MERGE_LBN 2
#define        MC_CMD_INIT_EVQ_V2_OUT_FLAG_TX_MERGE_WIDTH 1
#define        MC_CMD_INIT_EVQ_V2_OUT_FLAG_RXQ_FORCE_EV_MERGING_LBN 3
#define        MC_CMD_INIT_EVQ_V2_OUT_FLAG_RXQ_FORCE_EV_MERGING_WIDTH 1

/* QUEUE_CRC_MODE structuredef */
#define    QUEUE_CRC_MODE_LEN 1
#define       QUEUE_CRC_MODE_MODE_LBN 0
#define       QUEUE_CRC_MODE_MODE_WIDTH 4
/* enum: No CRC. */
#define          QUEUE_CRC_MODE_NONE  0x0
/* enum: CRC Fiber channel over ethernet. */
#define          QUEUE_CRC_MODE_FCOE  0x1
/* enum: CRC (digest) iSCSI header only. */
#define          QUEUE_CRC_MODE_ISCSI_HDR  0x2
/* enum: CRC (digest) iSCSI header and payload. */
#define          QUEUE_CRC_MODE_ISCSI  0x3
/* enum: CRC Fiber channel over IP over ethernet. */
#define          QUEUE_CRC_MODE_FCOIPOE  0x4
/* enum: CRC MPA. */
#define          QUEUE_CRC_MODE_MPA  0x5
#define       QUEUE_CRC_MODE_SPARE_LBN 4
#define       QUEUE_CRC_MODE_SPARE_WIDTH 4


/***********************************/
/* MC_CMD_INIT_RXQ
 * set up a receive queue according to the supplied parameters. The IN
 * arguments end with an address for each 4k of host memory required to back
 * the RXQ.
 */
#define MC_CMD_INIT_RXQ 0x81
#undef MC_CMD_0x81_PRIVILEGE_CTG

#define MC_CMD_0x81_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_INIT_RXQ_IN msgrequest: Legacy RXQ_INIT request. Use extended version
 * in new code.
 */
#define    MC_CMD_INIT_RXQ_IN_LENMIN 36
#define    MC_CMD_INIT_RXQ_IN_LENMAX 252
#define    MC_CMD_INIT_RXQ_IN_LEN(num) (28+8*(num))
/* Size, in entries */
#define       MC_CMD_INIT_RXQ_IN_SIZE_OFST 0
/* The EVQ to send events to. This is an index originally specified to INIT_EVQ
 */
#define       MC_CMD_INIT_RXQ_IN_TARGET_EVQ_OFST 4
/* The value to put in the event data. Check hardware spec. for valid range. */
#define       MC_CMD_INIT_RXQ_IN_LABEL_OFST 8
/* Desired instance. Must be set to a specific instance, which is a function
 * local queue index.
 */
#define       MC_CMD_INIT_RXQ_IN_INSTANCE_OFST 12
/* There will be more flags here. */
#define       MC_CMD_INIT_RXQ_IN_FLAGS_OFST 16
#define        MC_CMD_INIT_RXQ_IN_FLAG_BUFF_MODE_LBN 0
#define        MC_CMD_INIT_RXQ_IN_FLAG_BUFF_MODE_WIDTH 1
#define        MC_CMD_INIT_RXQ_IN_FLAG_HDR_SPLIT_LBN 1
#define        MC_CMD_INIT_RXQ_IN_FLAG_HDR_SPLIT_WIDTH 1
#define        MC_CMD_INIT_RXQ_IN_FLAG_TIMESTAMP_LBN 2
#define        MC_CMD_INIT_RXQ_IN_FLAG_TIMESTAMP_WIDTH 1
#define        MC_CMD_INIT_RXQ_IN_CRC_MODE_LBN 3
#define        MC_CMD_INIT_RXQ_IN_CRC_MODE_WIDTH 4
#define        MC_CMD_INIT_RXQ_IN_FLAG_CHAIN_LBN 7
#define        MC_CMD_INIT_RXQ_IN_FLAG_CHAIN_WIDTH 1
#define        MC_CMD_INIT_RXQ_IN_FLAG_PREFIX_LBN 8
#define        MC_CMD_INIT_RXQ_IN_FLAG_PREFIX_WIDTH 1
#define        MC_CMD_INIT_RXQ_IN_FLAG_DISABLE_SCATTER_LBN 9
#define        MC_CMD_INIT_RXQ_IN_FLAG_DISABLE_SCATTER_WIDTH 1
#define        MC_CMD_INIT_RXQ_IN_UNUSED_LBN 10
#define        MC_CMD_INIT_RXQ_IN_UNUSED_WIDTH 1
/* Owner ID to use if in buffer mode (zero if physical) */
#define       MC_CMD_INIT_RXQ_IN_OWNER_ID_OFST 20
/* The port ID associated with the v-adaptor which should contain this DMAQ. */
#define       MC_CMD_INIT_RXQ_IN_PORT_ID_OFST 24
/* 64-bit address of 4k of 4k-aligned host memory buffer */
#define       MC_CMD_INIT_RXQ_IN_DMA_ADDR_OFST 28
#define       MC_CMD_INIT_RXQ_IN_DMA_ADDR_LEN 8
#define       MC_CMD_INIT_RXQ_IN_DMA_ADDR_LO_OFST 28
#define       MC_CMD_INIT_RXQ_IN_DMA_ADDR_HI_OFST 32
#define       MC_CMD_INIT_RXQ_IN_DMA_ADDR_MINNUM 1
#define       MC_CMD_INIT_RXQ_IN_DMA_ADDR_MAXNUM 28

/* MC_CMD_INIT_RXQ_EXT_IN msgrequest: Extended RXQ_INIT with additional mode
 * flags
 */
#define    MC_CMD_INIT_RXQ_EXT_IN_LEN 544
/* Size, in entries */
#define       MC_CMD_INIT_RXQ_EXT_IN_SIZE_OFST 0
/* The EVQ to send events to. This is an index originally specified to INIT_EVQ
 */
#define       MC_CMD_INIT_RXQ_EXT_IN_TARGET_EVQ_OFST 4
/* The value to put in the event data. Check hardware spec. for valid range. */
#define       MC_CMD_INIT_RXQ_EXT_IN_LABEL_OFST 8
/* Desired instance. Must be set to a specific instance, which is a function
 * local queue index.
 */
#define       MC_CMD_INIT_RXQ_EXT_IN_INSTANCE_OFST 12
/* There will be more flags here. */
#define       MC_CMD_INIT_RXQ_EXT_IN_FLAGS_OFST 16
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_BUFF_MODE_LBN 0
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_BUFF_MODE_WIDTH 1
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_HDR_SPLIT_LBN 1
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_HDR_SPLIT_WIDTH 1
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_TIMESTAMP_LBN 2
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_TIMESTAMP_WIDTH 1
#define        MC_CMD_INIT_RXQ_EXT_IN_CRC_MODE_LBN 3
#define        MC_CMD_INIT_RXQ_EXT_IN_CRC_MODE_WIDTH 4
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_CHAIN_LBN 7
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_CHAIN_WIDTH 1
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_PREFIX_LBN 8
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_PREFIX_WIDTH 1
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_DISABLE_SCATTER_LBN 9
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_DISABLE_SCATTER_WIDTH 1
#define        MC_CMD_INIT_RXQ_EXT_IN_DMA_MODE_LBN 10
#define        MC_CMD_INIT_RXQ_EXT_IN_DMA_MODE_WIDTH 4
/* enum: One packet per descriptor (for normal networking) */
#define          MC_CMD_INIT_RXQ_EXT_IN_SINGLE_PACKET  0x0
/* enum: Pack multiple packets into large descriptors (for SolarCapture) */
#define          MC_CMD_INIT_RXQ_EXT_IN_PACKED_STREAM  0x1
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_SNAPSHOT_MODE_LBN 14
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_SNAPSHOT_MODE_WIDTH 1
#define        MC_CMD_INIT_RXQ_EXT_IN_PACKED_STREAM_BUFF_SIZE_LBN 15
#define        MC_CMD_INIT_RXQ_EXT_IN_PACKED_STREAM_BUFF_SIZE_WIDTH 3
#define          MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_1M  0x0 /* enum */
#define          MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_512K  0x1 /* enum */
#define          MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_256K  0x2 /* enum */
#define          MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_128K  0x3 /* enum */
#define          MC_CMD_INIT_RXQ_EXT_IN_PS_BUFF_64K  0x4 /* enum */
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_WANT_OUTER_CLASSES_LBN 18
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_WANT_OUTER_CLASSES_WIDTH 1
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_FORCE_EV_MERGING_LBN 19
#define        MC_CMD_INIT_RXQ_EXT_IN_FLAG_FORCE_EV_MERGING_WIDTH 1
/* Owner ID to use if in buffer mode (zero if physical) */
#define       MC_CMD_INIT_RXQ_EXT_IN_OWNER_ID_OFST 20
/* The port ID associated with the v-adaptor which should contain this DMAQ. */
#define       MC_CMD_INIT_RXQ_EXT_IN_PORT_ID_OFST 24
/* 64-bit address of 4k of 4k-aligned host memory buffer */
#define       MC_CMD_INIT_RXQ_EXT_IN_DMA_ADDR_OFST 28
#define       MC_CMD_INIT_RXQ_EXT_IN_DMA_ADDR_LEN 8
#define       MC_CMD_INIT_RXQ_EXT_IN_DMA_ADDR_LO_OFST 28
#define       MC_CMD_INIT_RXQ_EXT_IN_DMA_ADDR_HI_OFST 32
#define       MC_CMD_INIT_RXQ_EXT_IN_DMA_ADDR_NUM 64
/* Maximum length of packet to receive, if SNAPSHOT_MODE flag is set */
#define       MC_CMD_INIT_RXQ_EXT_IN_SNAPSHOT_LENGTH_OFST 540

/* MC_CMD_INIT_RXQ_OUT msgresponse */
#define    MC_CMD_INIT_RXQ_OUT_LEN 0

/* MC_CMD_INIT_RXQ_EXT_OUT msgresponse */
#define    MC_CMD_INIT_RXQ_EXT_OUT_LEN 0


/***********************************/
/* MC_CMD_INIT_TXQ
 */
#define MC_CMD_INIT_TXQ 0x82
#undef MC_CMD_0x82_PRIVILEGE_CTG

#define MC_CMD_0x82_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_INIT_TXQ_IN msgrequest: Legacy INIT_TXQ request. Use extended version
 * in new code.
 */
#define    MC_CMD_INIT_TXQ_IN_LENMIN 36
#define    MC_CMD_INIT_TXQ_IN_LENMAX 252
#define    MC_CMD_INIT_TXQ_IN_LEN(num) (28+8*(num))
/* Size, in entries */
#define       MC_CMD_INIT_TXQ_IN_SIZE_OFST 0
/* The EVQ to send events to. This is an index originally specified to
 * INIT_EVQ.
 */
#define       MC_CMD_INIT_TXQ_IN_TARGET_EVQ_OFST 4
/* The value to put in the event data. Check hardware spec. for valid range. */
#define       MC_CMD_INIT_TXQ_IN_LABEL_OFST 8
/* Desired instance. Must be set to a specific instance, which is a function
 * local queue index.
 */
#define       MC_CMD_INIT_TXQ_IN_INSTANCE_OFST 12
/* There will be more flags here. */
#define       MC_CMD_INIT_TXQ_IN_FLAGS_OFST 16
#define        MC_CMD_INIT_TXQ_IN_FLAG_BUFF_MODE_LBN 0
#define        MC_CMD_INIT_TXQ_IN_FLAG_BUFF_MODE_WIDTH 1
#define        MC_CMD_INIT_TXQ_IN_FLAG_IP_CSUM_DIS_LBN 1
#define        MC_CMD_INIT_TXQ_IN_FLAG_IP_CSUM_DIS_WIDTH 1
#define        MC_CMD_INIT_TXQ_IN_FLAG_TCP_CSUM_DIS_LBN 2
#define        MC_CMD_INIT_TXQ_IN_FLAG_TCP_CSUM_DIS_WIDTH 1
#define        MC_CMD_INIT_TXQ_IN_FLAG_TCP_UDP_ONLY_LBN 3
#define        MC_CMD_INIT_TXQ_IN_FLAG_TCP_UDP_ONLY_WIDTH 1
#define        MC_CMD_INIT_TXQ_IN_CRC_MODE_LBN 4
#define        MC_CMD_INIT_TXQ_IN_CRC_MODE_WIDTH 4
#define        MC_CMD_INIT_TXQ_IN_FLAG_TIMESTAMP_LBN 8
#define        MC_CMD_INIT_TXQ_IN_FLAG_TIMESTAMP_WIDTH 1
#define        MC_CMD_INIT_TXQ_IN_FLAG_PACER_BYPASS_LBN 9
#define        MC_CMD_INIT_TXQ_IN_FLAG_PACER_BYPASS_WIDTH 1
#define        MC_CMD_INIT_TXQ_IN_FLAG_INNER_IP_CSUM_EN_LBN 10
#define        MC_CMD_INIT_TXQ_IN_FLAG_INNER_IP_CSUM_EN_WIDTH 1
#define        MC_CMD_INIT_TXQ_IN_FLAG_INNER_TCP_CSUM_EN_LBN 11
#define        MC_CMD_INIT_TXQ_IN_FLAG_INNER_TCP_CSUM_EN_WIDTH 1
/* Owner ID to use if in buffer mode (zero if physical) */
#define       MC_CMD_INIT_TXQ_IN_OWNER_ID_OFST 20
/* The port ID associated with the v-adaptor which should contain this DMAQ. */
#define       MC_CMD_INIT_TXQ_IN_PORT_ID_OFST 24
/* 64-bit address of 4k of 4k-aligned host memory buffer */
#define       MC_CMD_INIT_TXQ_IN_DMA_ADDR_OFST 28
#define       MC_CMD_INIT_TXQ_IN_DMA_ADDR_LEN 8
#define       MC_CMD_INIT_TXQ_IN_DMA_ADDR_LO_OFST 28
#define       MC_CMD_INIT_TXQ_IN_DMA_ADDR_HI_OFST 32
#define       MC_CMD_INIT_TXQ_IN_DMA_ADDR_MINNUM 1
#define       MC_CMD_INIT_TXQ_IN_DMA_ADDR_MAXNUM 28

/* MC_CMD_INIT_TXQ_EXT_IN msgrequest: Extended INIT_TXQ with additional mode
 * flags
 */
#define    MC_CMD_INIT_TXQ_EXT_IN_LEN 544
/* Size, in entries */
#define       MC_CMD_INIT_TXQ_EXT_IN_SIZE_OFST 0
/* The EVQ to send events to. This is an index originally specified to
 * INIT_EVQ.
 */
#define       MC_CMD_INIT_TXQ_EXT_IN_TARGET_EVQ_OFST 4
/* The value to put in the event data. Check hardware spec. for valid range. */
#define       MC_CMD_INIT_TXQ_EXT_IN_LABEL_OFST 8
/* Desired instance. Must be set to a specific instance, which is a function
 * local queue index.
 */
#define       MC_CMD_INIT_TXQ_EXT_IN_INSTANCE_OFST 12
/* There will be more flags here. */
#define       MC_CMD_INIT_TXQ_EXT_IN_FLAGS_OFST 16
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_BUFF_MODE_LBN 0
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_BUFF_MODE_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_IP_CSUM_DIS_LBN 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_IP_CSUM_DIS_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_TCP_CSUM_DIS_LBN 2
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_TCP_CSUM_DIS_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_TCP_UDP_ONLY_LBN 3
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_TCP_UDP_ONLY_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_CRC_MODE_LBN 4
#define        MC_CMD_INIT_TXQ_EXT_IN_CRC_MODE_WIDTH 4
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_TIMESTAMP_LBN 8
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_TIMESTAMP_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_PACER_BYPASS_LBN 9
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_PACER_BYPASS_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_INNER_IP_CSUM_EN_LBN 10
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_INNER_IP_CSUM_EN_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_INNER_TCP_CSUM_EN_LBN 11
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_INNER_TCP_CSUM_EN_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_TSOV2_EN_LBN 12
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_TSOV2_EN_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_CTPIO_LBN 13
#define        MC_CMD_INIT_TXQ_EXT_IN_FLAG_CTPIO_WIDTH 1
/* Owner ID to use if in buffer mode (zero if physical) */
#define       MC_CMD_INIT_TXQ_EXT_IN_OWNER_ID_OFST 20
/* The port ID associated with the v-adaptor which should contain this DMAQ. */
#define       MC_CMD_INIT_TXQ_EXT_IN_PORT_ID_OFST 24
/* 64-bit address of 4k of 4k-aligned host memory buffer */
#define       MC_CMD_INIT_TXQ_EXT_IN_DMA_ADDR_OFST 28
#define       MC_CMD_INIT_TXQ_EXT_IN_DMA_ADDR_LEN 8
#define       MC_CMD_INIT_TXQ_EXT_IN_DMA_ADDR_LO_OFST 28
#define       MC_CMD_INIT_TXQ_EXT_IN_DMA_ADDR_HI_OFST 32
#define       MC_CMD_INIT_TXQ_EXT_IN_DMA_ADDR_MINNUM 1
#define       MC_CMD_INIT_TXQ_EXT_IN_DMA_ADDR_MAXNUM 64
/* Flags related to Qbb flow control mode. */
#define       MC_CMD_INIT_TXQ_EXT_IN_QBB_FLAGS_OFST 540
#define        MC_CMD_INIT_TXQ_EXT_IN_QBB_ENABLE_LBN 0
#define        MC_CMD_INIT_TXQ_EXT_IN_QBB_ENABLE_WIDTH 1
#define        MC_CMD_INIT_TXQ_EXT_IN_QBB_PRIORITY_LBN 1
#define        MC_CMD_INIT_TXQ_EXT_IN_QBB_PRIORITY_WIDTH 3

/* MC_CMD_INIT_TXQ_OUT msgresponse */
#define    MC_CMD_INIT_TXQ_OUT_LEN 0


/***********************************/
/* MC_CMD_FINI_EVQ
 * Teardown an EVQ.
 *
 * All DMAQs or EVQs that point to the EVQ to tear down must be torn down first
 * or the operation will fail with EBUSY
 */
#define MC_CMD_FINI_EVQ 0x83
#undef MC_CMD_0x83_PRIVILEGE_CTG

#define MC_CMD_0x83_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_FINI_EVQ_IN msgrequest */
#define    MC_CMD_FINI_EVQ_IN_LEN 4
/* Instance of EVQ to destroy. Should be the same instance as that previously
 * passed to INIT_EVQ
 */
#define       MC_CMD_FINI_EVQ_IN_INSTANCE_OFST 0

/* MC_CMD_FINI_EVQ_OUT msgresponse */
#define    MC_CMD_FINI_EVQ_OUT_LEN 0


/***********************************/
/* MC_CMD_FINI_RXQ
 * Teardown a RXQ.
 */
#define MC_CMD_FINI_RXQ 0x84
#undef MC_CMD_0x84_PRIVILEGE_CTG

#define MC_CMD_0x84_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_FINI_RXQ_IN msgrequest */
#define    MC_CMD_FINI_RXQ_IN_LEN 4
/* Instance of RXQ to destroy */
#define       MC_CMD_FINI_RXQ_IN_INSTANCE_OFST 0

/* MC_CMD_FINI_RXQ_OUT msgresponse */
#define    MC_CMD_FINI_RXQ_OUT_LEN 0


/***********************************/
/* MC_CMD_FINI_TXQ
 * Teardown a TXQ.
 */
#define MC_CMD_FINI_TXQ 0x85
#undef MC_CMD_0x85_PRIVILEGE_CTG

#define MC_CMD_0x85_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_FINI_TXQ_IN msgrequest */
#define    MC_CMD_FINI_TXQ_IN_LEN 4
/* Instance of TXQ to destroy */
#define       MC_CMD_FINI_TXQ_IN_INSTANCE_OFST 0

/* MC_CMD_FINI_TXQ_OUT msgresponse */
#define    MC_CMD_FINI_TXQ_OUT_LEN 0


/***********************************/
/* MC_CMD_FILTER_OP
 * Multiplexed MCDI call for filter operations
 */
#define MC_CMD_FILTER_OP 0x8a
#undef MC_CMD_0x8a_PRIVILEGE_CTG

#define MC_CMD_0x8a_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_FILTER_OP_IN msgrequest */
#define    MC_CMD_FILTER_OP_IN_LEN 108
/* identifies the type of operation requested */
#define       MC_CMD_FILTER_OP_IN_OP_OFST 0
/* enum: single-recipient filter insert */
#define          MC_CMD_FILTER_OP_IN_OP_INSERT  0x0
/* enum: single-recipient filter remove */
#define          MC_CMD_FILTER_OP_IN_OP_REMOVE  0x1
/* enum: multi-recipient filter subscribe */
#define          MC_CMD_FILTER_OP_IN_OP_SUBSCRIBE  0x2
/* enum: multi-recipient filter unsubscribe */
#define          MC_CMD_FILTER_OP_IN_OP_UNSUBSCRIBE  0x3
/* enum: replace one recipient with another (warning - the filter handle may
 * change)
 */
#define          MC_CMD_FILTER_OP_IN_OP_REPLACE  0x4
/* filter handle (for remove / unsubscribe operations) */
#define       MC_CMD_FILTER_OP_IN_HANDLE_OFST 4
#define       MC_CMD_FILTER_OP_IN_HANDLE_LEN 8
#define       MC_CMD_FILTER_OP_IN_HANDLE_LO_OFST 4
#define       MC_CMD_FILTER_OP_IN_HANDLE_HI_OFST 8
/* The port ID associated with the v-adaptor which should contain this filter.
 */
#define       MC_CMD_FILTER_OP_IN_PORT_ID_OFST 12
/* fields to include in match criteria */
#define       MC_CMD_FILTER_OP_IN_MATCH_FIELDS_OFST 16
#define        MC_CMD_FILTER_OP_IN_MATCH_SRC_IP_LBN 0
#define        MC_CMD_FILTER_OP_IN_MATCH_SRC_IP_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_DST_IP_LBN 1
#define        MC_CMD_FILTER_OP_IN_MATCH_DST_IP_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_SRC_MAC_LBN 2
#define        MC_CMD_FILTER_OP_IN_MATCH_SRC_MAC_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_SRC_PORT_LBN 3
#define        MC_CMD_FILTER_OP_IN_MATCH_SRC_PORT_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_DST_MAC_LBN 4
#define        MC_CMD_FILTER_OP_IN_MATCH_DST_MAC_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_DST_PORT_LBN 5
#define        MC_CMD_FILTER_OP_IN_MATCH_DST_PORT_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_ETHER_TYPE_LBN 6
#define        MC_CMD_FILTER_OP_IN_MATCH_ETHER_TYPE_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_INNER_VLAN_LBN 7
#define        MC_CMD_FILTER_OP_IN_MATCH_INNER_VLAN_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_OUTER_VLAN_LBN 8
#define        MC_CMD_FILTER_OP_IN_MATCH_OUTER_VLAN_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_IP_PROTO_LBN 9
#define        MC_CMD_FILTER_OP_IN_MATCH_IP_PROTO_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_FWDEF0_LBN 10
#define        MC_CMD_FILTER_OP_IN_MATCH_FWDEF0_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_FWDEF1_LBN 11
#define        MC_CMD_FILTER_OP_IN_MATCH_FWDEF1_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_MCAST_DST_LBN 30
#define        MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_MCAST_DST_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_UCAST_DST_LBN 31
#define        MC_CMD_FILTER_OP_IN_MATCH_UNKNOWN_UCAST_DST_WIDTH 1
/* receive destination */
#define       MC_CMD_FILTER_OP_IN_RX_DEST_OFST 20
/* enum: drop packets */
#define          MC_CMD_FILTER_OP_IN_RX_DEST_DROP  0x0
/* enum: receive to host */
#define          MC_CMD_FILTER_OP_IN_RX_DEST_HOST  0x1
/* enum: receive to MC */
#define          MC_CMD_FILTER_OP_IN_RX_DEST_MC  0x2
/* enum: loop back to TXDP 0 */
#define          MC_CMD_FILTER_OP_IN_RX_DEST_TX0  0x3
/* enum: loop back to TXDP 1 */
#define          MC_CMD_FILTER_OP_IN_RX_DEST_TX1  0x4
/* receive queue handle (for multiple queue modes, this is the base queue) */
#define       MC_CMD_FILTER_OP_IN_RX_QUEUE_OFST 24
/* receive mode */
#define       MC_CMD_FILTER_OP_IN_RX_MODE_OFST 28
/* enum: receive to just the specified queue */
#define          MC_CMD_FILTER_OP_IN_RX_MODE_SIMPLE  0x0
/* enum: receive to multiple queues using RSS context */
#define          MC_CMD_FILTER_OP_IN_RX_MODE_RSS  0x1
/* enum: receive to multiple queues using .1p mapping */
#define          MC_CMD_FILTER_OP_IN_RX_MODE_DOT1P_MAPPING  0x2
/* enum: install a filter entry that will never match; for test purposes only
 */
#define          MC_CMD_FILTER_OP_IN_RX_MODE_TEST_NEVER_MATCH  0x80000000
/* RSS context (for RX_MODE_RSS) or .1p mapping handle (for
 * RX_MODE_DOT1P_MAPPING), as returned by MC_CMD_RSS_CONTEXT_ALLOC or
 * MC_CMD_DOT1P_MAPPING_ALLOC.
 */
#define       MC_CMD_FILTER_OP_IN_RX_CONTEXT_OFST 32
/* transmit domain (reserved; set to 0) */
#define       MC_CMD_FILTER_OP_IN_TX_DOMAIN_OFST 36
/* transmit destination (either set the MAC and/or PM bits for explicit
 * control, or set this field to TX_DEST_DEFAULT for sensible default
 * behaviour)
 */
#define       MC_CMD_FILTER_OP_IN_TX_DEST_OFST 40
/* enum: request default behaviour (based on filter type) */
#define          MC_CMD_FILTER_OP_IN_TX_DEST_DEFAULT  0xffffffff
#define        MC_CMD_FILTER_OP_IN_TX_DEST_MAC_LBN 0
#define        MC_CMD_FILTER_OP_IN_TX_DEST_MAC_WIDTH 1
#define        MC_CMD_FILTER_OP_IN_TX_DEST_PM_LBN 1
#define        MC_CMD_FILTER_OP_IN_TX_DEST_PM_WIDTH 1
/* source MAC address to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_IN_SRC_MAC_OFST 44
#define       MC_CMD_FILTER_OP_IN_SRC_MAC_LEN 6
/* source port to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_IN_SRC_PORT_OFST 50
#define       MC_CMD_FILTER_OP_IN_SRC_PORT_LEN 2
/* destination MAC address to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_IN_DST_MAC_OFST 52
#define       MC_CMD_FILTER_OP_IN_DST_MAC_LEN 6
/* destination port to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_IN_DST_PORT_OFST 58
#define       MC_CMD_FILTER_OP_IN_DST_PORT_LEN 2
/* Ethernet type to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_IN_ETHER_TYPE_OFST 60
#define       MC_CMD_FILTER_OP_IN_ETHER_TYPE_LEN 2
/* Inner VLAN tag to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_IN_INNER_VLAN_OFST 62
#define       MC_CMD_FILTER_OP_IN_INNER_VLAN_LEN 2
/* Outer VLAN tag to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_IN_OUTER_VLAN_OFST 64
#define       MC_CMD_FILTER_OP_IN_OUTER_VLAN_LEN 2
/* IP protocol to match (in low byte; set high byte to 0) */
#define       MC_CMD_FILTER_OP_IN_IP_PROTO_OFST 66
#define       MC_CMD_FILTER_OP_IN_IP_PROTO_LEN 2
/* Firmware defined register 0 to match (reserved; set to 0) */
#define       MC_CMD_FILTER_OP_IN_FWDEF0_OFST 68
/* Firmware defined register 1 to match (reserved; set to 0) */
#define       MC_CMD_FILTER_OP_IN_FWDEF1_OFST 72
/* source IP address to match (as bytes in network order; set last 12 bytes to
 * 0 for IPv4 address)
 */
#define       MC_CMD_FILTER_OP_IN_SRC_IP_OFST 76
#define       MC_CMD_FILTER_OP_IN_SRC_IP_LEN 16
/* destination IP address to match (as bytes in network order; set last 12
 * bytes to 0 for IPv4 address)
 */
#define       MC_CMD_FILTER_OP_IN_DST_IP_OFST 92
#define       MC_CMD_FILTER_OP_IN_DST_IP_LEN 16

/* MC_CMD_FILTER_OP_EXT_IN msgrequest: Extension to MC_CMD_FILTER_OP_IN to
 * include handling of VXLAN/NVGRE encapsulated frame filtering (which is
 * supported on Medford only).
 */
#define    MC_CMD_FILTER_OP_EXT_IN_LEN 172
/* identifies the type of operation requested */
#define       MC_CMD_FILTER_OP_EXT_IN_OP_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_FILTER_OP_IN/OP */
/* filter handle (for remove / unsubscribe operations) */
#define       MC_CMD_FILTER_OP_EXT_IN_HANDLE_OFST 4
#define       MC_CMD_FILTER_OP_EXT_IN_HANDLE_LEN 8
#define       MC_CMD_FILTER_OP_EXT_IN_HANDLE_LO_OFST 4
#define       MC_CMD_FILTER_OP_EXT_IN_HANDLE_HI_OFST 8
/* The port ID associated with the v-adaptor which should contain this filter.
 */
#define       MC_CMD_FILTER_OP_EXT_IN_PORT_ID_OFST 12
/* fields to include in match criteria */
#define       MC_CMD_FILTER_OP_EXT_IN_MATCH_FIELDS_OFST 16
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_SRC_IP_LBN 0
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_SRC_IP_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_DST_IP_LBN 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_DST_IP_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_SRC_MAC_LBN 2
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_SRC_MAC_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_SRC_PORT_LBN 3
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_SRC_PORT_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_DST_MAC_LBN 4
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_DST_MAC_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_DST_PORT_LBN 5
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_DST_PORT_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_ETHER_TYPE_LBN 6
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_ETHER_TYPE_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_INNER_VLAN_LBN 7
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_INNER_VLAN_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_OUTER_VLAN_LBN 8
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_OUTER_VLAN_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IP_PROTO_LBN 9
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IP_PROTO_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_FWDEF0_LBN 10
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_FWDEF0_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_VNI_OR_VSID_LBN 11
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_VNI_OR_VSID_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_SRC_IP_LBN 12
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_SRC_IP_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_DST_IP_LBN 13
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_DST_IP_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_SRC_MAC_LBN 14
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_SRC_MAC_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_SRC_PORT_LBN 15
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_SRC_PORT_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_DST_MAC_LBN 16
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_DST_MAC_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_DST_PORT_LBN 17
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_DST_PORT_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_ETHER_TYPE_LBN 18
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_ETHER_TYPE_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_INNER_VLAN_LBN 19
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_INNER_VLAN_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_OUTER_VLAN_LBN 20
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_OUTER_VLAN_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_IP_PROTO_LBN 21
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_IP_PROTO_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_FWDEF0_LBN 22
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_FWDEF0_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_FWDEF1_LBN 23
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_FWDEF1_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_UNKNOWN_MCAST_DST_LBN 24
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_UNKNOWN_MCAST_DST_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_UNKNOWN_UCAST_DST_LBN 25
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_IFRM_UNKNOWN_UCAST_DST_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_UNKNOWN_MCAST_DST_LBN 30
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_UNKNOWN_MCAST_DST_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_UNKNOWN_UCAST_DST_LBN 31
#define        MC_CMD_FILTER_OP_EXT_IN_MATCH_UNKNOWN_UCAST_DST_WIDTH 1
/* receive destination */
#define       MC_CMD_FILTER_OP_EXT_IN_RX_DEST_OFST 20
/* enum: drop packets */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_DEST_DROP  0x0
/* enum: receive to host */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_DEST_HOST  0x1
/* enum: receive to MC */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_DEST_MC  0x2
/* enum: loop back to TXDP 0 */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_DEST_TX0  0x3
/* enum: loop back to TXDP 1 */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_DEST_TX1  0x4
/* receive queue handle (for multiple queue modes, this is the base queue) */
#define       MC_CMD_FILTER_OP_EXT_IN_RX_QUEUE_OFST 24
/* receive mode */
#define       MC_CMD_FILTER_OP_EXT_IN_RX_MODE_OFST 28
/* enum: receive to just the specified queue */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_MODE_SIMPLE  0x0
/* enum: receive to multiple queues using RSS context */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_MODE_RSS  0x1
/* enum: receive to multiple queues using .1p mapping */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_MODE_DOT1P_MAPPING  0x2
/* enum: install a filter entry that will never match; for test purposes only
 */
#define          MC_CMD_FILTER_OP_EXT_IN_RX_MODE_TEST_NEVER_MATCH  0x80000000
/* RSS context (for RX_MODE_RSS) or .1p mapping handle (for
 * RX_MODE_DOT1P_MAPPING), as returned by MC_CMD_RSS_CONTEXT_ALLOC or
 * MC_CMD_DOT1P_MAPPING_ALLOC.
 */
#define       MC_CMD_FILTER_OP_EXT_IN_RX_CONTEXT_OFST 32
/* transmit domain (reserved; set to 0) */
#define       MC_CMD_FILTER_OP_EXT_IN_TX_DOMAIN_OFST 36
/* transmit destination (either set the MAC and/or PM bits for explicit
 * control, or set this field to TX_DEST_DEFAULT for sensible default
 * behaviour)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_TX_DEST_OFST 40
/* enum: request default behaviour (based on filter type) */
#define          MC_CMD_FILTER_OP_EXT_IN_TX_DEST_DEFAULT  0xffffffff
#define        MC_CMD_FILTER_OP_EXT_IN_TX_DEST_MAC_LBN 0
#define        MC_CMD_FILTER_OP_EXT_IN_TX_DEST_MAC_WIDTH 1
#define        MC_CMD_FILTER_OP_EXT_IN_TX_DEST_PM_LBN 1
#define        MC_CMD_FILTER_OP_EXT_IN_TX_DEST_PM_WIDTH 1
/* source MAC address to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_EXT_IN_SRC_MAC_OFST 44
#define       MC_CMD_FILTER_OP_EXT_IN_SRC_MAC_LEN 6
/* source port to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_EXT_IN_SRC_PORT_OFST 50
#define       MC_CMD_FILTER_OP_EXT_IN_SRC_PORT_LEN 2
/* destination MAC address to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_EXT_IN_DST_MAC_OFST 52
#define       MC_CMD_FILTER_OP_EXT_IN_DST_MAC_LEN 6
/* destination port to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_EXT_IN_DST_PORT_OFST 58
#define       MC_CMD_FILTER_OP_EXT_IN_DST_PORT_LEN 2
/* Ethernet type to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_EXT_IN_ETHER_TYPE_OFST 60
#define       MC_CMD_FILTER_OP_EXT_IN_ETHER_TYPE_LEN 2
/* Inner VLAN tag to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_EXT_IN_INNER_VLAN_OFST 62
#define       MC_CMD_FILTER_OP_EXT_IN_INNER_VLAN_LEN 2
/* Outer VLAN tag to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_EXT_IN_OUTER_VLAN_OFST 64
#define       MC_CMD_FILTER_OP_EXT_IN_OUTER_VLAN_LEN 2
/* IP protocol to match (in low byte; set high byte to 0) */
#define       MC_CMD_FILTER_OP_EXT_IN_IP_PROTO_OFST 66
#define       MC_CMD_FILTER_OP_EXT_IN_IP_PROTO_LEN 2
/* Firmware defined register 0 to match (reserved; set to 0) */
#define       MC_CMD_FILTER_OP_EXT_IN_FWDEF0_OFST 68
/* VNI (for VXLAN/Geneve, when IP protocol is UDP) or VSID (for NVGRE, when IP
 * protocol is GRE) to match (as bytes in network order; set last byte to 0 for
 * VXLAN/NVGRE, or 1 for Geneve)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_VNI_OR_VSID_OFST 72
#define        MC_CMD_FILTER_OP_EXT_IN_VNI_VALUE_LBN 0
#define        MC_CMD_FILTER_OP_EXT_IN_VNI_VALUE_WIDTH 24
#define        MC_CMD_FILTER_OP_EXT_IN_VNI_TYPE_LBN 24
#define        MC_CMD_FILTER_OP_EXT_IN_VNI_TYPE_WIDTH 8
/* enum: Match VXLAN traffic with this VNI */
#define          MC_CMD_FILTER_OP_EXT_IN_VNI_TYPE_VXLAN  0x0
/* enum: Match Geneve traffic with this VNI */
#define          MC_CMD_FILTER_OP_EXT_IN_VNI_TYPE_GENEVE  0x1
/* enum: Reserved for experimental development use */
#define          MC_CMD_FILTER_OP_EXT_IN_VNI_TYPE_EXPERIMENTAL  0xfe
#define        MC_CMD_FILTER_OP_EXT_IN_VSID_VALUE_LBN 0
#define        MC_CMD_FILTER_OP_EXT_IN_VSID_VALUE_WIDTH 24
#define        MC_CMD_FILTER_OP_EXT_IN_VSID_TYPE_LBN 24
#define        MC_CMD_FILTER_OP_EXT_IN_VSID_TYPE_WIDTH 8
/* enum: Match NVGRE traffic with this VSID */
#define          MC_CMD_FILTER_OP_EXT_IN_VSID_TYPE_NVGRE  0x0
/* source IP address to match (as bytes in network order; set last 12 bytes to
 * 0 for IPv4 address)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_SRC_IP_OFST 76
#define       MC_CMD_FILTER_OP_EXT_IN_SRC_IP_LEN 16
/* destination IP address to match (as bytes in network order; set last 12
 * bytes to 0 for IPv4 address)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_DST_IP_OFST 92
#define       MC_CMD_FILTER_OP_EXT_IN_DST_IP_LEN 16
/* VXLAN/NVGRE inner frame source MAC address to match (as bytes in network
 * order)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_SRC_MAC_OFST 108
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_SRC_MAC_LEN 6
/* VXLAN/NVGRE inner frame source port to match (as bytes in network order) */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_SRC_PORT_OFST 114
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_SRC_PORT_LEN 2
/* VXLAN/NVGRE inner frame destination MAC address to match (as bytes in
 * network order)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_DST_MAC_OFST 116
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_DST_MAC_LEN 6
/* VXLAN/NVGRE inner frame destination port to match (as bytes in network
 * order)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_DST_PORT_OFST 122
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_DST_PORT_LEN 2
/* VXLAN/NVGRE inner frame Ethernet type to match (as bytes in network order)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_ETHER_TYPE_OFST 124
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_ETHER_TYPE_LEN 2
/* VXLAN/NVGRE inner frame Inner VLAN tag to match (as bytes in network order)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_INNER_VLAN_OFST 126
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_INNER_VLAN_LEN 2
/* VXLAN/NVGRE inner frame Outer VLAN tag to match (as bytes in network order)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_OUTER_VLAN_OFST 128
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_OUTER_VLAN_LEN 2
/* VXLAN/NVGRE inner frame IP protocol to match (in low byte; set high byte to
 * 0)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_IP_PROTO_OFST 130
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_IP_PROTO_LEN 2
/* VXLAN/NVGRE inner frame Firmware defined register 0 to match (reserved; set
 * to 0)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_FWDEF0_OFST 132
/* VXLAN/NVGRE inner frame Firmware defined register 1 to match (reserved; set
 * to 0)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_FWDEF1_OFST 136
/* VXLAN/NVGRE inner frame source IP address to match (as bytes in network
 * order; set last 12 bytes to 0 for IPv4 address)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_SRC_IP_OFST 140
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_SRC_IP_LEN 16
/* VXLAN/NVGRE inner frame destination IP address to match (as bytes in network
 * order; set last 12 bytes to 0 for IPv4 address)
 */
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_DST_IP_OFST 156
#define       MC_CMD_FILTER_OP_EXT_IN_IFRM_DST_IP_LEN 16

/* MC_CMD_FILTER_OP_OUT msgresponse */
#define    MC_CMD_FILTER_OP_OUT_LEN 12
/* identifies the type of operation requested */
#define       MC_CMD_FILTER_OP_OUT_OP_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_FILTER_OP_IN/OP */
/* Returned filter handle (for insert / subscribe operations). Note that these
 * handles should be considered opaque to the host, although a value of
 * 0xFFFFFFFF_FFFFFFFF is guaranteed never to be a valid handle.
 */
#define       MC_CMD_FILTER_OP_OUT_HANDLE_OFST 4
#define       MC_CMD_FILTER_OP_OUT_HANDLE_LEN 8
#define       MC_CMD_FILTER_OP_OUT_HANDLE_LO_OFST 4
#define       MC_CMD_FILTER_OP_OUT_HANDLE_HI_OFST 8
/* enum: guaranteed invalid filter handle (low 32 bits) */
#define          MC_CMD_FILTER_OP_OUT_HANDLE_LO_INVALID  0xffffffff
/* enum: guaranteed invalid filter handle (high 32 bits) */
#define          MC_CMD_FILTER_OP_OUT_HANDLE_HI_INVALID  0xffffffff

/* MC_CMD_FILTER_OP_EXT_OUT msgresponse */
#define    MC_CMD_FILTER_OP_EXT_OUT_LEN 12
/* identifies the type of operation requested */
#define       MC_CMD_FILTER_OP_EXT_OUT_OP_OFST 0
/*            Enum values, see field(s): */
/*               MC_CMD_FILTER_OP_EXT_IN/OP */
/* Returned filter handle (for insert / subscribe operations). Note that these
 * handles should be considered opaque to the host, although a value of
 * 0xFFFFFFFF_FFFFFFFF is guaranteed never to be a valid handle.
 */
#define       MC_CMD_FILTER_OP_EXT_OUT_HANDLE_OFST 4
#define       MC_CMD_FILTER_OP_EXT_OUT_HANDLE_LEN 8
#define       MC_CMD_FILTER_OP_EXT_OUT_HANDLE_LO_OFST 4
#define       MC_CMD_FILTER_OP_EXT_OUT_HANDLE_HI_OFST 8
/*            Enum values, see field(s): */
/*               MC_CMD_FILTER_OP_OUT/HANDLE */


/***********************************/
/* MC_CMD_ALLOC_VIS
 * Allocate VIs for current PCI function.
 */
#define MC_CMD_ALLOC_VIS 0x8b
#undef MC_CMD_0x8b_PRIVILEGE_CTG

#define MC_CMD_0x8b_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_ALLOC_VIS_IN msgrequest */
#define    MC_CMD_ALLOC_VIS_IN_LEN 8
/* The minimum number of VIs that is acceptable */
#define       MC_CMD_ALLOC_VIS_IN_MIN_VI_COUNT_OFST 0
/* The maximum number of VIs that would be useful */
#define       MC_CMD_ALLOC_VIS_IN_MAX_VI_COUNT_OFST 4

/* MC_CMD_ALLOC_VIS_OUT msgresponse: Huntington-compatible VI_ALLOC request.
 * Use extended version in new code.
 */
#define    MC_CMD_ALLOC_VIS_OUT_LEN 8
/* The number of VIs allocated on this function */
#define       MC_CMD_ALLOC_VIS_OUT_VI_COUNT_OFST 0
/* The base absolute VI number allocated to this function. Required to
 * correctly interpret wakeup events.
 */
#define       MC_CMD_ALLOC_VIS_OUT_VI_BASE_OFST 4

/* MC_CMD_ALLOC_VIS_EXT_OUT msgresponse */
#define    MC_CMD_ALLOC_VIS_EXT_OUT_LEN 12
/* The number of VIs allocated on this function */
#define       MC_CMD_ALLOC_VIS_EXT_OUT_VI_COUNT_OFST 0
/* The base absolute VI number allocated to this function. Required to
 * correctly interpret wakeup events.
 */
#define       MC_CMD_ALLOC_VIS_EXT_OUT_VI_BASE_OFST 4
/* Function's port vi_shift value (always 0 on Huntington) */
#define       MC_CMD_ALLOC_VIS_EXT_OUT_VI_SHIFT_OFST 8


/***********************************/
/* MC_CMD_FREE_VIS
 * Free VIs for current PCI function. Any linked PIO buffers will be unlinked,
 * but not freed.
 */
#define MC_CMD_FREE_VIS 0x8c
#undef MC_CMD_0x8c_PRIVILEGE_CTG

#define MC_CMD_0x8c_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_FREE_VIS_IN msgrequest */
#define    MC_CMD_FREE_VIS_IN_LEN 0

/* MC_CMD_FREE_VIS_OUT msgresponse */
#define    MC_CMD_FREE_VIS_OUT_LEN 0


/***********************************/
/* MC_CMD_GET_PORT_ASSIGNMENT
 * Get port assignment for current PCI function.
 */
#define MC_CMD_GET_PORT_ASSIGNMENT 0xb8
#undef MC_CMD_0xb8_PRIVILEGE_CTG

#define MC_CMD_0xb8_PRIVILEGE_CTG SRIOV_CTG_GENERAL

/* MC_CMD_GET_PORT_ASSIGNMENT_IN msgrequest */
#define    MC_CMD_GET_PORT_ASSIGNMENT_IN_LEN 0

/* MC_CMD_GET_PORT_ASSIGNMENT_OUT msgresponse */
#define    MC_CMD_GET_PORT_ASSIGNMENT_OUT_LEN 4
/* Identifies the port assignment for this function. */
#define       MC_CMD_GET_PORT_ASSIGNMENT_OUT_PORT_OFST 0


/***********************************/
/* MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS
 * Configure UDP ports for tunnel encapsulation hardware acceleration. The
 * parser-dispatcher will attempt to parse traffic on these ports as tunnel
 * encapsulation PDUs and filter them using the tunnel encapsulation filter
 * chain rather than the standard filter chain. Note that this command can
 * cause all functions to see a reset. (Available on Medford only.)
 */
#define MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS 0x117
#undef MC_CMD_0x117_PRIVILEGE_CTG

#define MC_CMD_0x117_PRIVILEGE_CTG SRIOV_CTG_ADMIN

/* MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN msgrequest */
#define    MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_LENMIN 4
#define    MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_LENMAX 68
#define    MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_LEN(num) (4+4*(num))
/* Flags */
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_FLAGS_OFST 0
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_FLAGS_LEN 2
#define        MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_UNLOADING_LBN 0
#define        MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_UNLOADING_WIDTH 1
/* The number of entries in the ENTRIES array */
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_NUM_ENTRIES_OFST 2
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_NUM_ENTRIES_LEN 2
/* Entries defining the UDP port to protocol mapping, each laid out as a
 * TUNNEL_ENCAP_UDP_PORT_ENTRY
 */
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_ENTRIES_OFST 4
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_ENTRIES_LEN 4
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_ENTRIES_MINNUM 0
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_IN_ENTRIES_MAXNUM 16

/* MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT msgresponse */
#define    MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_LEN 2
/* Flags */
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_FLAGS_OFST 0
#define       MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_FLAGS_LEN 2
#define        MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_RESETTING_LBN 0
#define        MC_CMD_SET_TUNNEL_ENCAP_UDP_PORTS_OUT_RESETTING_WIDTH 1


#endif	/* SFC_MCDI_PCOL_H */
