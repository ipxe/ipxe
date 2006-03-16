/*
  This software is available to you under a choice of one of two
  licenses.  You may choose to be licensed under the terms of the GNU
  General Public License (GPL) Version 2, available at
  <http://www.fsf.org/copyleft/gpl.html>, or the OpenIB.org BSD
  license, available in the LICENSE.TXT file accompanying this
  software.  These details are also available at
  <http://openib.org/license.html>.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Copyright (c) 2004 Mellanox Technologies Ltd.  All rights reserved.
*/

/***
 *** This file was generated at "Tue Sep  6 09:14:00 2005"
 *** by:
 ***    % csp_bf -copyright=/mswg/misc/license-header.txt -prefix tavorprm_ -bits -fixnames MT23108_PRM.csp
 ***/

#ifndef H_prefix_tavorprm_bits_fixnames_MT23108_PRM_csp_H
#define H_prefix_tavorprm_bits_fixnames_MT23108_PRM_csp_H

#include "bit_ops.h"

/* Send doorbell */

struct tavorprm_send_doorbell_st {	/* Little Endian */
    pseudo_bit_t	nopcode[0x00005];      /* Opcode of descriptor to be executed */
    pseudo_bit_t	f[0x00001];            /* Fence bit. If set, descriptor is fenced */
    pseudo_bit_t	nda[0x0001a];          /* Bits 31:6 of descriptors virtual address */
/* -------------- */
    pseudo_bit_t	nds[0x00006];          /* Next descriptor size (in 16-byte chunks) */
    pseudo_bit_t	reserved0[0x00002];
    pseudo_bit_t	qpn[0x00018];          /* QP number this doorbell is rung on */
/* -------------- */
}; 

/* ACCESS_DDR_inject_errors_input_modifier */

struct tavorprm_access_ddr_inject_errors_input_modifier_st {	/* Little Endian */
    pseudo_bit_t	index3[0x00007];
    pseudo_bit_t	q3[0x00001];
    pseudo_bit_t	index2[0x00007];
    pseudo_bit_t	q2[0x00001];
    pseudo_bit_t	index1[0x00007];
    pseudo_bit_t	q1[0x00001];
    pseudo_bit_t	index0[0x00007];
    pseudo_bit_t	q0[0x00001];
/* -------------- */
}; 

/* ACCESS_DDR_inject_errors_input_parameter */

struct tavorprm_access_ddr_inject_errors_input_parameter_st {	/* Little Endian */
    pseudo_bit_t	ba[0x00002];           /* Bank Address */
    pseudo_bit_t	da[0x00002];           /* Dimm Address */
    pseudo_bit_t	reserved0[0x0001c];
/* -------------- */
    pseudo_bit_t	ra[0x00010];           /* Row Address */
    pseudo_bit_t	ca[0x00010];           /* Column Address */
/* -------------- */
}; 

/* Address Path */

struct tavorprm_address_path_st {	/* Little Endian */
    pseudo_bit_t	pkey_index[0x00007];   /* PKey table index */
    pseudo_bit_t	reserved0[0x00011];
    pseudo_bit_t	port_number[0x00002];  /* Specific port associated with this QP/EE.
                                                 1 - Port 1
                                                 2 - Port 2
                                                 other - reserved */
    pseudo_bit_t	reserved1[0x00006];
/* -------------- */
    pseudo_bit_t	rlid[0x00010];         /* Remote (Destination) LID */
    pseudo_bit_t	my_lid_path_bits[0x00007];/* Source LID - the lower 7 bits (upper bits are taken from PortInfo) */
    pseudo_bit_t	g[0x00001];            /* Global address enable - if set, GRH will be formed for packet header */
    pseudo_bit_t	reserved2[0x00005];
    pseudo_bit_t	rnr_retry[0x00003];    /* RNR retry count (see C9-132 in IB spec Vol 1)
                                                 0-6 - number of retries
                                                 7    - infinite */
/* -------------- */
    pseudo_bit_t	hop_limit[0x00008];    /* IPv6 hop limit */
    pseudo_bit_t	max_stat_rate[0x00003];/* Maximum static rate control. 
                                                 0 - 4X injection rate
                                                 1 - 1X injection rate
                                                 other - reserved
                                                  */
    pseudo_bit_t	reserved3[0x00005];
    pseudo_bit_t	mgid_index[0x00006];   /* Index to port GID table */
    pseudo_bit_t	reserved4[0x00005];
    pseudo_bit_t	ack_timeout[0x00005];  /* Local ACK timeout - Transport timer for activation of retransmission mechanism. Refer to IB spec Vol1 9.7.6.1.3 for further details.
                                                 The transport timer is set to 4.096us*2^ack_timeout, if ack_timeout is 0 then transport timer is disabled. */
/* -------------- */
    pseudo_bit_t	flow_label[0x00014];   /* IPv6 flow label */
    pseudo_bit_t	tclass[0x00008];       /* IPv6 TClass */
    pseudo_bit_t	sl[0x00004];           /* InfiniBand Service Level (SL) */
/* -------------- */
    pseudo_bit_t	rgid_127_96[0x00020];  /* Remote GID[127:96] */
/* -------------- */
    pseudo_bit_t	rgid_95_64[0x00020];   /* Remote GID[95:64] */
/* -------------- */
    pseudo_bit_t	rgid_63_32[0x00020];   /* Remote GID[63:32] */
/* -------------- */
    pseudo_bit_t	rgid_31_0[0x00020];    /* Remote GID[31:0] */
/* -------------- */
}; 

/* HCA Command Register (HCR) */

struct tavorprm_hca_command_register_st {	/* Little Endian */
    pseudo_bit_t	in_param_h[0x00020];   /* Input Parameter: parameter[63:32] or pointer[63:32] to input mailbox (see command description) */
/* -------------- */
    pseudo_bit_t	in_param_l[0x00020];   /* Input Parameter: parameter[31:0] or pointer[31:0] to input mailbox (see command description) */
/* -------------- */
    pseudo_bit_t	input_modifier[0x00020];/* Input Parameter Modifier */
/* -------------- */
    pseudo_bit_t	out_param_h[0x00020];  /* Output Parameter: parameter[63:32] or pointer[63:32] to output mailbox (see command description) */
/* -------------- */
    pseudo_bit_t	out_param_l[0x00020];  /* Output Parameter: parameter[31:0] or pointer[31:0] to output mailbox (see command description) */
/* -------------- */
    pseudo_bit_t	reserved0[0x00010];
    pseudo_bit_t	token[0x00010];        /* Software assigned token to the command, to uniquely identify it. The token is returned to the software in the EQE reported. */
/* -------------- */
    pseudo_bit_t	opcode[0x0000c];       /* Command opcode */
    pseudo_bit_t	opcode_modifier[0x00004];/* Opcode Modifier, see specific description for each command. */
    pseudo_bit_t	reserved1[0x00006];
    pseudo_bit_t	e[0x00001];            /* Event Request
                                                 0 - Don't report event (software will poll the GO bit)
                                                 1 - Report event to EQ when the command completes */
    pseudo_bit_t	go[0x00001];           /* Go (0=Software ownership for the HCR, 1=Hardware ownership for the HCR)
                                                 Software can write to the HCR only if Go bit is cleared.
                                                 Software must set the Go bit to trigger the HW to execute the command. Software must not write to this register value other than 1 for the Go bit. */
    pseudo_bit_t	status[0x00008];       /* Command execution status report. Valid only if command interface in under SW ownership (Go bit is cleared)
                                                 0 - command completed without error. If different than zero, command execution completed with error. Syndrom encoding is depended on command executed and is defined for each command */
/* -------------- */
}; 

/* EQ Doorbell */

struct tavorprm_eq_cmd_doorbell_st {	/* Little Endian */
    pseudo_bit_t	eqn[0x00006];          /* EQ accessed */
    pseudo_bit_t	reserved0[0x00012];
    pseudo_bit_t	eq_cmd[0x00008];       /* Command to be executed on EQ
                                                 01 - increment Consumer_indx by one
                                                 02 - Request notification for next event (Arm EQ)
                                                 03 - Disarm CQ (CQ number is specified in EQ_param)
                                                 04 - set Consumer_indx to value of EQ_param
                                                 05 - move EQ to Always Armed state
                                                 other - reserved */
/* -------------- */
    pseudo_bit_t	eq_param[0x00020];     /* parameter to be used by EQ commands 03 and 04. Reserved for other commands. */
/* -------------- */
}; 

/* CQ Doorbell */

struct tavorprm_cq_cmd_doorbell_st {	/* Little Endian */
    pseudo_bit_t	cqn[0x00018];          /* CQ number accessed */
    pseudo_bit_t	cq_cmd[0x00008];       /* Command to be executed on CQ
                                                 01 - Increment Consumer_indx by cq_param plus 1
                                                 02 - Request notification for next Solicited or Unsolicited completion event. CQ_param must contain last succesfully polled consumer index. For newly generated CQs the CQ_param should  contain (initial consumer index-1) modulu CQ size. When working with CQs with overrun detection, CQ_param can be set to 0xFFFFFFFF (HW will use the last polled index). 
                                                 03 - Request notification for next Solicited completion event CQ_param must contain last succesfully polled consumer index. For newly generated CQs the CQ_param should  contain (initial consumer index-1) modulu CQ size. When working with CQs with overrun detection, CQ_param can be set to 0xFFFFFFFF (HW will use the last polled index).
                                                 04 - Set Consumer_indx to value of CQ_param
                                                 05 - Request notification for multiple completions (see Advanced Topics chater)
                                                 other - reserved */
/* -------------- */
    pseudo_bit_t	cq_param[0x00020];     /* parameter to be used by CQ command */
/* -------------- */
}; 

/* Receive doorbell */

struct tavorprm_receive_doorbell_st {	/* Little Endian */
    pseudo_bit_t	nds[0x00006];          /* Next descriptor size (in 16-byte chunks)
                                                 Must be zero for SRQ doorbells */
    pseudo_bit_t	nda[0x0001a];          /* Bits 31:6 of descriptors virtual address */
/* -------------- */
    pseudo_bit_t	credits[0x00008];      /* Amount of credits ((length of the chain) posted with the doorbell on receive queue. Chain of up to 256 descriptors can be linked with single doorbell. Zero value in this field means 256. */
    pseudo_bit_t	qpn[0x00018];          /* QP number or SRQ number this doorbell is rung on */
/* -------------- */
}; 

/* RD-send doorbell */

struct tavorprm_rd_send_doorbell_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00008];
    pseudo_bit_t	een[0x00018];          /* End-to-end context number (reliable datagram)
                                                 Must be zero for Nop and Bind operations */
/* -------------- */
    pseudo_bit_t	reserved1[0x00008];
    pseudo_bit_t	qpn[0x00018];          /* QP number this doorbell is rung on */
/* -------------- */
    struct tavorprm_send_doorbell_st	snd_params;/* Send parameters */
/* -------------- */
}; 

/* Multicast Group Member QP */

struct tavorprm_mgmqp_st {	/* Little Endian */
    pseudo_bit_t	qpn_i[0x00018];        /* QPN_i: QP number which is a member in this multicast group. Valid only if Qi bit is set. Length of the QPN_i list is set in INIT_HCA */
    pseudo_bit_t	reserved0[0x00007];
    pseudo_bit_t	qi[0x00001];           /* Qi: QPN_i is valid */
/* -------------- */
}; 

/* vsd */

struct tavorprm_vsd_st {	/* Little Endian */
    pseudo_bit_t	vsd_dw0[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw1[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw2[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw3[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw4[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw5[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw6[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw7[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw8[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw9[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw10[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw11[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw12[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw13[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw14[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw15[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw16[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw17[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw18[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw19[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw20[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw21[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw22[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw23[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw24[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw25[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw26[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw27[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw28[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw29[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw30[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw31[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw32[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw33[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw34[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw35[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw36[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw37[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw38[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw39[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw40[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw41[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw42[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw43[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw44[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw45[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw46[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw47[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw48[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw49[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw50[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw51[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw52[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw53[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw54[0x00020];
/* -------------- */
    pseudo_bit_t	vsd_dw55[0x00020];
/* -------------- */
}; 

/* ACCESS_DDR_inject_errors */

struct tavorprm_access_ddr_inject_errors_st {	/* Little Endian */
    struct tavorprm_access_ddr_inject_errors_input_parameter_st	access_ddr_inject_errors_input_parameter;
/* -------------- */
    struct tavorprm_access_ddr_inject_errors_input_modifier_st	access_ddr_inject_errors_input_modifier;
/* -------------- */
    pseudo_bit_t	reserved0[0x00020];
/* -------------- */
}; 

/* Logical DIMM Information */

struct tavorprm_dimminfo_st {	/* Little Endian */
    pseudo_bit_t	dimmsize[0x00010];     /* Size of DIMM in units of 2^20 Bytes. This value is valid only when DIMMStatus is 0. */
    pseudo_bit_t	reserved0[0x00008];
    pseudo_bit_t	dimmstatus[0x00001];   /* DIMM Status
                                                 0 - Enabled
                                                 1 - Disabled
                                                  */
    pseudo_bit_t	dh[0x00001];           /* When set, the DIMM is Hidden and can not be accessed from the PCI bus. */
    pseudo_bit_t	wo[0x00001];           /* When set, the DIMM is write only.
                                                 If data integrity is configured (other than none), the DIMM must be
                                                 only targeted by write transactions where the address and size are multiples of 16 bytes. */
    pseudo_bit_t	reserved1[0x00005];
/* -------------- */
    pseudo_bit_t	spd[0x00001];          /* 0 - DIMM SPD was read from DIMM
                                                 1 - DIMM SPD was read from InfiniHost NVMEM */
    pseudo_bit_t	sladr[0x00003];        /* SPD Slave Address 3 LSBits. 
                                                 Valid only if spd bit is 0. */
    pseudo_bit_t	sock_num[0x00002];     /* DIMM socket number (for double sided DIMM one of the two numbers will be reported) */
    pseudo_bit_t	syn[0x00004];          /* Error syndrome (valid regardless of status value)
                                                 0 - DIMM has no error
                                                 1 - SPD error (e.g. checksum error, no response, error while reading)
                                                 2 - DIMM out of bounds (e.g. DIMM rows number is not between 7 and 14, DIMM type is not 2)
                                                 3 - DIMM conflict (e.g. mix of registered and unbuffered DIMMs, CAS latency conflict)
                                                 5 - DIMM size trimmed due to configuration (size exceeds)
                                                 other - Error, reserved
                                                  */
    pseudo_bit_t	reserved2[0x00016];
/* -------------- */
    pseudo_bit_t	vendor_id_h[0x00020];  /* JDEC Manufacturer ID[63:32] */
/* -------------- */
    pseudo_bit_t	vendor_id_l[0x00020];  /* JDEC Manufacturer ID[31:0] */
/* -------------- */
    pseudo_bit_t	dimm_start_adr_h[0x00020];/* DIMM memory start address [63:32].  This value is valid only when DIMMStatus is 0. */
/* -------------- */
    pseudo_bit_t	dimm_start_adr_l[0x00020];/* DIMM memory start address [31:0].  This value is valid only when DIMMStatus is 0. */
/* -------------- */
    pseudo_bit_t	reserved3[0x00040];
/* -------------- */
}; 

/* UAR Parameters */

struct tavorprm_uar_params_st {	/* Little Endian */
    pseudo_bit_t	uar_base_addr_h[0x00020];/* UAR Base Address [63:32] (QUERY_HCA only) */
/* -------------- */
    pseudo_bit_t	reserved0[0x00014];
    pseudo_bit_t	uar_base_addr_l[0x0000c];/* UAR Base Address [31:20] (QUERY_HCA only) */
/* -------------- */
    pseudo_bit_t	uar_page_sz[0x00008];  /* This field defines the size of each UAR page.
                                                 Size of UAR Page is 4KB*2^UAR_Page_Size */
    pseudo_bit_t	reserved1[0x00018];
/* -------------- */
    pseudo_bit_t	reserved2[0x00020];
/* -------------- */
    pseudo_bit_t	uar_scratch_base_addr_h[0x00020];/* Base address of UAR scratchpad [63:32].
                                                 Number of entries in table is UAR BAR size divided by UAR Page Size. 
                                                 Table must be aligned to entry size. */
/* -------------- */
    pseudo_bit_t	uar_scratch_base_addr_l[0x00020];/* Base address of UAR scratchpad [31:0].
                                                 Number of entries in table is UAR BAR size divided by UAR Page Size. 
                                                 Table must be aligned to entry size. */
/* -------------- */
    pseudo_bit_t	reserved3[0x00040];
/* -------------- */
}; 

/* Translation and Protection Tables Parameters */

struct tavorprm_tptparams_st {	/* Little Endian */
    pseudo_bit_t	mpt_base_adr_h[0x00020];/* MPT - Memory Protection Table base physical address [63:32].
                                                 Entry size is 64 bytes.
                                                 Table must be aligned to its size.
                                                 Address may be set to zero if address translation and protection is not supported. */
/* -------------- */
    pseudo_bit_t	mpt_base_adr_l[0x00020];/* MPT - Memory Protection Table base physical address [31:0].
                                                 Entry size is 64 bytes.
                                                 Table must be aligned to its size.
                                                 Address may be set to zero if address translation and protection is not supported. */
/* -------------- */
    pseudo_bit_t	log_mpt_sz[0x00006];   /* Log (base 2) of the number of region/windows entries in the MPT table. */
    pseudo_bit_t	reserved0[0x00002];
    pseudo_bit_t	pfto[0x00005];         /* Page Fault RNR Timeout  - 
                                                 The field returned in RNR Naks generated when a page fault is detected.
                                                 It has no effect when on-demand-paging is not used. */
    pseudo_bit_t	reserved1[0x00003];
    pseudo_bit_t	mtt_segment_size[0x00003];/* The size of MTT segment is 64*2^MTT_Segment_Size bytes */
    pseudo_bit_t	reserved2[0x0000d];
/* -------------- */
    pseudo_bit_t	mtt_version[0x00008];  /* Version of MTT page walk. Must be zero */
    pseudo_bit_t	reserved3[0x00018];
/* -------------- */
    pseudo_bit_t	mtt_base_addr_h[0x00020];/* MTT - Memory Translation table base physical address [63:32].
                                                 Table must be aligned to its size.
                                                 Address may be set to zero if address translation and protection is not supported. */
/* -------------- */
    pseudo_bit_t	mtt_base_addr_l[0x00020];/* MTT - Memory Translation table base physical address [31:0].
                                                 Table must be aligned to its size.
                                                 Address may be set to zero if address translation and protection is not supported. */
/* -------------- */
    pseudo_bit_t	reserved4[0x00040];
/* -------------- */
}; 

/* Multicast Support Parameters */

struct tavorprm_multicastparam_st {	/* Little Endian */
    pseudo_bit_t	mc_base_addr_h[0x00020];/* Base Address of the Multicast Table [63:32].
                                                 The base address must be aligned to the entry size.
                                                 Address may be set to zero if multicast is not supported. */
/* -------------- */
    pseudo_bit_t	mc_base_addr_l[0x00020];/* Base Address of the Multicast Table [31:0]. 
                                                 The base address must be aligned to the entry size.
                                                 Address may be set to zero if multicast is not supported. */
/* -------------- */
    pseudo_bit_t	reserved0[0x00040];
/* -------------- */
    pseudo_bit_t	log_mc_table_entry_sz[0x00010];/* Log2 of the Size of multicast group member (MGM) entry.
                                                 Must be greater than 5 (to allow CTRL and GID sections). 
                                                 That implies the number of QPs per MC table entry. */
    pseudo_bit_t	reserved1[0x00010];
/* -------------- */
    pseudo_bit_t	mc_table_hash_sz[0x00011];/* Number of entries in multicast DGID hash table (must be power of 2)
                                                 INIT_HCA - the required number of entries
                                                 QUERY_HCA - the actual number of entries assigned by firmware (will be less than or equal to the amount required in INIT_HCA) */
    pseudo_bit_t	reserved2[0x0000f];
/* -------------- */
    pseudo_bit_t	log_mc_table_sz[0x00005];/* Log2 of the overall number of MC entries in the MCG table (includes both hash and auxiliary tables) */
    pseudo_bit_t	reserved3[0x00013];
    pseudo_bit_t	mc_hash_fn[0x00003];   /* Multicast hash function
                                                 0 - Default hash function
                                                 other - reserved */
    pseudo_bit_t	reserved4[0x00005];
/* -------------- */
    pseudo_bit_t	reserved5[0x00020];
/* -------------- */
}; 

/* Memory Access Parameters for UD Address Vector Table */

struct tavorprm_udavtable_memory_parameters_st {	/* Little Endian */
    pseudo_bit_t	l_key[0x00020];        /* L_Key used to access TPT */
/* -------------- */
    pseudo_bit_t	pd[0x00018];           /* PD used by TPT for matching against PD of region entry being accessed. */
    pseudo_bit_t	reserved0[0x00005];
    pseudo_bit_t	xlation_en[0x00001];   /* When cleared, address is physical address and no translation will be done. When set, address is virtual. TPT will be accessed in both cases for address decoding purposes. */
    pseudo_bit_t	reserved1[0x00002];
/* -------------- */
}; 

/* QPC/EEC/CQC/EQC/RDB Parameters */

struct tavorprm_qpcbaseaddr_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00080];
/* -------------- */
    pseudo_bit_t	qpc_base_addr_h[0x00020];/* QPC Base Address [63:32]
                                                 Table must be aligned on its size */
/* -------------- */
    pseudo_bit_t	log_num_of_qp[0x00005];/* Log base 2 of number of supported QPs */
    pseudo_bit_t	reserved1[0x00002];
    pseudo_bit_t	qpc_base_addr_l[0x00019];/* QPC Base Address [31:7]
                                                 Table must be aligned on its size */
/* -------------- */
    pseudo_bit_t	reserved2[0x00040];
/* -------------- */
    pseudo_bit_t	eec_base_addr_h[0x00020];/* EEC Base Address [63:32]
                                                 Table must be aligned on its size.
                                                 Address may be set to zero if RD is not supported. */
/* -------------- */
    pseudo_bit_t	log_num_of_ee[0x00005];/* Log base 2 of number of supported EEs. */
    pseudo_bit_t	reserved3[0x00002];
    pseudo_bit_t	eec_base_addr_l[0x00019];/* EEC Base Address [31:7]
                                                 Table must be aligned on its size
                                                 Address may be set to zero if RD is not supported. */
/* -------------- */
    pseudo_bit_t	srqc_base_addr_h[0x00020];/* SRQ Context Base Address [63:32]
                                                 Table must be aligned on its size
                                                 Address may be set to zero if SRQ is not supported. */
/* -------------- */
    pseudo_bit_t	log_num_of_srq[0x00005];/* Log base 2 of number of supported SRQs. */
    pseudo_bit_t	srqc_base_addr_l[0x0001b];/* SRQ Context Base Address [31:5]
                                                 Table must be aligned on its size
                                                 Address may be set to zero if SRQ is not supported. */
/* -------------- */
    pseudo_bit_t	cqc_base_addr_h[0x00020];/* CQC Base Address [63:32]
                                                 Table must be aligned on its size */
/* -------------- */
    pseudo_bit_t	log_num_of_cq[0x00005];/* Log base 2 of number of supported CQs. */
    pseudo_bit_t	reserved4[0x00001];
    pseudo_bit_t	cqc_base_addr_l[0x0001a];/* CQC Base Address [31:6]
                                                 Table must be aligned on its size */
/* -------------- */
    pseudo_bit_t	reserved5[0x00040];
/* -------------- */
    pseudo_bit_t	eqpc_base_addr_h[0x00020];/* Extended QPC Base Address [63:32]
                                                 Table has same number of entries as QPC table.
                                                 Table must be aligned to entry size. */
/* -------------- */
    pseudo_bit_t	eqpc_base_addr_l[0x00020];/* Extended QPC Base Address [31:0]
                                                 Table has same number of entries as QPC table.
                                                 Table must be aligned to entry size. */
/* -------------- */
    pseudo_bit_t	reserved6[0x00040];
/* -------------- */
    pseudo_bit_t	eeec_base_addr_h[0x00020];/* Extended EEC Base Address [63:32]
                                                 Table has same number of entries as EEC table.
                                                 Table must be aligned to entry size.
                                                 Address may be set to zero if RD is not supported. */
/* -------------- */
    pseudo_bit_t	eeec_base_addr_l[0x00020];/* Extended EEC Base Address [31:0]
                                                 Table has same number of entries as EEC table.
                                                 Table must be aligned to entry size.
                                                 Address may be set to zero if RD is not supported. */
/* -------------- */
    pseudo_bit_t	reserved7[0x00040];
/* -------------- */
    pseudo_bit_t	eqc_base_addr_h[0x00020];/* EQC Base Address [63:32]
                                                 Address may be set to zero if EQs are not supported.
                                                 Table must be aligned to entry size. */
/* -------------- */
    pseudo_bit_t	log_num_eq[0x00004];   /* Log base 2 of number of supported EQs.
                                                 Must be 6 or less in InfiniHost. */
    pseudo_bit_t	reserved8[0x00002];
    pseudo_bit_t	eqc_base_addr_l[0x0001a];/* EQC Base Address [31:6]
                                                 Address may be set to zero if EQs are not supported.
                                                 Table must be aligned to entry size. */
/* -------------- */
    pseudo_bit_t	reserved9[0x00040];
/* -------------- */
    pseudo_bit_t	rdb_base_addr_h[0x00020];/* Base address of table that holds remote read and remote atomic requests [63:32]. 
                                                 Table must be aligned to RDB entry size (32 bytes).
                                                 Address may be set to zero if remote RDMA reads are not supported.
                                                 Please refer to QP and EE chapter for further explanation on RDB allocation. */
/* -------------- */
    pseudo_bit_t	rdb_base_addr_l[0x00020];/* Base address of table that holds remote read and remote atomic requests [31:0]. 
                                                 Table must be aligned to RDB entry size (32 bytes).
                                                 This field must always be zero. 
                                                 Please refer to QP and EE chapter for further explanation on RDB allocation. */
/* -------------- */
    pseudo_bit_t	reserved10[0x00040];
/* -------------- */
}; 

/* Performance Monitors */

struct tavorprm_performance_monitors_st {	/* Little Endian */
    pseudo_bit_t	e0[0x00001];           /* Enables counting of respective performance counter */
    pseudo_bit_t	e1[0x00001];           /* Enables counting of respective performance counter */
    pseudo_bit_t	e2[0x00001];           /* Enables counting of respective performance counter */
    pseudo_bit_t	reserved0[0x00001];
    pseudo_bit_t	r0[0x00001];           /* If written to as '1 - resets respective performance counter, if written to az '0 - no change to matter */
    pseudo_bit_t	r1[0x00001];           /* If written to as '1 - resets respective performance counter, if written to az '0 - no change to matter */
    pseudo_bit_t	r2[0x00001];           /* If written to as '1 - resets respective performance counter, if written to az '0 - no change to matter */
    pseudo_bit_t	reserved1[0x00001];
    pseudo_bit_t	i0[0x00001];           /* Interrupt enable on respective counter overflow. '1 - interrupt enabled, '0 - interrupt disabled. */
    pseudo_bit_t	i1[0x00001];           /* Interrupt enable on respective counter overflow. '1 - interrupt enabled, '0 - interrupt disabled. */
    pseudo_bit_t	i2[0x00001];           /* Interrupt enable on respective counter overflow. '1 - interrupt enabled, '0 - interrupt disabled. */
    pseudo_bit_t	reserved2[0x00001];
    pseudo_bit_t	f0[0x00001];           /* Overflow flag. If set, overflow occurred on respective counter. Cleared if written to as '1 */
    pseudo_bit_t	f1[0x00001];           /* Overflow flag. If set, overflow occurred on respective counter. Cleared if written to as '1 */
    pseudo_bit_t	f2[0x00001];           /* Overflow flag. If set, overflow occurred on respective counter. Cleared if written to as '1 */
    pseudo_bit_t	reserved3[0x00001];
    pseudo_bit_t	ev_cnt1[0x00005];      /* Specifies event to be counted by Event_counter1 See XXX for events' definition. */
    pseudo_bit_t	reserved4[0x00003];
    pseudo_bit_t	ev_cnt2[0x00005];      /* Specifies event to be counted by Event_counter2 See XXX for events' definition. */
    pseudo_bit_t	reserved5[0x00003];
/* -------------- */
    pseudo_bit_t	clock_counter[0x00020];
/* -------------- */
    pseudo_bit_t	event_counter1[0x00020];
/* -------------- */
    pseudo_bit_t	event_counter2[0x00020];/* Read/write event counter, counting events specified by EvCntl and EvCnt2 fields repsectively. When the event counter reaches is maximum value of 0xFFFFFF, the next event will cause it to roll over to zero, set F1 or F2 bit respectively and generate interrupt by I1 I2 bit respectively. */
/* -------------- */
}; 

/* QP and EE Context Entry */

struct tavorprm_queue_pair_ee_context_entry_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00008];
    pseudo_bit_t	de[0x00001];           /* Send/Receive Descriptor Event enable - if set, events can be generated upon descriptors' completion on send/receive queue (controlled by E bit in WQE). Invalid in EE context */
    pseudo_bit_t	reserved1[0x00002];
    pseudo_bit_t	pm_state[0x00002];     /* Path migration state (Migrated, Armed or Rearm)
                                                 11-Migrated
                                                 00-Armed
                                                 01-Rearm
                                                 10-Reserved
                                                 Should be set to 11 for UD QPs and for QPs which do not support APM */
    pseudo_bit_t	reserved2[0x00003];
    pseudo_bit_t	st[0x00003];           /* Service type (invalid in EE context):
                                                 000-Reliable Connection
                                                 001-Unreliable Connection
                                                 010-Reliable Datagram (Not supported for InfiniHost MT23108)
                                                 011-Unreliable Datagram
                                                 111-MLX transport (raw bits injection). Used for management QPs and RAW */
    pseudo_bit_t	reserved3[0x00009];
    pseudo_bit_t	state[0x00004];        /* QP/EE state:
                                                 0 - RST
                                                 1 - INIT
                                                 2 - RTR
                                                 3 - RTS
                                                 4 - SQEr
                                                 5 - SQD (Send Queue Drained)
                                                 6 - ERR
                                                 7 - Send Queue Draining
                                                 8 - F - RESERVED
                                                 (Valid for QUERY_QPEE and ERR2RST_QPEE commands only) */
/* -------------- */
    pseudo_bit_t	sched_queue[0x00004];  /* Schedule queue to be used for WQE scheduling to execution. Determines QOS for this QP. */
    pseudo_bit_t	reserved4[0x0001c];
/* -------------- */
    pseudo_bit_t	reserved5[0x00018];
    pseudo_bit_t	msg_max[0x00005];      /* Max message size allowed on the QP. Maximum message size is 2^msg_Max.
                                                 Must be equal to MTU for UD and MLX QPs. */
    pseudo_bit_t	mtu[0x00003];          /* MTU of the QP (Must be the same for both paths: primary and alternative):
                                                 0x1 - 256 bytes
                                                 0x2 - 512
                                                 0x3 - 1024
                                                 0x4 - 2048
                                                 other - reserved
                                                 
                                                 Should be configured to 0x4 for UD and MLX QPs. */
/* -------------- */
    pseudo_bit_t	usr_page[0x00018];     /* Index (offset) of user page allocated for this QP (see "non_privileged Access to the HCA Hardware"). Not valid (reserved) in EE context. */
    pseudo_bit_t	reserved6[0x00008];
/* -------------- */
    pseudo_bit_t	local_qpn_een[0x00018];/* Local QP/EE number Lower bits determine position of this record in QPC table, and - thus - constrained
                                                 This field is valid for QUERY and ERR2RST commands only. */
    pseudo_bit_t	reserved7[0x00008];
/* -------------- */
    pseudo_bit_t	remote_qpn_een[0x00018];/* Remote QP/EE number */
    pseudo_bit_t	reserved8[0x00008];
/* -------------- */
    pseudo_bit_t	reserved9[0x00040];
/* -------------- */
    struct tavorprm_address_path_st	primary_address_path;/* Primary address path for the QP/EE */
/* -------------- */
    struct tavorprm_address_path_st	alternative_address_path;/* Alternate address path for the QP/EE */
/* -------------- */
    pseudo_bit_t	rdd[0x00018];          /* Reliable Datagram Domain */
    pseudo_bit_t	reserved10[0x00008];
/* -------------- */
    pseudo_bit_t	pd[0x00018];           /* QP protection domain.  Not valid (reserved) in EE context. */
    pseudo_bit_t	reserved11[0x00008];
/* -------------- */
    pseudo_bit_t	wqe_base_adr[0x00020]; /* Bits 63:32 of WQE address for both SQ and RQ. 
                                                 Reserved for EE context. */
/* -------------- */
    pseudo_bit_t	wqe_lkey[0x00020];     /* memory key (L-Key) to be used to access WQEs. Not valid (reserved) in EE context. */
/* -------------- */
    pseudo_bit_t	reserved12[0x00003];
    pseudo_bit_t	ssc[0x00001];          /* Send Signaled Completion
                                                 1 - all send WQEs generate CQEs. 
                                                 0 - only send WQEs with C bit set generate completion. 
                                                 Not valid (reserved) in EE context. */
    pseudo_bit_t	sic[0x00001];          /* If set - Ignore end to end credits on send queue. Not valid (reserved) in EE context. */
    pseudo_bit_t	cur_retry_cnt[0x00003];/* Current transport retry counter (QUERY_QPEE only).
                                                 The current transport retry counter can vary from retry_count down to 1, where 1 means that the last retry attempt is currently executing. */
    pseudo_bit_t	cur_rnr_retry[0x00003];/* Current RNR retry counter (QUERY_QPEE only).
                                                 The current RNR retry counter can vary from rnr_retry to 1, where 1 means that the last retry attempt is currently executing. */
    pseudo_bit_t	reserved13[0x00002];
    pseudo_bit_t	sae[0x00001];          /* If set - Atomic operations enabled on send queue. Not valid (reserved) in EE context. */
    pseudo_bit_t	swe[0x00001];          /* If set - RDMA - write enabled on send queue. Not valid (reserved) in EE context. */
    pseudo_bit_t	sre[0x00001];          /* If set - RDMA - read enabled on send queue. Not valid (reserved) in EE context. */
    pseudo_bit_t	retry_count[0x00003];  /* Transport timeout Retry count */
    pseudo_bit_t	reserved14[0x00002];
    pseudo_bit_t	sra_max[0x00003];      /* Maximum number of outstanding RDMA-read/Atomic operations allowed in the send queue. Maximum number is 2^SRA_Max. Must be zero in EE context. */
    pseudo_bit_t	flight_lim[0x00004];   /* Number of outstanding (in-flight) messages on the wire allowed for this send queue. 
                                                 Number of outstanding messages is 2^Flight_Lim. 
                                                 Use 0xF for unlimited number of outstanding messages. */
    pseudo_bit_t	ack_req_freq[0x00004]; /* ACK required frequency. ACK required bit will be set in every 2^AckReqFreq packets at least.  Not valid for RD QP. */
/* -------------- */
    pseudo_bit_t	reserved15[0x00020];
/* -------------- */
    pseudo_bit_t	next_send_psn[0x00018];/* Next PSN to be sent */
    pseudo_bit_t	reserved16[0x00008];
/* -------------- */
    pseudo_bit_t	cqn_snd[0x00018];      /* CQ number completions from the send queue to be reported to.  Not valid (reserved) in EE context. */
    pseudo_bit_t	reserved17[0x00008];
/* -------------- */
    pseudo_bit_t	next_snd_wqe_0[0x00020];/* Pointer and properties of next WQE on send queue. The format is same as next segment (first 8 bytes) in the WQE. This field is read-only and provided for debug purposes.  Not valid (reserved) in EE context. */
/* -------------- */
    pseudo_bit_t	next_snd_wqe_1[0x00020];/* Pointer and properties of next WQE on send queue. The format is same as next segment (first 8 bytes) in the WQE. This field is read-only and provided for debug purposes.  Not valid (reserved) in EE context. */
/* -------------- */
    pseudo_bit_t	last_acked_psn[0x00018];/* The last acknowledged PSN for the requester (QUERY_QPEE only) */
    pseudo_bit_t	reserved18[0x00008];
/* -------------- */
    pseudo_bit_t	ssn[0x00018];          /* Requester Send Sequence Number (QUERY_QPEE only) */
    pseudo_bit_t	reserved19[0x00008];
/* -------------- */
    pseudo_bit_t	reserved20[0x00003];
    pseudo_bit_t	rsc[0x00001];          /* 1 - all receive WQEs generate CQEs. 
                                                 0 - only receive WQEs with C bit set generate completion. 
                                                 Not valid (reserved) in EE context.
                                                  */
    pseudo_bit_t	ric[0x00001];          /* Invalid Credits. 
                                                 1 - place "Invalid Credits" to ACKs sent from this queue.
                                                 0 - ACKs report the actual number of end to end credits on the connection.  
                                                 Not valid (reserved) in EE context.
                                                 Must be set to 1 on QPs which are attached to SRQ. */
    pseudo_bit_t	reserved21[0x00008];
    pseudo_bit_t	rae[0x00001];          /* If set - Atomic operations enabled. on receive queue. Not valid (reserved) in EE context. */
    pseudo_bit_t	rwe[0x00001];          /* If set - RDMA - write enabled on receive queue. Not valid (reserved) in EE context. */
    pseudo_bit_t	rre[0x00001];          /* If set - RDMA - read enabled on receive queue. Not valid (reserved) in EE context. */
    pseudo_bit_t	reserved22[0x00005];
    pseudo_bit_t	rra_max[0x00003];      /* Maximum number of outstanding RDMA-read/Atomic operations allowed on receive queue is 2^RRA_Max. 
                                                 Must be 0 for EE context. */
    pseudo_bit_t	reserved23[0x00008];
/* -------------- */
    pseudo_bit_t	next_rcv_psn[0x00018]; /* Next (expected) PSN on receive */
    pseudo_bit_t	min_rnr_nak[0x00005];  /* Minimum RNR NAK timer value (TTTTT field encoding according to the IB spec Vol1 9.7.5.2.8). 
                                                 Not valid (reserved) in EE context. */
    pseudo_bit_t	reserved24[0x00003];
/* -------------- */
    pseudo_bit_t	reserved25[0x00005];
    pseudo_bit_t	ra_buff_indx[0x0001b]; /* Index to outstanding read/atomic buffer. 
                                                 This field constructs the address to the RDB for maintaining the incoming RDMA read and atomic requests. */
/* -------------- */
    pseudo_bit_t	cqn_rcv[0x00018];      /* CQ number completions from receive queue to be reported to. Not valid (reserved) in EE context. */
    pseudo_bit_t	reserved26[0x00008];
/* -------------- */
    pseudo_bit_t	next_rcv_wqe_0[0x00020];/* Pointer and properties of next WQE on the receive queue. This format is same as next segment (first 8 bytes) in the WQE.This field is read-only and provided for debug purposes. Not valid (reserved) in EE context. */
/* -------------- */
    pseudo_bit_t	next_rcv_wqe_1[0x00020];/* Pointer and properties of next WQE on the receive queue. This format is same as next segment (first 8 bytes) in the WQE.This field is read-only and provided for debug purposes. Not valid (reserved) in EE context. */
/* -------------- */
    pseudo_bit_t	q_key[0x00020];        /* Q_Key to be validated against received datagrams.
                                                 On send datagrams,  if Q_Key[31] specified in the WQE is set, then this Q_Key will be transmitted in the outgoing message.
                                                 Not valid (reserved) in EE context. */
/* -------------- */
    pseudo_bit_t	srqn[0x00018];         /* SRQN - Shared Receive Queue Number - specifies the SRQ number from which the QP dequeues receive descriptors. 
                                                 SRQN is valid only if SRQ bit is set. Not valid (reserved) in EE context. */
    pseudo_bit_t	srq[0x00001];          /* SRQ - Shared Receive Queue. If this bit is set, then the QP is associated with a SRQ. Not valid (reserved) in EE context. */
    pseudo_bit_t	reserved27[0x00007];
/* -------------- */
    pseudo_bit_t	rmsn[0x00018];         /* Responder current message sequence number (QUERY_QPEE only) */
    pseudo_bit_t	reserved28[0x00008];
/* -------------- */
    pseudo_bit_t	reserved29[0x00260];
/* -------------- */
}; 

/* MOD_STAT_CFG */

struct tavorprm_mod_stat_cfg_st {	/* Little Endian */
    pseudo_bit_t	log_max_srqs[0x00005]; /* Log (base 2) of the number of SRQs to allocate (0 if no SRQs are required), valid only if srq bit is set. */
    pseudo_bit_t	reserved0[0x00001];
    pseudo_bit_t	srq[0x00001];          /* When set SRQs are supported */
    pseudo_bit_t	srq_m[0x00001];        /* Modify SRQ parameters */
    pseudo_bit_t	reserved1[0x00018];
/* -------------- */
    pseudo_bit_t	reserved2[0x007e0];
/* -------------- */
}; 

/* SRQ Context */

struct tavorprm_srq_context_st {	/* Little Endian */
    pseudo_bit_t	wqe_addr_h[0x00020];   /* WQE base address for the SRQ [63:32]
                                                 Must be set at SW2HW_SRQ */
/* -------------- */
    pseudo_bit_t	ds[0x00006];           /* Descriptor Size on the SRQ in units of 16 bytes */
    pseudo_bit_t	next_wqe_addr_l[0x0001a];/* Next WQE address for the SRQ [31:6] 
                                                 Valid only on QUERY_SRQ and HW2SW_SRQ commands. */
/* -------------- */
    pseudo_bit_t	pd[0x00018];           /* SRQ PD - used for descriptor fetching on the SRQ and for data scatter on send operations on QPs attached to SRQ.
                                                 In InfiniHost MT23108 SRQ.PD must be equal to the PD of all QPs which are attached to the SRQ */
    pseudo_bit_t	reserved0[0x00004];
    pseudo_bit_t	state[0x00004];        /* SRQ State:
                                                 1111 - SW Ownership
                                                 0000 - HW Ownership
                                                 0001 - Error
                                                 Valid only on QUERY_SRQ and HW2SW_SRQ commands. */
/* -------------- */
    pseudo_bit_t	l_key[0x00020];        /* L_Key for descriptor fetching on the SRQ */
/* -------------- */
    pseudo_bit_t	uar[0x00018];          /* SRQ User Access Region - Index (offset) of user page allocated for the SRQ (see "Non Privileged Access to the HCA HW"). */
    pseudo_bit_t	reserved1[0x00008];
/* -------------- */
    pseudo_bit_t	wqe_cnt[0x00010];      /* WQE count on the SRQ. 
                                                 Valid only on QUERY_SRQ and HW2SW_SRQ commands. */
    pseudo_bit_t	lwm[0x00010];          /* Limit Water Mark - if the LWM is not zero, and the wqe_cnt drops below LWM when a WQE is dequeued from the SRQ, then a SRQ limit event is fired and the LWM is set to zero. */
/* -------------- */
    pseudo_bit_t	reserved2[0x00010];
    pseudo_bit_t	reserved3[0x00010];
/* -------------- */
    pseudo_bit_t	reserved4[0x00020];
/* -------------- */
}; 

/* InfiniHost Configuration Registers */

struct tavorprm_mt23108_configuration_registers_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x403400];
/* -------------- */
    struct tavorprm_hca_command_register_st	hca_command_interface_register;/* HCA Command Register */
/* -------------- */
    pseudo_bit_t	reserved1[0x00320];
/* -------------- */
    pseudo_bit_t	ecr_h[0x00020];        /* Event Cause Register[63:32]. Each bit in the ECR corresponds to one of the 64 Event Queues in InfiniHost. If bit is set, interrupt was asserted due to event reported on corresponding event queue. This register is read-only; writing to this register will cause undefined results
                                                  */
/* -------------- */
    pseudo_bit_t	ecr_l[0x00020];        /* Event Cause Register[31:0]. Each bit in the ECR corresponds to one of the 64 Event Queues in InfiniHost. If bit is set, interrupt was asserted due to event reported on corresponding event queue. This register is read-only; writing to this register will cause undefined results
                                                  */
/* -------------- */
    pseudo_bit_t	clr_ecr_h[0x00020];    /* Clear Event Cause Register[63:32]. 
                                                 This register is used to clear bits in ECR register. Each set bit in data written to this register clears corresponding bit in the ECR register, Each bit written with zero has no effect. This register is write-only. Reading from this register will cause undefined result
                                                  */
/* -------------- */
    pseudo_bit_t	clr_ecr_l[0x00020];    /* Clear Event Cause Register[31:0]. 
                                                 This register is used to clear bits in ECR register. Each set bit in data written to this register clears corresponding bit in the ECR register, Each bit written with zero has no effect. This register is write-only. Reading from this register will cause undefined result
                                                  */
/* -------------- */
    pseudo_bit_t	reserved2[0x4c780];
/* -------------- */
    pseudo_bit_t	reserved3[0x01000];
/* -------------- */
    pseudo_bit_t	reserved4[0x32f6c0];
/* -------------- */
    pseudo_bit_t	clr_int_h[0x00020];    /* Clear Interrupt [63:32]
                                                 This register is used to clear (de-assert) interrupt output pins of InfiniHost. The value to be written in this register is obtained by executing QUERY_ADAPTER command on command interface after system boot. This register is write-only. Reading from this register will cause undefined result */
/* -------------- */
    pseudo_bit_t	clr_int_l[0x00020];    /* Clear Interrupt [31:0]
                                                 This register is used to clear (de-assert) interrupt output pins of InfiniHost. The value to be written in this register is obtained by executing QUERY_ADAPTER command on command interface after system boot. This register is write-only. Reading from this register will cause undefined result */
/* -------------- */
    pseudo_bit_t	reserved5[0x7f900];
/* -------------- */
}; 

/* Schedule queues configuration */

struct tavorprm_cfg_schq_st {	/* Little Endian */
    pseudo_bit_t	quota[0x00008];        /* Number of WQEs that are executed until preemption of the scheduling queue and switching to the next schedule queue */
    pseudo_bit_t	reserved0[0x00018];
/* -------------- */
    pseudo_bit_t	rqsq0[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq0[0x00008];        /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq1[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq1[0x00008];        /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq2[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq2[0x00008];        /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq3[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq3[0x00008];        /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq4[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq4[0x00008];        /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq5[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq5[0x00008];        /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq6[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq6[0x00008];        /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq7[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq7[0x00008];        /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq8[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq8[0x00008];        /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq9[0x00008];        /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq9[0x00008];        /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq10[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq10[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq11[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq11[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq12[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq12[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq13[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq13[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq14[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq14[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq15[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq15[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq16[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq16[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq17[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq17[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq18[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq18[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq19[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq19[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq20[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq20[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq21[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq21[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq22[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq22[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq23[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq23[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq24[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq24[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq25[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq25[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq26[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq26[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq27[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq27[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq28[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq28[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq29[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq29[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	rqsq30[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq30[0x00008];       /* Weight for responder schedule queue */
    pseudo_bit_t	rqsq31[0x00008];       /* Weight for requestor schedule queue */
    pseudo_bit_t	rssq31[0x00008];       /* Weight for responder schedule queue */
/* -------------- */
    pseudo_bit_t	reserved1[0x005e0];
/* -------------- */
}; 

/* Query BAR */

struct tavorprm_query_bar_st {	/* Little Endian */
    pseudo_bit_t	bar_base_h[0x00020];   /* BAR base [63:32] */
/* -------------- */
    pseudo_bit_t	reserved0[0x00014];
    pseudo_bit_t	bar_base_l[0x0000c];   /* BAR base [31:20] */
/* -------------- */
}; 

/* Performance Counters */

struct tavorprm_performance_counters_st {	/* Little Endian */
    pseudo_bit_t	sqpc_access_cnt[0x00020];/* SQPC cache access count */
/* -------------- */
    pseudo_bit_t	sqpc_miss_cnt[0x00020];/* SQPC cache miss count */
/* -------------- */
    pseudo_bit_t	reserved0[0x00040];
/* -------------- */
    pseudo_bit_t	rqpc_access_cnt[0x00020];/* RQPC cache access count */
/* -------------- */
    pseudo_bit_t	rqpc_miss_cnt[0x00020];/* RQPC cache miss count */
/* -------------- */
    pseudo_bit_t	reserved1[0x00040];
/* -------------- */
    pseudo_bit_t	cqc_access_cnt[0x00020];/* CQC cache access count */
/* -------------- */
    pseudo_bit_t	cqc_miss_cnt[0x00020]; /* CQC cache miss count */
/* -------------- */
    pseudo_bit_t	reserved2[0x00040];
/* -------------- */
    pseudo_bit_t	tpt_access_cnt[0x00020];/* TPT cache access count */
/* -------------- */
    pseudo_bit_t	mpt_miss_cnt[0x00020]; /* MPT cache miss count */
/* -------------- */
    pseudo_bit_t	mtt_miss_cnt[0x00020]; /* MTT cache miss count */
/* -------------- */
    pseudo_bit_t	reserved3[0x00620];
/* -------------- */
}; 

/* Transport and CI Error Counters */

struct tavorprm_transport_and_ci_error_counters_st {	/* Little Endian */
    pseudo_bit_t	rq_num_lle[0x00020];   /* Responder - number of local length errors.
                                                 Local Length Errors: Inbound "Send" request message exceeded the responders available buffer space. */
/* -------------- */
    pseudo_bit_t	sq_num_lle[0x00020];   /* Requester - number of local length errors.
                                                 Length Errors: RDMA READ response message contained too much or too little payload data. */
/* -------------- */
    pseudo_bit_t	rq_num_lqpoe[0x00020]; /* Responder - number local QP operation error.
                                                 1. Malformed WQE:  Responder detected a malformed Receive Queue WQE while processing the packet.
                                                 2. Local QP Error:  Responder detected a local QP related error while executing the request message. The local error  prevented the responder from completing the request. */
/* -------------- */
    pseudo_bit_t	sq_num_lqpoe[0x00020]; /* Requester - number local QP operation error
                                                 1. Local Operation Error: (WQE gather, affiliated or unaffiliated): An error occurred in the requesters local channel interface that either cannot be associated with a certain WQE, or occurred when reading a WQE.
                                                  */
/* -------------- */
    pseudo_bit_t	rq_num_leeoe[0x00020]; /* Responder - number local EE operation error.
                                                 RD */
/* -------------- */
    pseudo_bit_t	sq_num_leeoe[0x00020]; /* Requester - number local EE operation error.
                                                 RD */
/* -------------- */
    pseudo_bit_t	rq_num_lpe[0x00020];   /* Responder - number of local protection errors.
                                                 Local QP (Protection) Error: Responder detected a local access violation error while executing a send request message. The error  prevented the responder from completing the request. */
/* -------------- */
    pseudo_bit_t	sq_num_lpe[0x00020];   /* Requester - number of local protection errors.
                                                 Local Memory Protection Error: Requester detected a memory translation/protection (TPT) error.
                                                  */
/* -------------- */
    pseudo_bit_t	rq_num_wrfe[0x00020];  /* Responder - number of CQEs with error generated. */
/* -------------- */
    pseudo_bit_t	sq_num_wrfe[0x00020];  /* Requester - number of CQEs with error generated. */
/* -------------- */
    pseudo_bit_t	reserved0[0x00020];
/* -------------- */
    pseudo_bit_t	sq_num_mwbe[0x00020];  /* Requester - number of memory window bind errors. */
/* -------------- */
    pseudo_bit_t	reserved1[0x00020];
/* -------------- */
    pseudo_bit_t	sq_num_bre[0x00020];   /* Requester - number of bad response errors.
                                                 Bad response: Unexpected opcode for the response packet received at the expected response PSN. */
/* -------------- */
    pseudo_bit_t	rq_num_lae[0x00020];   /* Responder - number of local access errors.
                                                 Unused. */
/* -------------- */
    pseudo_bit_t	reserved2[0x00040];
/* -------------- */
    pseudo_bit_t	sq_num_rire[0x00020];  /* Requester - number of remote invalid request errors.
                                                 NAK-Invalid Request on:
                                                  1. Unsupported OpCode:   Responder detected an unsupported OpCode.
                                                  2. Unexpected OpCode:  Responder detected an error in the sequence of OpCodes, such as a missing "Last" packet.
                                                    Note: there is no PSN error, thus this does not indicate a dropped packet. */
/* -------------- */
    pseudo_bit_t	rq_num_rire[0x00020];  /* Responder - number of remote invalid request errors.
                                                 NAK may or may not be sent.
                                                  1. Unsupported or Reserved OpCode:  Inbound request OpCode was either reserved, or was for a function not supported by this QP. (E.G. RDMA or ATOMIC on QP not set up for this). For RC this is "QP Async affiliated".
                                                  2. Misaligned ATOMIC:  VA does not point to an aligned address on an atomic operation.
                                                  3. Too many RDMA READ or ATOMIC Requests:  There were more requests received and not ACKed than allowed for the connection.
                                                  4. Out of Sequence OpCode, current packet is "first" or "Only":  The Responder detected an error in the sequence of OpCodes; a missing "Last" packet
                                                  5. Out of Sequence OpCode, current packet is not "first" or "Only":  The Responder detected an error in the sequence of OpCodes; a missing "First" packet
                                                  6. Local Length Error: Inbound "Send" request message exceeded the responder.s available buffer space.
                                                  7. Length error:  RDMA WRITE request message contained too much or too little payload data compared to the DMA length advertised in the first or only packet.
                                                  8. Length error: Payload length was not consistent with the opcode: 
                                                     a: 0 byte <= "only" <= PMTU bytes
                                                     b: ("first" or "middle") == PMTU bytes
                                                    c: 1byte <= "last" <= PMTU bytes 
                                                  9. Length error: Inbound message exceeded the size supported by the CA port. */
/* -------------- */
    pseudo_bit_t	sq_num_rae[0x00020];   /* Requester - number of remote access errors.
                                                 NAK-Remote Access Error on:
                                                  R_Key Violation:   Responder detected an invalid R_Key while executing an RDMA Request. */
/* -------------- */
    pseudo_bit_t	rq_num_rae[0x00020];   /* Responder - number of remote access errors.
                                                 R_Key Violation   Responder detected an R_Key violation  while executing an RDMA request.
                                                 NAK may or may not be sent. */
/* -------------- */
    pseudo_bit_t	sq_num_roe[0x00020];   /* Requester - number of remote operation errors.
                                                 NAK-Remote Operation Error on:
                                                 Remote Operation Error:   Responder encountered an error, (local to the responder), which prevented it from completing the request. */
/* -------------- */
    pseudo_bit_t	rq_num_roe[0x00020];   /* Responder - number of remote operation errors.
                                                 NAK-Remote Operation Error on:
                                                  1. Malformed WQE:  Responder detected a malformed Receive Queue WQE while processing the packet.
                                                  2. Remote Operation Error:   Responder encountered an error, (local to the responder), which prevented it from completing the request. */
/* -------------- */
    pseudo_bit_t	sq_num_tree[0x00020];  /* Requester - number of transport retries exceeded errors.
                                                  1. Packet sequence error: Retry limit exceeded. Responder detected a PSN larger than it expected. The requestor performed retries, and automatic path migration and additional retries, if applicable, but all attempts failed.
                                                  2. Implied NAK sequence error: Retry limit exceeded.  Requestor detected an ACK with a PSN larger than the expected PSN for an RDMA READ or atomic response. The requestor performed retries, and automatic path migration and additional retries, if applicable, but all attempts failed.
                                                  3. Local Ack Timeout error: Retry limit exceeded. No ACK response within timer interval. The requestor performed retries, and automatic path migration and additional retries, but all attempts failed. */
/* -------------- */
    pseudo_bit_t	reserved3[0x00020];
/* -------------- */
    pseudo_bit_t	sq_num_rree[0x00020];  /* Requester - number of RNR nak retries exceeded errors.
                                                  RNR NAK Retry error. Retry limit exceeded. Excessive RNR NAKs returned by the responder:  Requestor retried the request "n" times, but received RNR NAK each time. */
/* -------------- */
    pseudo_bit_t	reserved4[0x00020];
/* -------------- */
    pseudo_bit_t	sq_num_lrdve[0x00020]; /* Requester - number of local RDD violation errors.
                                                 RD only. */
/* -------------- */
    pseudo_bit_t	rq_num_rirdre[0x00020];/* Responder - number of remote invalid RD request errors.
                                                 RD only. */
/* -------------- */
    pseudo_bit_t	reserved5[0x00040];
/* -------------- */
    pseudo_bit_t	sq_num_rabrte[0x00020];/* Requester - number of remote aborted errors.
                                                 RD only. */
/* -------------- */
    pseudo_bit_t	reserved6[0x00020];
/* -------------- */
    pseudo_bit_t	sq_num_ieecne[0x00020];/* Requester - number of invalid EE context number errors.
                                                 RD only. */
/* -------------- */
    pseudo_bit_t	reserved7[0x00020];
/* -------------- */
    pseudo_bit_t	sq_num_ieecse[0x00020];/* Requester - invalid EE context state errors.
                                                 RD only. */
/* -------------- */
    pseudo_bit_t	reserved8[0x00380];
/* -------------- */
    pseudo_bit_t	rq_num_oos[0x00020];   /* Responder - number of out of sequence requests received.
                                                 Out of Sequence Request Packet: Packet PSN of the inbound request is outside the responders valid PSN window.
                                                 NAK may or may not be sent. */
/* -------------- */
    pseudo_bit_t	sq_num_oos[0x00020];   /* Requester - number of out of sequence Naks received.
                                                 NAK-Sequence Error on:
                                                  1. Packet sequence error. Retry limit not exceeded: Responder detected a PSN larger than it expected. Requester may retry the request.
                                                  2. Packet sequence error. Retry limit exceeded: Responder detected a PSN larger than it expected. The requestor performed retries, and automatic path migration and additional retries, if applicable, but all attempts failed. */
/* -------------- */
    pseudo_bit_t	rq_num_mce[0x00020];   /* Responder - number of bad multicast packets received.
                                                 Missing GID or bad GID. */
/* -------------- */
    pseudo_bit_t	reserved9[0x00020];
/* -------------- */
    pseudo_bit_t	rq_num_rsync[0x00020]; /* Responder - number of RESYNC operations.
                                                 RD only. */
/* -------------- */
    pseudo_bit_t	sq_num_rsync[0x00020]; /* Requester - number of RESYNC operations.
                                                 RD only. */
/* -------------- */
    pseudo_bit_t	rq_num_udsdprd[0x00020];/* The number of UD packets silently discarded on the receive queue due to lack of receive descriptor.
                                                 Resources Not Ready Error: A UD WQE is not currently available. */
/* -------------- */
    pseudo_bit_t	reserved10[0x00020];
/* -------------- */
    pseudo_bit_t	rq_num_ucsdprd[0x00020];/* The number of UC packets silently discarded on the receive queue due to lack of receive descriptor.
                                                 Resources Not Ready Error: A UC WQE is not currently available. */
/* -------------- */
    pseudo_bit_t	reserved11[0x003e0];
/* -------------- */
    pseudo_bit_t	num_cqovf[0x00020];    /* Number of CQ overflows.
                                                 Incremented each time a completion is discarded due CQ overflow. */
/* -------------- */
    pseudo_bit_t	num_eqovf[0x00020];    /* Number of EQ overflows.
                                                  Incremented each time EQ enters the overflow state. */
/* -------------- */
    pseudo_bit_t	num_baddb[0x00020];    /* Number of bad doorbells.
                                                  Doorbell dropped due to UAR violation or bad resource state. */
/* -------------- */
    pseudo_bit_t	reserved12[0x002a0];
/* -------------- */
}; 

/* Event_data Field - HCR Completion Event */

struct tavorprm_hcr_completion_event_st {	/* Little Endian */
    pseudo_bit_t	token[0x00010];        /* HCR Token */
    pseudo_bit_t	reserved0[0x00010];
/* -------------- */
    pseudo_bit_t	reserved1[0x00020];
/* -------------- */
    pseudo_bit_t	status[0x00008];       /* HCR Status */
    pseudo_bit_t	reserved2[0x00018];
/* -------------- */
    pseudo_bit_t	out_param_h[0x00020];  /* HCR Output Parameter [63:32] */
/* -------------- */
    pseudo_bit_t	out_param_l[0x00020];  /* HCR Output Parameter [31:0] */
/* -------------- */
    pseudo_bit_t	reserved3[0x00020];
/* -------------- */
}; 

/* Completion with Error CQE */

struct tavorprm_completion_with_error_st {	/* Little Endian */
    pseudo_bit_t	myqpn[0x00018];        /* Indicates the QP for which completion is being reported */
    pseudo_bit_t	reserved0[0x00008];
/* -------------- */
    pseudo_bit_t	reserved1[0x00060];
/* -------------- */
    pseudo_bit_t	db_cnt[0x00010];       /* Doorbell count */
    pseudo_bit_t	reserved2[0x00008];
    pseudo_bit_t	syndrome[0x00008];     /* Completion with error syndrome:
                                                         0x01 - Local Length Error
                                                         0x02 - Local QP Operation Error
                                                         0x03 - Local EE Context Operation Error
                                                         0x04 - Local Protection Error
                                                         0x05 - Work Request Flushed Error 
                                                         0x06 - Memory Window Bind Error
                                                         0x10 - Bad Response Error
                                                         0x11 - Local Access Error
                                                         0x12 - Remote Invalid Request Error
                                                         0x13 - Remote Access Error
                                                         0x14 - Remote Operation Error
                                                         0x15 - Transport Retry Counter Exceeded
                                                         0x16 - RNR Retry Counter Exceeded
                                                         0x20 - Local RDD Violation Error
                                                         0x21 - Remote Invalid RD Request
                                                         0x22 - Remote Aborted Error
                                                         0x23 - Invalid EE Context Number
                                                         0x24 - Invalid EE Context State
                                                         other - Reserved
                                                 Syndrome is defined according to the IB specification volume 1. For detailed explanation of the syndromes, refer to chapters 10-11 of the IB specification rev 1.1. */
/* -------------- */
    pseudo_bit_t	reserved3[0x00020];
/* -------------- */
    pseudo_bit_t	wqe_size[0x00006];     /* Size (in 16-byte chunks) of WQE completion is reported for */
    pseudo_bit_t	wqe_addr[0x0001a];     /* Bits 31:6 of WQE virtual address completion is reported for. The 6 least significant bits are zero. */
/* -------------- */
    pseudo_bit_t	reserved4[0x00007];
    pseudo_bit_t	owner[0x00001];        /* Owner field. Zero value of this field means SW ownership of CQE. */
    pseudo_bit_t	reserved5[0x00010];
    pseudo_bit_t	opcode[0x00008];       /* The opcode of WQE completion is reported for.
                                                 
                                                 The following values are reported in case of completion with error:
                                                 0xFE - For completion with error on Receive Queues
                                                 0xFF - For completion with error on Send Queues */
/* -------------- */
}; 

/* Resize CQ Input Mailbox */

struct tavorprm_resize_cq_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00020];
/* -------------- */
    pseudo_bit_t	start_addr_h[0x00020]; /* Start address of CQ[63:32]. 
                                                 Must be aligned on CQE size (32 bytes) */
/* -------------- */
    pseudo_bit_t	start_addr_l[0x00020]; /* Start address of CQ[31:0]. 
                                                 Must be aligned on CQE size (32 bytes) */
/* -------------- */
    pseudo_bit_t	reserved1[0x00018];
    pseudo_bit_t	log_cq_size[0x00005];  /* Log (base 2) of the CQ size (in entries) */
    pseudo_bit_t	reserved2[0x00003];
/* -------------- */
    pseudo_bit_t	reserved3[0x00060];
/* -------------- */
    pseudo_bit_t	l_key[0x00020];        /* Memory key (L_Key) to be used to access CQ */
/* -------------- */
    pseudo_bit_t	reserved4[0x00100];
/* -------------- */
}; 

/* SYS_EN Output Parameter */

struct tavorprm_sys_en_out_param_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00020];
/* -------------- */
    pseudo_bit_t	spd[0x00001];          /* 0 - DIMM SPD was read from DIMM
                                                 1 - DIMM SPD was read from InfiniHost NVMEM */
    pseudo_bit_t	sladr[0x00003];        /* SPD Slave Address 3 LSBits. 
                                                 Valid only if spd bit is 0. */
    pseudo_bit_t	sock_num[0x00002];     /* DIMM socket number (for double sided DIMM one of the two numbers will be reported) */
    pseudo_bit_t	syn[0x00004];          /* Error Syndrome
                                                 0 - reserved
                                                 1 - SPD error (e.g. checksum error, no response, error while reading)
                                                 2 - DIMM out of bounds (e.g. DIMM rows number is not between 7 and 14, DIMM type is not 2)
                                                 3 - DIMM conflict (e.g. mix of registered and unbuffered DIMMs, CAS latency conflict)
                                                 4 - Calibration error
                                                 5 - reserved
                                                 6- DDR Memory check failed
                                                 other - Error, reserved */
    pseudo_bit_t	reserved1[0x00016];
/* -------------- */
}; 

/* Query Debug Message */

struct tavorprm_query_debug_msg_st {	/* Little Endian */
    pseudo_bit_t	base_addr_h[0x00020];  /* Debug Buffers Base Address [63:32] */
/* -------------- */
    pseudo_bit_t	base_addr_l[0x00020];  /* Debug Buffers Base Address [31:0] */
/* -------------- */
    pseudo_bit_t	buf_sz[0x00020];       /* Debug Buffer Size (in bytes) */
/* -------------- */
    pseudo_bit_t	reserved0[0x00020];
/* -------------- */
    pseudo_bit_t	trc_hdr_sz[0x00020];   /* Trace message header size in dwords. */
/* -------------- */
    pseudo_bit_t	trc_arg_num[0x00020];  /* The number of arguments per trace message. */
/* -------------- */
    pseudo_bit_t	reserved1[0x000c0];
/* -------------- */
    pseudo_bit_t	dbg_msk_h[0x00020];    /* Debug messages mask [63:32] */
/* -------------- */
    pseudo_bit_t	dbg_msk_l[0x00020];    /* Debug messages mask [31:0] */
/* -------------- */
    pseudo_bit_t	reserved2[0x00040];
/* -------------- */
    pseudo_bit_t	fs_base_addr0_h[0x00020];/* Base address for format string for irisc 0 bits[63:32] */
/* -------------- */
    pseudo_bit_t	fs_base_addr0_l[0x00020];/* Base address for format string for irisc 0 bits[31:0] */
/* -------------- */
    pseudo_bit_t	fs_base_addr1_h[0x00020];/* Base address for format string for irisc 1 bits[63:32] */
/* -------------- */
    pseudo_bit_t	fs_base_addr1_l[0x00020];/* Base address for format string for irisc 1 bits[31:0] */
/* -------------- */
    pseudo_bit_t	fs_base_addr2_h[0x00020];/* Base address for format string for irisc 2 bits[63:32] */
/* -------------- */
    pseudo_bit_t	fs_base_addr2_l[0x00020];/* Base address for format string for irisc 2 bits[31:0] */
/* -------------- */
    pseudo_bit_t	fs_base_addr3_h[0x00020];/* Base address for format string for irisc 3 bits[63:32] */
/* -------------- */
    pseudo_bit_t	fs_base_addr3_l[0x00020];/* Base address for format string for irisc 3 bits[31:0] */
/* -------------- */
    pseudo_bit_t	fs_base_addr4_h[0x00020];/* Base address for format string for irisc 4 bits[63:32] */
/* -------------- */
    pseudo_bit_t	fs_base_addr4_l[0x00020];/* Base address for format string for irisc 4 bits[31:0] */
/* -------------- */
    pseudo_bit_t	fs_base_addr5_h[0x00020];/* Base address for format string for irisc 5 bits[63:32] */
/* -------------- */
    pseudo_bit_t	fs_base_addr5_l[0x00020];/* Base address for format string for irisc 5 bits[31:0] */
/* -------------- */
    pseudo_bit_t	reserved3[0x00480];
/* -------------- */
}; 

/* User Access Region */

struct tavorprm_uar_st {	/* Little Endian */
    struct tavorprm_rd_send_doorbell_st	rd_send_doorbell;/* Reliable Datagram SQ Doorbell */
/* -------------- */
    struct tavorprm_send_doorbell_st	send_doorbell;/* SQ Doorbell */
/* -------------- */
    struct tavorprm_receive_doorbell_st	receive_doorbell;/* RQ Doorbell */
/* -------------- */
    struct tavorprm_cq_cmd_doorbell_st	cq_command_doorbell;/* CQ Doorbell */
/* -------------- */
    struct tavorprm_eq_cmd_doorbell_st	eq_command_doorbell;/* EQ Doorbell */
/* -------------- */
    pseudo_bit_t	reserved0[0x01e80];
/* -------------- */
    pseudo_bit_t	infini_blast[256][0x00020];/* InfiniBlast buffer (same format as WQE format)
                                                 Infiniblast is not supported by InfiniHost MT23108 */
/* -------------- */
}; 

/* SET_IB Parameters */

struct tavorprm_set_ib_st {	/* Little Endian */
    pseudo_bit_t	rqk[0x00001];          /* Reset QKey Violation Counter */
    pseudo_bit_t	reserved0[0x00011];
    pseudo_bit_t	sig[0x00001];          /* Set System Image GUID to system_image_guid specified.
                                                 system_image_guid and sig must be the same for all ports. */
    pseudo_bit_t	reserved1[0x0000d];
/* -------------- */
    pseudo_bit_t	capability_mask[0x00020];/* PortInfo Capability Mask */
/* -------------- */
    pseudo_bit_t	system_image_guid_h[0x00020];/* System Image GUID[63:32], takes effect only if the SIG bit is set
                                                 Must be the same for both ports. */
/* -------------- */
    pseudo_bit_t	system_image_guid_l[0x00020];/* System Image GUID[31:0], takes effect only if the SIG bit is set
                                                 Must be the same for both ports. */
/* -------------- */
    pseudo_bit_t	reserved2[0x00180];
/* -------------- */
}; 

/* Multicast Group Member */

struct tavorprm_mgm_entry_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00006];
    pseudo_bit_t	next_gid_index[0x0001a];/* Index of next Multicast Group Member whose GID maps to same MGID_HASH number.
                                                 The index is into the Multicast Group Table, which is the comprised the MGHT and AMGM tables.
                                                 next_gid_index=0 means end of the chain. */
/* -------------- */
    pseudo_bit_t	reserved1[0x00060];
/* -------------- */
    pseudo_bit_t	mgid_128_96[0x00020];  /* Multicast group GID[128:96] in big endian format.
                                                 Use the Reserved GID 0:0:0:0:0:0:0:0 for an invalid entry. */
/* -------------- */
    pseudo_bit_t	mgid_95_64[0x00020];   /* Multicast group GID[95:64] in big endian format.
                                                 Use the Reserved GID 0:0:0:0:0:0:0:0 for an invalid entry. */
/* -------------- */
    pseudo_bit_t	mgid_63_32[0x00020];   /* Multicast group GID[63:32] in big endian format.
                                                 Use the Reserved GID 0:0:0:0:0:0:0:0 for an invalid entry. */
/* -------------- */
    pseudo_bit_t	mgid_31_0[0x00020];    /* Multicast group GID[31:0] in big endian format.
                                                 Use the Reserved GID 0:0:0:0:0:0:0:0 for an invalid entry. */
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp_0;   /* Multicast Group Member QP */
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp_1;   /* Multicast Group Member QP */
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp_2;   /* Multicast Group Member QP */
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp_3;   /* Multicast Group Member QP */
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp_4;   /* Multicast Group Member QP */
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp_5;   /* Multicast Group Member QP */
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp_6;   /* Multicast Group Member QP */
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp_7;   /* Multicast Group Member QP */
/* -------------- */
}; 

/* INIT_IB Parameters */

struct tavorprm_init_ib_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00004];
    pseudo_bit_t	vl_cap[0x00004];       /* Maximum VLs supported on the port, excluding VL15.
                                                 Legal values are 1,2,4 and 8. */
    pseudo_bit_t	port_width_cap[0x00004];/* IB Port Width
                                                 1   - 1x
                                                 3   - 1x, 4x
                                                 11 - 1x, 4x or 12x (must not be used in InfiniHost MT23108)
                                                 else - Reserved */
    pseudo_bit_t	mtu_cap[0x00004];      /* Maximum MTU Supported
                                                 0x0 - Reserved
                                                 0x1 - 256
                                                 0x2 - 512
                                                 0x3 - 1024
                                                 0x4 - 2048
                                                 0x5 - 0xF Reserved */
    pseudo_bit_t	g0[0x00001];           /* Set port GUID0 to GUID0 specified */
    pseudo_bit_t	ng[0x00001];           /* Set node GUID to node_guid specified.
                                                 node_guid and ng must be the same for all ports. */
    pseudo_bit_t	sig[0x00001];          /* Set System Image GUID to system_image_guid specified.
                                                 system_image_guid and sig must be the same for all ports. */
    pseudo_bit_t	reserved1[0x0000d];
/* -------------- */
    pseudo_bit_t	max_gid[0x00010];      /* Maximum number of GIDs for the port */
    pseudo_bit_t	reserved2[0x00010];
/* -------------- */
    pseudo_bit_t	max_pkey[0x00010];     /* Maximum pkeys for the port.
                                                 Must be the same for both ports. */
    pseudo_bit_t	reserved3[0x00010];
/* -------------- */
    pseudo_bit_t	reserved4[0x00020];
/* -------------- */
    pseudo_bit_t	guid0_h[0x00020];      /* EUI-64 GUID assigned by the manufacturer, takes effect only if the G0 bit is set (bits 63:32) */
/* -------------- */
    pseudo_bit_t	guid0_l[0x00020];      /* EUI-64 GUID assigned by the manufacturer, takes effect only if the G0 bit is set (bits 31:0) */
/* -------------- */
    pseudo_bit_t	node_guid_h[0x00020];  /* Node GUID[63:32], takes effect only if the NG bit is set
                                                 Must be the same for both ports. */
/* -------------- */
    pseudo_bit_t	node_guid_l[0x00020];  /* Node GUID[31:0], takes effect only if the NG bit is set
                                                 Must be the same for both ports. */
/* -------------- */
    pseudo_bit_t	system_image_guid_h[0x00020];/* System Image GUID[63:32], takes effect only if the SIG bit is set
                                                 Must be the same for both ports. */
/* -------------- */
    pseudo_bit_t	system_image_guid_l[0x00020];/* System Image GUID[31:0], takes effect only if the SIG bit is set
                                                 Must be the same for both ports. */
/* -------------- */
    pseudo_bit_t	reserved5[0x006c0];
/* -------------- */
}; 

/* Query Device Limitations */

struct tavorprm_query_dev_lim_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00080];
/* -------------- */
    pseudo_bit_t	log_max_qp[0x00005];   /* Log2 of the Maximum number of QPs supported */
    pseudo_bit_t	reserved1[0x00003];
    pseudo_bit_t	log2_rsvd_qps[0x00004];/* Log (base 2) of the number of QPs reserved for firmware use
                                                 The reserved resources are numbered from 0 to 2^log2_rsvd_qps-1 */
    pseudo_bit_t	reserved2[0x00004];
    pseudo_bit_t	log_max_qp_sz[0x00008];/* The maximum number of WQEs allowed on the RQ or the SQ is 2^log_max_qp_sz-1 */
    pseudo_bit_t	log_max_srq_sz[0x00008];/* The maximum number of WQEs allowed on the SRQ is 2^log_max_srq_sz-1 */
/* -------------- */
    pseudo_bit_t	log_max_ee[0x00005];   /* Log2 of the Maximum number of EE contexts supported */
    pseudo_bit_t	reserved3[0x00003];
    pseudo_bit_t	log2_rsvd_ees[0x00004];/* Log (base 2) of the number of EECs reserved for firmware use
                                                 The reserved resources are numbered from 0 to 2^log2_rsvd_ees-1 */
    pseudo_bit_t	reserved4[0x00004];
    pseudo_bit_t	log_max_srqs[0x00005]; /* Log base 2 of the maximum number of SRQs supported, valid only if SRQ bit is set.
                                                  */
    pseudo_bit_t	reserved5[0x00007];
    pseudo_bit_t	log2_rsvd_srqs[0x00004];/* Log (base 2) of the number of reserved SRQs for firmware use
                                                 The reserved resources are numbered from 0 to 2^log2_rsvd_srqs-1
                                                 This parameter is valid only if the SRQ bit is set. */
/* -------------- */
    pseudo_bit_t	log_max_cq[0x00005];   /* Log2 of the Maximum number of CQs supported */
    pseudo_bit_t	reserved6[0x00003];
    pseudo_bit_t	log2_rsvd_cqs[0x00004];/* Log (base 2) of the number of CQs reserved for firmware use
                                                 The reserved resources are numbered from 0 to 2^log2_rsrvd_cqs-1 */
    pseudo_bit_t	reserved7[0x00004];
    pseudo_bit_t	log_max_cq_sz[0x00008];/* Log2 of the Maximum CQEs allowed in a CQ */
    pseudo_bit_t	reserved8[0x00008];
/* -------------- */
    pseudo_bit_t	log_max_eq[0x00003];   /* Log2 of the Maximum number of EQs */
    pseudo_bit_t	reserved9[0x00005];
    pseudo_bit_t	num_rsvd_eqs[0x00004]; /* The number of EQs reserved for firmware use
                                                 The reserved resources are numbered from 0 to num_rsvd_eqs-1
                                                 If 0 - no resources are reserved. */
    pseudo_bit_t	reserved10[0x00004];
    pseudo_bit_t	log_max_mpts[0x00006]; /* Log (base 2) of the maximum number of MPT entries (the number of Regions/Windows) */
    pseudo_bit_t	reserved11[0x0000a];
/* -------------- */
    pseudo_bit_t	log_max_mtt_seg[0x00006];/* Log2 of the Maximum number of MTT segments */
    pseudo_bit_t	reserved12[0x00002];
    pseudo_bit_t	log2_rsvd_mrws[0x00004];/* Log (base 2) of the number of MPTs reserved for firmware use
                                                 The reserved resources are numbered from 0 to 2^log2_rsvd_mrws-1 */
    pseudo_bit_t	reserved13[0x00004];
    pseudo_bit_t	log_max_mrw_sz[0x00008];/* Log2 of the Maximum Size of Memory Region/Window */
    pseudo_bit_t	reserved14[0x00004];
    pseudo_bit_t	log2_rsvd_mtts[0x00004];/* Log (base 2) of the number of MTT segments reserved for firmware use
                                                 The reserved resources are numbered from 0 to 2^log2_rsvd_mtts-1
                                                  */
/* -------------- */
    pseudo_bit_t	log_max_av[0x00006];   /* Log2 of the Maximum number of Address Vectors */
    pseudo_bit_t	reserved15[0x0001a];
/* -------------- */
    pseudo_bit_t	log_max_ra_res_qp[0x00006];/* Log2 of the Maximum number of outstanding RDMA read/Atomic per QP as a responder */
    pseudo_bit_t	reserved16[0x0000a];
    pseudo_bit_t	log_max_ra_req_qp[0x00006];/* Log2 of the maximum number of outstanding RDMA read/Atomic per QP as a requester */
    pseudo_bit_t	reserved17[0x0000a];
/* -------------- */
    pseudo_bit_t	log_max_ra_res_global[0x00006];/* Log2 of the maximum number of RDMA read/atomic operations the HCA responder can support globally. That implies the RDB table size. */
    pseudo_bit_t	reserved18[0x0001a];
/* -------------- */
    pseudo_bit_t	reserved19[0x00020];
/* -------------- */
    pseudo_bit_t	num_ports[0x00004];    /* Number of IB ports. */
    pseudo_bit_t	max_vl[0x00004];       /* Maximum VLs supported on each port, excluding VL15 */
    pseudo_bit_t	max_port_width[0x00004];/* IB Port Width
                                                 1   - 1x
                                                 3   - 1x, 4x
                                                 11 - 1x, 4x or 12x
                                                 else - Reserved */
    pseudo_bit_t	max_mtu[0x00004];      /* Maximum MTU Supported
                                                 0x0 - Reserved
                                                 0x1 - 256
                                                 0x2 - 512
                                                 0x3 - 1024
                                                 0x4 - 2048
                                                 0x5 - 0xF Reserved */
    pseudo_bit_t	local_ca_ack_delay[0x00005];/* The Local CA ACK Delay. This is the value recommended to be returned in Query HCA verb.
                                                 The delay value in microseconds is computed using 4.096us * 2^(Local_CA_ACK_Delay). */
    pseudo_bit_t	reserved20[0x0000b];
/* -------------- */
    pseudo_bit_t	log_max_gid[0x00004];  /* Log2 of the maximum number of GIDs per port */
    pseudo_bit_t	reserved21[0x0001c];
/* -------------- */
    pseudo_bit_t	log_max_pkey[0x00004]; /* Log2 of the max PKey Table Size (per IB port) */
    pseudo_bit_t	reserved22[0x0001c];
/* -------------- */
    pseudo_bit_t	reserved23[0x00020];
/* -------------- */
    pseudo_bit_t	rc[0x00001];           /* RC Transport supported */
    pseudo_bit_t	uc[0x00001];           /* UC Transport Supported */
    pseudo_bit_t	ud[0x00001];           /* UD Transport Supported */
    pseudo_bit_t	rd[0x00001];           /* RD Transport Supported
                                                 RD is not supported in InfiniHost MT23108 */
    pseudo_bit_t	raw_ipv6[0x00001];     /* Raw IPv6 Transport Supported */
    pseudo_bit_t	raw_ether[0x00001];    /* Raw Ethertype Transport Supported */
    pseudo_bit_t	srq[0x00001];          /* SRQ is supported
                                                  */
    pseudo_bit_t	reserved24[0x00001];
    pseudo_bit_t	pkv[0x00001];          /* PKey Violation Counter Supported */
    pseudo_bit_t	qkv[0x00001];          /* QKey Violation Coutner Supported */
    pseudo_bit_t	reserved25[0x00006];
    pseudo_bit_t	mw[0x00001];           /* Memory windows supported */
    pseudo_bit_t	apm[0x00001];          /* Automatic Path Migration Supported */
    pseudo_bit_t	atm[0x00001];          /* Atomic operations supported (atomicity is guaranteed between QPs on this HCA) */
    pseudo_bit_t	rm[0x00001];           /* Raw Multicast Supported */
    pseudo_bit_t	avp[0x00001];          /* Address Vector Port checking supported */
    pseudo_bit_t	udm[0x00001];          /* UD Multicast Supported */
    pseudo_bit_t	reserved26[0x00002];
    pseudo_bit_t	pg[0x00001];           /* Paging on demand supported */
    pseudo_bit_t	r[0x00001];            /* Router mode supported */
    pseudo_bit_t	reserved27[0x00006];
/* -------------- */
    pseudo_bit_t	log_pg_sz[0x00008];    /* Minimum system page size supported (log2) . 
                                                 For proper operation it must be less than or equal the hosting platform (CPU) minimum page size. */
    pseudo_bit_t	reserved28[0x00008];
    pseudo_bit_t	uar_sz[0x00006];       /* UAR Area Size = 1MB * 2^uar_sz */
    pseudo_bit_t	reserved29[0x00006];
    pseudo_bit_t	num_rsvd_uars[0x00004];/* The number of UARs reserved for firmware use
                                                 The reserved resources are numbered from 0 to num_reserved_uars-1
                                                 Note that UAR 1 is always for the kernel
                                                 If 0 - no resources are reserved. */
/* -------------- */
    pseudo_bit_t	reserved30[0x00020];
/* -------------- */
    pseudo_bit_t	max_desc_sz[0x00010];  /* Max descriptor size in bytes */
    pseudo_bit_t	max_sg[0x00008];       /* The maximum S/G list elements in a WQE (max_desc_sz/16 - 3) */
    pseudo_bit_t	reserved31[0x00008];
/* -------------- */
    pseudo_bit_t	reserved32[0x00060];
/* -------------- */
    pseudo_bit_t	log_max_mcg[0x00008];  /* Log2 of the maximum number of multicast groups */
    pseudo_bit_t	num_rsvd_mcgs[0x00004];/* The number of MGMs reserved for firmware use in the MGHT.
                                                 The reserved resources are numbered from 0 to num_reserved_mcgs-1
                                                 If 0 - no resources are reserved. */
    pseudo_bit_t	reserved33[0x00004];
    pseudo_bit_t	log_max_qp_mcg[0x00008];/* Log2 of the maximum number of QPs per multicast group */
    pseudo_bit_t	reserved34[0x00008];
/* -------------- */
    pseudo_bit_t	log_max_rdds[0x00006]; /* Log2 of the maximum number of RDDs */
    pseudo_bit_t	reserved35[0x00006];
    pseudo_bit_t	num_rsvd_rdds[0x00004];/* The number of RDDs reserved for firmware use
                                                 The reserved resources are numbered from 0 to num_reserved_rdds-1.
                                                 If 0 - no resources are reserved. */
    pseudo_bit_t	log_max_pd[0x00006];   /* Log2 of the maximum number of PDs */
    pseudo_bit_t	reserved36[0x00006];
    pseudo_bit_t	num_rsvd_pds[0x00004]; /* The number of PDs reserved for firmware use
                                                 The reserved resources are numbered from 0 to num_reserved_pds-1
                                                 If 0 - no resources are reserved. */
/* -------------- */
    pseudo_bit_t	reserved37[0x000c0];
/* -------------- */
    pseudo_bit_t	qpc_entry_sz[0x00010]; /* QPC Entry Size for the device
                                                 For the InfiniHost MT23108 entry size is 256 bytes */
    pseudo_bit_t	eec_entry_sz[0x00010]; /* EEC Entry Size for the device
                                                 For  the InfiniHost MT23108 entry size is 256 bytes */
/* -------------- */
    pseudo_bit_t	eqpc_entry_sz[0x00010];/* Extended QPC entry size for the device
                                                 For  the InfiniHost MT23108 entry size is 32 bytes */
    pseudo_bit_t	eeec_entry_sz[0x00010];/* Extended EEC entry size for the device
                                                 For  the InfiniHost MT23108 entry size is 32 bytes */
/* -------------- */
    pseudo_bit_t	cqc_entry_sz[0x00010]; /* CQC entry size for the device
                                                 For  the InfiniHost MT23108 entry size is 64 bytes */
    pseudo_bit_t	eqc_entry_sz[0x00010]; /* EQ context entry size for the device
                                                 For  the InfiniHost MT23108 entry size is 64 bytes */
/* -------------- */
    pseudo_bit_t	uar_scratch_entry_sz[0x00010];/* UAR Scratchpad Entry Size
                                                 For  the InfiniHost MT23108 entry size is 32 bytes */
    pseudo_bit_t	srq_entry_sz[0x00010]; /* SRQ context entry size for the device
                                                 For  the InfiniHost MT23108 entry size is 32 bytes */
/* -------------- */
    pseudo_bit_t	reserved38[0x00380];
/* -------------- */
}; 

/* QUERY_ADAPTER Parameters Block */

struct tavorprm_query_adapter_st {	/* Little Endian */
    pseudo_bit_t	vendor_id[0x00020];    /* Adapter vendor ID */
/* -------------- */
    pseudo_bit_t	device_id[0x00020];    /* Adapter Device ID */
/* -------------- */
    pseudo_bit_t	revision_id[0x00020];  /* Adapter Revision ID */
/* -------------- */
    pseudo_bit_t	reserved0[0x00020];
/* -------------- */
    pseudo_bit_t	reserved1[0x00018];
    pseudo_bit_t	intapin[0x00008];      /* Interrupt Signal ID of HCA device pin that is connected to the INTA trace in the HCA board.
                                                 0..39 and 63 are valid values
                                                 255 means INTA trace in board is not connected to the HCA device.
                                                 All other values are reserved */
/* -------------- */
    pseudo_bit_t	mode_pci[0x00001];     /* Set when the device is operating in conventional PCI mode (as opposed to PCI-X/PCI-Express). */
    pseudo_bit_t	mode_32bit[0x00001];   /* Set when the device is operating in 32 bit mode (the sampled bus width is 32 bit). */
    pseudo_bit_t	reserved2[0x0001e];
/* -------------- */
    pseudo_bit_t	reserved3[0x00040];
/* -------------- */
    struct tavorprm_vsd_st	vsd;
/* -------------- */
}; 

/* QUERY_FW Parameters Block */

struct tavorprm_query_fw_st {	/* Little Endian */
    pseudo_bit_t	fw_rev_major[0x00010]; /* Firmware Revision - Major */
    pseudo_bit_t	reserved0[0x00010];
/* -------------- */
    pseudo_bit_t	fw_rev_minor[0x00010]; /* Firmware Revision - Minor */
    pseudo_bit_t	fw_rev_subminor[0x00010];/* Firmware Sub-minor version (Patch level). */
/* -------------- */
    pseudo_bit_t	cmd_interface_rev[0x00010];/* Command Interface Interpreter Revision ID */
    pseudo_bit_t	reserved1[0x00010];
/* -------------- */
    pseudo_bit_t	log_max_outstanding_cmd[0x00008];/* Log2 of the maximum number of commands the HCR can support simultaneously */
    pseudo_bit_t	reserved2[0x00017];
    pseudo_bit_t	dt[0x00001];           /* Debug Trace Support
                                                 0 - Debug trace is not supported 
                                                 1 - Debug trace is supported */
/* -------------- */
    pseudo_bit_t	cmd_interface_db[0x00001];/* Set if the device accepts commands by means of special doorbells. */
    pseudo_bit_t	reserved3[0x0001f];
/* -------------- */
    pseudo_bit_t	reserved4[0x00060];
/* -------------- */
    pseudo_bit_t	fw_base_addr_h[0x00020];/* Physical Address of Firmware Area in DDR Memory [63:32] */
/* -------------- */
    pseudo_bit_t	fw_base_addr_l[0x00020];/* Physical Address of Firmware Area in DDR Memory [31:0] */
/* -------------- */
    pseudo_bit_t	fw_end_addr_h[0x00020];/* End of firmware address in DDR memory [63:32] */
/* -------------- */
    pseudo_bit_t	fw_end_addr_l[0x00020];/* End of firmware address in DDR memory [31:0] */
/* -------------- */
    pseudo_bit_t	error_buf_start_h[0x00020];/* Read Only buffer for catastrofic error reports. */
/* -------------- */
    pseudo_bit_t	error_buf_start_l[0x00020];
/* -------------- */
    pseudo_bit_t	error_buf_size[0x00020];/* Size in words */
/* -------------- */
    pseudo_bit_t	reserved5[0x000a0];
/* -------------- */
    pseudo_bit_t	cmd_db_dw1[0x00010];   /* offset in bytes from cmd_db_addr_base where DWord 1 of a Command Interface Doorbell should be written. Valid only if CmdInterfaceDb bit is '1' */
    pseudo_bit_t	cmd_db_dw0[0x00010];   /* offset in bytes from cmd_db_addr_base where DWord 0 of a Command Interface Doorbell should be written. Valid only if CmdInterfaceDb bit is '1' */
/* -------------- */
    pseudo_bit_t	cmd_db_dw3[0x00010];   /* offset in bytes from cmd_db_addr_base where DWord 3 of a Command Interface Doorbell should be written. Valid only if CmdInterfaceDb bit is '1' */
    pseudo_bit_t	cmd_db_dw2[0x00010];   /* offset in bytes from cmd_db_addr_base where DWord 2 of a Command Interface Doorbell should be written. Valid only if CmdInterfaceDb bit is '1' */
/* -------------- */
    pseudo_bit_t	cmd_db_dw5[0x00010];   /* offset in bytes from cmd_db_addr_base where DWord 5 of a Command Interface Doorbell should be written. Valid only if CmdInterfaceDb bit is '1' */
    pseudo_bit_t	cmd_db_dw4[0x00010];   /* offset in bytes from cmd_db_addr_base where DWord 4 of a Command Interface Doorbell should be written. Valid only if CmdInterfaceDb bit is '1' */
/* -------------- */
    pseudo_bit_t	cmd_db_dw7[0x00010];   /* offset in bytes from cmd_db_addr_base where DWord 7 of a Command Interface Doorbell should be written. Valid only if CmdInterfaceDb bit is '1' */
    pseudo_bit_t	cmd_db_dw6[0x00010];   /* offset in bytes from cmd_db_addr_base where DWord 6 of a Command Interface Doorbell should be written. Valid only if CmdInterfaceDb bit is '1' */
/* -------------- */
    pseudo_bit_t	cmd_db_addr_base_h[0x00020];/* High bits of cmd_db_addr_base, which cmd_db_dw offsets refer to. Valid only if CmdInterfaceDb bit is '1' */
/* -------------- */
    pseudo_bit_t	cmd_db_addr_base_l[0x00020];/* Low  bits of cmd_db_addr_base, which cmd_db_dw offsets refer to. Valid only if CmdInterfaceDb bit is '1' */
/* -------------- */
    pseudo_bit_t	reserved6[0x004c0];
/* -------------- */
}; 

/* ACCESS_DDR */

struct tavorprm_access_ddr_st {	/* Little Endian */
    struct tavorprm_access_ddr_inject_errors_st	access_ddr_inject_errors;
/* -------------- */
    pseudo_bit_t	reserved0[0x00080];
/* -------------- */
}; 

/* QUERY_DDR Parameters Block */

struct tavorprm_query_ddr_st {	/* Little Endian */
    pseudo_bit_t	ddr_start_adr_h[0x00020];/* DDR memory start address [63:32] */
/* -------------- */
    pseudo_bit_t	ddr_start_adr_l[0x00020];/* DDR memory start address [31:0] */
/* -------------- */
    pseudo_bit_t	ddr_end_adr_h[0x00020];/* DDR memory end address [63:32] */
/* -------------- */
    pseudo_bit_t	ddr_end_adr_l[0x00020];/* DDR memory end address [31:0] */
/* -------------- */
    pseudo_bit_t	di[0x00002];           /* Data Integrity Configuration:
                                                 00 - none
                                                 01 - Parity
                                                 10 - ECC Detection Only
                                                 11 - ECC With Correction */
    pseudo_bit_t	ap[0x00002];           /* Auto Precharge Mode
                                                 00 - No auto precharge
                                                 01 - Auto precharge per transaction
                                                 10 - Auto precharge per 64 bytes
                                                 11 - reserved */
    pseudo_bit_t	dh[0x00001];           /* When set, DDR is Hidden and can not be accessed from the PCI bus. */
    pseudo_bit_t	reserved0[0x0001b];
/* -------------- */
    pseudo_bit_t	reserved1[0x00160];
/* -------------- */
    struct tavorprm_dimminfo_st	dimm0;  /* Logical DIMM 0 Parameters */
/* -------------- */
    struct tavorprm_dimminfo_st	dimm1;  /* Logical DIMM 1 Parameters */
/* -------------- */
    struct tavorprm_dimminfo_st	dimm2;  /* Logical DIMM 2 Parameters */
/* -------------- */
    struct tavorprm_dimminfo_st	dimm3;  /* Logical DIMM 3 Parameters */
/* -------------- */
    pseudo_bit_t	reserved2[0x00200];
/* -------------- */
}; 

/* INIT_HCA & QUERY_HCA Parameters Block */

struct tavorprm_init_hca_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00060];
/* -------------- */
    pseudo_bit_t	reserved1[0x00018];
    pseudo_bit_t	hca_core_clock[0x00008];/* Internal Clock Period (in units of 1/16 ns) (QUERY_HCA only) */
/* -------------- */
    pseudo_bit_t	reserved2[0x00008];
    pseudo_bit_t	router_qp[0x00010];    /* Upper 16 bit to be used as a QP number for router mode. Low order 8 bits are taken from the TClass field of the incoming packet.
                                                 Valid only if RE bit is set */
    pseudo_bit_t	reserved3[0x00007];
    pseudo_bit_t	re[0x00001];           /* Router Mode Enable
                                                 If this bit is set, entire packet (including all headers and ICRC) will be considered as a data payload and will be scattered to memory as specified in the descriptor that is posted on the QP matching the TClass field of packet. */
/* -------------- */
    pseudo_bit_t	udp[0x00001];          /* UD Port Check Enable
                                                 0 - Port field in Address Vector is ignored
                                                 1 - HCA will check the port field in AV entry (fetched for UD descriptor) against the Port of the UD QP executing the descriptor. */
    pseudo_bit_t	he[0x00001];           /* Host Endianess - Used for Atomic Operations
                                                 0 - Host is Little Endian
                                                 1 - Host is Big endian
                                                  */
    pseudo_bit_t	ud[0x00001];           /* Force UD address vector protection check. If this bit is set, Passing address vector as immediate data in WQE is suppressed and privileged memory key will be used by hardware to access UD address vector table. */
    pseudo_bit_t	reserved4[0x00005];
    pseudo_bit_t	responder_exu[0x00004];/* How many execution engines are dedicated to the responder. Legal values are 0x0-0xF. 0 is "auto" */
    pseudo_bit_t	reserved5[0x00004];
    pseudo_bit_t	wqe_quota[0x0000f];    /* Maximum number of WQEs that are executed prior to preemption of execution unit. 0 - reserved. */
    pseudo_bit_t	wqe_quota_en[0x00001]; /* If set - wqe_quota field is used. If cleared - WQE quota is set to "auto" value */
/* -------------- */
    pseudo_bit_t	reserved6[0x00040];
/* -------------- */
    struct tavorprm_qpcbaseaddr_st	qpc_eec_cqc_eqc_rdb_parameters;
/* -------------- */
    pseudo_bit_t	reserved7[0x00080];
/* -------------- */
    struct tavorprm_udavtable_memory_parameters_st	udavtable_memory_parameters;/* Memory Access Parameters for UD Address Vector Table. Used for QPs/EEc that are configured to use protected Address Vectors. */
/* -------------- */
    pseudo_bit_t	reserved8[0x00040];
/* -------------- */
    struct tavorprm_multicastparam_st	multicast_parameters;
/* -------------- */
    pseudo_bit_t	reserved9[0x00080];
/* -------------- */
    struct tavorprm_tptparams_st	tpt_parameters;
/* -------------- */
    pseudo_bit_t	reserved10[0x00080];
/* -------------- */
    struct tavorprm_uar_params_st	uar_parameters;/* UAR Parameters */
/* -------------- */
    pseudo_bit_t	reserved11[0x00600];
/* -------------- */
}; 

/* Event Queue Context Table Entry */

struct tavorprm_eqc_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00008];
    pseudo_bit_t	st[0x00002];           /* Event delivery state machine
                                                 01 - Armed
                                                 10 - Fired
                                                 11 - Always_Armed (auto-rearm)
                                                 00 - Reserved */
    pseudo_bit_t	reserved1[0x00007];
    pseudo_bit_t	oi[0x00001];           /* Ignore overrun on this EQ if this bit is set */
    pseudo_bit_t	tr[0x00001];           /* Translation Required. If set - EQ access undergo address translation. */
    pseudo_bit_t	reserved2[0x00005];
    pseudo_bit_t	owner[0x00004];        /* 0 - SW ownership
                                                 1 - HW ownership
                                                 Valid for the QUERY_EQ and HW2SW_EQ commands only */
    pseudo_bit_t	status[0x00004];       /* EQ status:
                                                 0000 - OK
                                                 1001 - EQ overflow
                                                 1010 - EQ write failure
                                                 Valid for the QUERY_EQ and HW2SW_EQ commands only */
/* -------------- */
    pseudo_bit_t	start_address_h[0x00020];/* Start Address of Event Queue[63:32]. 
                                                 Must be aligned on 32-byte boundary */
/* -------------- */
    pseudo_bit_t	start_address_l[0x00020];/* Start Address of Event Queue[31:0]. 
                                                 Must be aligned on 32-byte boundary */
/* -------------- */
    pseudo_bit_t	usr_page[0x00018];
    pseudo_bit_t	log_eq_size[0x00005];  /* Amount of entries in this EQ is 2^log_eq_size.
                                                 Log_eq_size must be bigger than 1 */
    pseudo_bit_t	reserved3[0x00003];
/* -------------- */
    pseudo_bit_t	pd[0x00018];           /* PD to be used to access EQ */
    pseudo_bit_t	reserved4[0x00008];
/* -------------- */
    pseudo_bit_t	intr[0x00008];         /* Interrupt (message) to be generated to report event to INT layer.
                                                 00iiiiii - specifies GPIO pin to be asserted (according to INTA given in QUERY_ADAPTER)
                                                 10jjjjjj - specificies type of interrupt message to be generated (total 64 different messages supported).
                                                 
                                                 If interrupt generation is not required one of the two following options should be set:
                                                 1. ST must be set on creation to Fired state and not EQ arming doorbell should be performed. In this case hardware will not generate any interrupt.
                                                 2. intr should be set to 60 decimal
                                                  */
    pseudo_bit_t	reserved5[0x00018];
/* -------------- */
    pseudo_bit_t	lost_count[0x00020];   /* Number of events lost due to EQ overrun */
/* -------------- */
    pseudo_bit_t	lkey[0x00020];         /* Memory key (L-Key) to be used to access EQ */
/* -------------- */
    pseudo_bit_t	reserved6[0x00040];
/* -------------- */
    pseudo_bit_t	consumer_indx[0x00020];/* Contains next entry to be read upon polling the event queue.
                                                 Must be initalized to '0 while opening EQ */
/* -------------- */
    pseudo_bit_t	producer_indx[0x00020];/* Contains next entry in EQ to be written by the HCA.
                                                 Must be initalized to '0 while opening EQ. */
/* -------------- */
    pseudo_bit_t	reserved7[0x00080];
/* -------------- */
}; 

/* Memory Translation Table (MTT) Entry */

struct tavorprm_mtt_st {	/* Little Endian */
    pseudo_bit_t	ptag_h[0x00020];       /* High-order bits of physical tag. The size of the field depends on the page size of the region. Maximum PTAG size is 52 bits. */
/* -------------- */
    pseudo_bit_t	p[0x00001];            /* Present bit. If set, page entry is valid. If cleared, access to this page will generate 'non-present page access fault'. */
    pseudo_bit_t	reserved0[0x0000b];
    pseudo_bit_t	ptag_l[0x00014];       /* Low-order bits of Physical tag. The size of the field depends on the page size of the region. Maximum PTAG size is 52 bits. */
/* -------------- */
}; 

/* Memory Protection Table (MPT) Entry */

struct tavorprm_mpt_st {	/* Little Endian */
    pseudo_bit_t	ver[0x00004];          /* Version. Must be zero for InfiniHost */
    pseudo_bit_t	reserved0[0x00004];
    pseudo_bit_t	r_w[0x00001];          /* Defines whether this entry is Region (1) or Window (0) */
    pseudo_bit_t	pa[0x00001];           /* Physical address. If set, no virtual-to-physical address translation will be performed for this region */
    pseudo_bit_t	lr[0x00001];           /* If set - local read access enabled */
    pseudo_bit_t	lw[0x00001];           /* If set - local write access enabled */
    pseudo_bit_t	rr[0x00001];           /* If set - Remote read access enabled. */
    pseudo_bit_t	rw[0x00001];           /* If set - remote write access enabled */
    pseudo_bit_t	a[0x00001];            /* If set - Remote Atomic access is enabled */
    pseudo_bit_t	eb[0x00001];           /* If set - Bind is enabled. Valid for region entry only. */
    pseudo_bit_t	reserved1[0x00001];
    pseudo_bit_t	m_io[0x00001];         /* Memory / I/O
                                                 1 - Memory commands used on the uplink bus
                                                 0 - I/O commands used on the uplink bus
                                                 Must be 1 for the InfiniHost MT23108. */
    pseudo_bit_t	reserved2[0x0000a];
    pseudo_bit_t	status[0x00004];       /* Regios/Window Status
                                                 0xF - not valid (SW ownership)
                                                 else - HW ownership
                                                 Note that an unbound Window is denoted by the reg_wnd_len field equals zero. */
/* -------------- */
    pseudo_bit_t	page_size[0x00005];    /* Page size used for the region. Actual size is [4K]*2^Page_size bytes.
                                                 page_size should be less than 20. */
    pseudo_bit_t	reserved3[0x00002];
    pseudo_bit_t	reserved4[0x00001];
    pseudo_bit_t	reserved5[0x00018];
/* -------------- */
    pseudo_bit_t	mem_key[0x00020];      /* The memory Key. This field is compared to key used to access the region/window. Lower-order bits are restricted (index to the table). */
/* -------------- */
    pseudo_bit_t	pd[0x00018];           /* Protection Domain */
    pseudo_bit_t	reserved6[0x00001];
    pseudo_bit_t	reserved7[0x00001];
    pseudo_bit_t	reserved8[0x00001];
    pseudo_bit_t	reserved9[0x00001];
    pseudo_bit_t	reserved10[0x00001];
    pseudo_bit_t	reserved11[0x00003];
/* -------------- */
    pseudo_bit_t	start_address_h[0x00020];/* Start Address[63:32] - Virtual Address where this region/window starts */
/* -------------- */
    pseudo_bit_t	start_address_l[0x00020];/* Start Address[31:0] - Virtual Address where this region/window starts */
/* -------------- */
    pseudo_bit_t	reg_wnd_len_h[0x00020];/* Region/Window Length[63:32] */
/* -------------- */
    pseudo_bit_t	reg_wnd_len_l[0x00020];/* Region/Window Length[31:0] */
/* -------------- */
    pseudo_bit_t	lkey[0x00020];         /* Must be 0 for SW2HW_MPT.
                                                 On QUERY_MPT and HW2SW_MPT commands for Memory Window it reflects the LKey of the Region that the Window is bound to. */
/* -------------- */
    pseudo_bit_t	win_cnt[0x00020];      /* Number of windows bound to this region. Valid for regions only.
                                                 The field is valid only for the QUERY_MPT and HW2SW_MPT commands. */
/* -------------- */
    pseudo_bit_t	win_cnt_limit[0x00020];/* The number of windows (limit) that can be bound to this region. If a bind operation is attempted when WIN_CNT == WIN_CNT_LIMIT, the operation will be aborted, a CQE with error will be generated, and the QP will be moved into the error state.
                                                 Zero means no limit.
                                                 Note that for best hardware performance, win_cnt_limit should be set to zero. */
/* -------------- */
    pseudo_bit_t	mtt_seg_adr_h[0x00020];/* Base (first) address of the MTT segment, aligned on segment_size boundary (bits 63:31). */
/* -------------- */
    pseudo_bit_t	reserved12[0x00006];
    pseudo_bit_t	mtt_seg_adr_l[0x0001a];/* Base (first) address of the MTT segment, aligned on segment_size boundary (bits 31:6). */
/* -------------- */
    pseudo_bit_t	reserved13[0x00060];
/* -------------- */
}; 

/* Completion Queue Context Table Entry */

struct tavorprm_completion_queue_context_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00008];
    pseudo_bit_t	st[0x00004];           /* Event delivery state machine
                                                 0x0 - DISARMED
                                                 0x1 - ARMED (Request for Notification)
                                                 0x4 - ARMED SOLICITED (Request Solicited Notification)
                                                 0xA - FIRED
                                                 other - reserved */
    pseudo_bit_t	reserved1[0x00005];
    pseudo_bit_t	oi[0x00001];           /* Ignore overrun of this CQ if this bit is set */
    pseudo_bit_t	tr[0x00001];           /* Translation Required
                                                 1 - accesses to CQ will undergo address translation
                                                 0 - accesses to CQ will not undergo address translation */
    pseudo_bit_t	reserved2[0x00009];
    pseudo_bit_t	status[0x00004];       /* CQ status
                                                 0000 -  OK
                                                 1001 - CQ overflow
                                                 1010 - CQ write failure
                                                 Valid for the QUERY_CQ and HW2SW_CQ commands only */
/* -------------- */
    pseudo_bit_t	start_address_h[0x00020];/* Start address of CQ[63:32]. 
                                                 Must be aligned on CQE size (32 bytes) */
/* -------------- */
    pseudo_bit_t	start_address_l[0x00020];/* Start address of CQ[31:0]. 
                                                 Must be aligned on CQE size (32 bytes) */
/* -------------- */
    pseudo_bit_t	usr_page[0x00018];     /* UAR page this CQ can be accessed through (ringinig CQ doorbells) */
    pseudo_bit_t	log_cq_size[0x00005];  /* Log (base 2) of the CQ size (in entries).
                                                 Maximum CQ size is 128K CQEs (max log_cq_size is 17) */
    pseudo_bit_t	reserved3[0x00003];
/* -------------- */
    pseudo_bit_t	e_eqn[0x00008];        /* Event Queue this CQ reports errors to (e.g. CQ overflow)
                                                 Valid values are 0 to 63
                                                 If configured to value other than 0-63, error events will not be reported on the CQ. */
    pseudo_bit_t	reserved4[0x00018];
/* -------------- */
    pseudo_bit_t	c_eqn[0x00008];        /* Event Queue this CQ reports completion events to.
                                                 Valid values are 0 to 63
                                                 If configured to value other than 0-63, completion events will not be reported on the CQ. */
    pseudo_bit_t	reserved5[0x00018];
/* -------------- */
    pseudo_bit_t	pd[0x00018];           /* Protection Domain to be used to access CQ.
                                                 Must be the same PD of the CQ L_Key. */
    pseudo_bit_t	reserved6[0x00008];
/* -------------- */
    pseudo_bit_t	l_key[0x00020];        /* Memory key (L_Key) to be used to access CQ */
/* -------------- */
    pseudo_bit_t	last_notified_indx[0x00020];/* Maintained by HW.
                                                 Valid for QUERY_CQ and HW2SW_CQ commands only. */
/* -------------- */
    pseudo_bit_t	solicit_producer_indx[0x00020];/* Maintained by HW.
                                                 Valid for QUERY_CQ and HW2SW_CQ commands only. 
                                                  */
/* -------------- */
    pseudo_bit_t	consumer_indx[0x00020];/* Contains index to the next entry to be read upon poll for completion. The first completion after passing ownership of CQ from software to hardware will be reported to value passed in this field. Only the low log_cq_size bits may be non-zero. */
/* -------------- */
    pseudo_bit_t	producer_indx[0x00020];/* Points to the next entry to be written to by Hardware. CQ overrun is reported if Producer_indx + 1 equals to Consumer_indx. 
                                                 Maintained by HW (valid for the QUERY_CQ and HW2SW_CQ commands only) */
/* -------------- */
    pseudo_bit_t	cqn[0x00018];          /* CQ number. Least significant bits are constrained by the position of this CQ in CQC table
                                                 Valid for the QUERY_CQ and HW2SW_CQ commands only */
    pseudo_bit_t	reserved7[0x00008];
/* -------------- */
    pseudo_bit_t	reserved8[0x00060];
/* -------------- */
}; 

/* UD Address Vector */

struct tavorprm_ud_address_vector_st {	/* Little Endian */
    pseudo_bit_t	pd[0x00018];           /* Protection Domain */
    pseudo_bit_t	port_number[0x00002];  /* Port number
                                                 1 - Port 1
                                                 2 - Port 2
                                                 other - reserved */
    pseudo_bit_t	reserved0[0x00006];
/* -------------- */
    pseudo_bit_t	rlid[0x00010];         /* Remote (Destination) LID */
    pseudo_bit_t	my_lid_path_bits[0x00007];/* Source LID - the lower 7 bits (upper bits are taken from PortInfo) */
    pseudo_bit_t	g[0x00001];            /* Global address enable - if set, GRH will be formed for packet header */
    pseudo_bit_t	reserved1[0x00008];
/* -------------- */
    pseudo_bit_t	hop_limit[0x00008];    /* IPv6 hop limit */
    pseudo_bit_t	max_stat_rate[0x00003];/* Maximum static rate control. 
                                                 0 - 4X injection rate
                                                 1 - 1X injection rate
                                                 other - reserved
                                                  */
    pseudo_bit_t	reserved2[0x00001];
    pseudo_bit_t	msg[0x00002];          /* Max Message size, size is 256*2^MSG bytes */
    pseudo_bit_t	reserved3[0x00002];
    pseudo_bit_t	mgid_index[0x00006];   /* Index to port GID table
                                                 mgid_index = (port_number-1) * 2^log_max_gid + gid_index
                                                 Where:
                                                 1. log_max_gid is taken from QUERY_DEV_LIM command
                                                 2. gid_index is the index to the GID table */
    pseudo_bit_t	reserved4[0x0000a];
/* -------------- */
    pseudo_bit_t	flow_label[0x00014];   /* IPv6 flow label */
    pseudo_bit_t	tclass[0x00008];       /* IPv6 TClass */
    pseudo_bit_t	sl[0x00004];           /* InfiniBand Service Level (SL) */
/* -------------- */
    pseudo_bit_t	rgid_127_96[0x00020];  /* Remote GID[127:96] */
/* -------------- */
    pseudo_bit_t	rgid_95_64[0x00020];   /* Remote GID[95:64] */
/* -------------- */
    pseudo_bit_t	rgid_63_32[0x00020];   /* Remote GID[63:32] */
/* -------------- */
    pseudo_bit_t	rgid_31_0[0x00020];    /* Remote GID[31:0] */
/* -------------- */
}; 

/* GPIO_event_data */

struct tavorprm_gpio_event_data_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00060];
/* -------------- */
    pseudo_bit_t	gpio_event_hi[0x00020];/* If any bit is set to 1, then a rising/falling event has occurred on the corrsponding GPIO pin. */
/* -------------- */
    pseudo_bit_t	gpio_event_lo[0x00020];/* If any bit is set to 1, then a rising/falling event has occurred on the corrsponding GPIO pin. */
/* -------------- */
    pseudo_bit_t	reserved1[0x00020];
/* -------------- */
}; 

/* Event_data Field - QP/EE Events */

struct tavorprm_qp_ee_event_st {	/* Little Endian */
    pseudo_bit_t	qpn_een[0x00018];      /* QP/EE/SRQ number event is reported for */
    pseudo_bit_t	reserved0[0x00008];
/* -------------- */
    pseudo_bit_t	reserved1[0x00020];
/* -------------- */
    pseudo_bit_t	reserved2[0x0001c];
    pseudo_bit_t	e_q[0x00001];          /* If set - EEN if cleared - QP in the QPN/EEN field
                                                 Not valid on SRQ events */
    pseudo_bit_t	reserved3[0x00003];
/* -------------- */
    pseudo_bit_t	reserved4[0x00060];
/* -------------- */
}; 

/* InfiniHost Type0 Configuration Header */

struct tavorprm_mt23108_type0_st {	/* Little Endian */
    pseudo_bit_t	vendor_id[0x00010];    /* Hardwired to 0x15B3 */
    pseudo_bit_t	device_id[0x00010];    /* hardwired to 23108 */
/* -------------- */
    pseudo_bit_t	command[0x00010];      /* PCI Command Register */
    pseudo_bit_t	status[0x00010];       /* PCI Status Register */
/* -------------- */
    pseudo_bit_t	revision_id[0x00008];
    pseudo_bit_t	class_code_hca_class_code[0x00018];
/* -------------- */
    pseudo_bit_t	cache_line_size[0x00008];/* Cache Line Size */
    pseudo_bit_t	latency_timer[0x00008];
    pseudo_bit_t	header_type[0x00008];  /* hardwired to zero */
    pseudo_bit_t	bist[0x00008];
/* -------------- */
    pseudo_bit_t	bar0_ctrl[0x00004];    /* hard-wired to '0100 */
    pseudo_bit_t	reserved0[0x00010];
    pseudo_bit_t	bar0_l[0x0000c];       /* Lower bits of BAR0 (configuration space) */
/* -------------- */
    pseudo_bit_t	bar0_h[0x00020];       /* Upper 32 bits of BAR0 (configuration space) */
/* -------------- */
    pseudo_bit_t	bar1_ctrl[0x00004];    /* Hardwired to '1100 */
    pseudo_bit_t	reserved1[0x00010];
    pseudo_bit_t	bar1_l[0x0000c];       /* Lower bits of BAR1 */
/* -------------- */
    pseudo_bit_t	bar1_h[0x00020];       /* upper 32 bits of BAR1 (User Access Revion - UAR - space) */
/* -------------- */
    pseudo_bit_t	bar2_ctrl[0x00004];    /* Hardwired to '1100 */
    pseudo_bit_t	reserved2[0x00010];
    pseudo_bit_t	bar2_l[0x0000c];       /* Lower bits of BAR2 */
/* -------------- */
    pseudo_bit_t	bar2_h[0x00020];       /* Upper 32 bits of BAR2 - DDR (attached memory) BAR */
/* -------------- */
    pseudo_bit_t	cardbus_cis_pointer[0x00020];
/* -------------- */
    pseudo_bit_t	subsystem_vendor_id[0x00010];/* Specified by the device NVMEM configuration */
    pseudo_bit_t	subsystem_id[0x00010]; /* Specified by the device NVMEM configuration */
/* -------------- */
    pseudo_bit_t	expansion_rom_enable[0x00001];/* Expansion ROM Enable. Hardwired to 0 if expansion ROM is disabled in the device NVMEM configuration. */
    pseudo_bit_t	reserved3[0x0000a];
    pseudo_bit_t	expansion_rom_base_address[0x00015];/* Expansion ROM Base Address (upper 21 bit). Hardwired to 0 if expansion ROM is disabled in the device NVMEM configuration. */
/* -------------- */
    pseudo_bit_t	capabilities_pointer[0x00008];/* Specified by the device NVMEM configuration */
    pseudo_bit_t	reserved4[0x00018];
/* -------------- */
    pseudo_bit_t	reserved5[0x00020];
/* -------------- */
    pseudo_bit_t	interrupt_line[0x00008];
    pseudo_bit_t	interrupt_pin[0x00008];
    pseudo_bit_t	min_gnt[0x00008];
    pseudo_bit_t	max_latency[0x00008];
/* -------------- */
    pseudo_bit_t	reserved6[0x00100];
/* -------------- */
    pseudo_bit_t	msi_cap_id[0x00008];
    pseudo_bit_t	msi_next_cap_ptr[0x00008];
    pseudo_bit_t	msi_en[0x00001];
    pseudo_bit_t	multiple_msg_cap[0x00003];
    pseudo_bit_t	multiple_msg_en[0x00003];
    pseudo_bit_t	cap_64_bit_addr[0x00001];
    pseudo_bit_t	reserved7[0x00008];
/* -------------- */
    pseudo_bit_t	msg_addr_l[0x00020];
/* -------------- */
    pseudo_bit_t	msg_addr_h[0x00020];
/* -------------- */
    pseudo_bit_t	msg_data[0x00010];
    pseudo_bit_t	reserved8[0x00010];
/* -------------- */
    pseudo_bit_t	pcix_cap_id[0x00008];
    pseudo_bit_t	pcix_next_cap_ptr[0x00008];
    pseudo_bit_t	pcix_command_reg[0x00010];/* PCIX command register */
/* -------------- */
    pseudo_bit_t	pcix_status_reg[0x00020];/* PCIX Status Register */
/* -------------- */
    pseudo_bit_t	reserved9[0x00440];
/* -------------- */
}; 

/* NTU QP Map Table Entry */

struct tavorprm_ntu_qpm_st {	/* Little Endian */
    pseudo_bit_t	va_h[0x00020];         /* Bits 63:32 of the virtual address to be used in IB request, Number of bits to be actually used depends on the page size (eg. will use all 52 for 4K page, 51 for 8K page etc). */
/* -------------- */
    pseudo_bit_t	wm[0x00002];           /* Amount of data to fill in to the read response buffer prior to delivering read response to uplink
                                                 00 - forward
                                                 01 - MTU
                                                 10 - full message
                                                 11 - Reserved */
    pseudo_bit_t	mtu[0x00002];          /* MTUI of the channel to be used by this page, value is 256*2MU bytes */
    pseudo_bit_t	rd_len[0x00003];       /* Length of speculative prefetch for read, value is 16*2RD_Len bytes */
    pseudo_bit_t	fence[0x00002];
    pseudo_bit_t	reserved0[0x00002];
    pseudo_bit_t	err_fence[0x00001];    /* 0,00 - No action in NTU - normal flow
                                                 0,01 - Reserved (fence bits value of "01" is not defined)
                                                 0,10 - Enter PCU transaction to Error fifo, NO fence trap to consequent transaction
                                                 0,11 - Enter PCU transaction to Error fifo, fence trap to consequent transactions
                                                 1,xx - Enter PCU transaction to Error fifo, mark QRM indication in error fifo. */
    pseudo_bit_t	va_l[0x00014];         /* Bits 31:12 of the virtual address to be used in IB request, Number of bits to be actually used depends on the page size (eg. will use all 52 for 4K page, 51 for 8K page etc). */
/* -------------- */
    pseudo_bit_t	rkey[0x00020];         /* RKey to be places for RDMA IB requests message */
/* -------------- */
    pseudo_bit_t	my_qpn[0x00018];       /* Local QO this page is mapped  to */
    pseudo_bit_t	s[0x00001];            /* Force solicit event bit in the descriptor */
    pseudo_bit_t	e[0x00001];            /* Force E-bit in the descriptor */
    pseudo_bit_t	s_r[0x00001];          /* S/R# - generate Send as a result of write hit to this page */
    pseudo_bit_t	b[0x00001];            /* Breakpoint - ptransfer control to firmware for every cycle that hits this page */
    pseudo_bit_t	reserved1[0x00002];
    pseudo_bit_t	ce[0x00001];           /* Cache Enable - entry can be cached if this bit is set. */
    pseudo_bit_t	v[0x00001];            /* Valid bit - the entry is valid only if this bit is set */
/* -------------- */
}; 

/* Event Data Field - Performance Monitor */

struct tavorprm_performance_monitor_event_st {	/* Little Endian */
    struct tavorprm_performance_monitors_st	performance_monitor_snapshot;/* Performance monitor snapshot */
/* -------------- */
    pseudo_bit_t	monitor_number[0x00008];/* 0x01 - SQPC
                                                 0x02 - RQPC
                                                 0x03 - CQC
                                                 0x04 - Rkey
                                                 0x05 - TLB
                                                 0x06 - port0
                                                 0x07 - port1 */
    pseudo_bit_t	reserved0[0x00018];
/* -------------- */
    pseudo_bit_t	reserved1[0x00040];
/* -------------- */
}; 

/* Event_data Field - Page Faults */

struct tavorprm_page_fault_event_data_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00040];
/* -------------- */
    pseudo_bit_t	s_r[0x00001];          /* Send (1) or Receive (0) queue caused page fault */
    pseudo_bit_t	r_l[0x00001];          /* Remote (1) or local (0) access caused fault */
    pseudo_bit_t	w_d[0x00001];          /* WQE (1) or data (0) access caused fault */
    pseudo_bit_t	wqv[0x00001];          /* Indicates whether message caused fault consumes descriptor (valid for receive queue only). */
    pseudo_bit_t	fault_type[0x00004];   /* 0000-0111  - RESERVED
                                                 1000 - Translation page not present
                                                 1001 - RESERVED
                                                 1010 - Page write access violation
                                                 1011 - 1101  -  RESERVED
                                                 1110 - Unsupported non-present page fault
                                                 1111 - unsupported write access fault */
    pseudo_bit_t	reserved1[0x00018];
/* -------------- */
    pseudo_bit_t	va_h[0x00020];         /* Virtual address that caused access fault[63:32] */
/* -------------- */
    pseudo_bit_t	va_l[0x00020];         /* Virtual address that caused access fault[31:0] */
/* -------------- */
    pseudo_bit_t	mem_key[0x00020];      /* Memory Key used for address translation */
/* -------------- */
}; 

/* Event_data Field - Port State Change */

struct tavorprm_port_state_change_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00040];
/* -------------- */
    pseudo_bit_t	reserved1[0x0001c];
    pseudo_bit_t	p[0x00002];            /* Port number (1 or 2) */
    pseudo_bit_t	reserved2[0x00002];
/* -------------- */
    pseudo_bit_t	reserved3[0x00060];
/* -------------- */
}; 

/* Event_data Field - Completion Queue Error */

struct tavorprm_completion_queue_error_st {	/* Little Endian */
    pseudo_bit_t	cqn[0x00018];          /* CQ number event is reported for */
    pseudo_bit_t	reserved0[0x00008];
/* -------------- */
    pseudo_bit_t	reserved1[0x00020];
/* -------------- */
    pseudo_bit_t	syndrome[0x00008];     /* Error syndrome
                                                 0x01 - CQ overrun
                                                 0x02 - CQ access violation error */
    pseudo_bit_t	reserved2[0x00018];
/* -------------- */
    pseudo_bit_t	reserved3[0x00060];
/* -------------- */
}; 

/* Event_data Field - Completion Event */

struct tavorprm_completion_event_st {	/* Little Endian */
    pseudo_bit_t	cqn[0x00018];          /* CQ number event is reported for */
    pseudo_bit_t	reserved0[0x00008];
/* -------------- */
    pseudo_bit_t	reserved1[0x000a0];
/* -------------- */
}; 

/* Event Queue Entry */

struct tavorprm_event_queue_entry_st {	/* Little Endian */
    pseudo_bit_t	event_sub_type[0x00008];/* Event Sub Type. 
                                                 Defined for events which have sub types, zero elsewhere. */
    pseudo_bit_t	reserved0[0x00008];
    pseudo_bit_t	event_type[0x00008];   /* Event Type */
    pseudo_bit_t	reserved1[0x00008];
/* -------------- */
    pseudo_bit_t	event_data[6][0x00020];/* Delivers auxilary data to handle event. */
/* -------------- */
    pseudo_bit_t	reserved2[0x00007];
    pseudo_bit_t	owner[0x00001];        /* Owner of the entry 
                                                 0 SW 
                                                 1 HW */
    pseudo_bit_t	reserved3[0x00018];
/* -------------- */
}; 

/* QP/EE State Transitions Command Parameters */

struct tavorprm_qp_ee_state_transitions_st {	/* Little Endian */
    pseudo_bit_t	opt_param_mask[0x00020];/* This field defines which optional parameters are passed. Each bit specifies whether optional parameter is passed (set) or not (cleared). The optparammask is defined for each QP/EE command. */
/* -------------- */
    pseudo_bit_t	reserved0[0x00020];
/* -------------- */
    struct tavorprm_queue_pair_ee_context_entry_st	qpc_eec_data;/* QPC/EEC data */
/* -------------- */
    pseudo_bit_t	reserved1[0x007c0];
/* -------------- */
}; 

/* Completion Queue Entry Format */

struct tavorprm_completion_queue_entry_st {	/* Little Endian */
    pseudo_bit_t	my_qpn[0x00018];       /* Indicates the QP for which completion is being reported */
    pseudo_bit_t	reserved0[0x00004];
    pseudo_bit_t	ver[0x00004];          /* CQE version. 
                                                 0 for InfiniHost */
/* -------------- */
    pseudo_bit_t	my_ee[0x00018];        /* EE context (for RD only).
                                                 Invalid for Bind and Nop operation on RD. */
    pseudo_bit_t	reserved1[0x00008];
/* -------------- */
    pseudo_bit_t	rqpn[0x00018];         /* Remote (source) QP number. Valid in Responder CQE only for Datagram QP. */
    pseudo_bit_t	reserved2[0x00008];
/* -------------- */
    pseudo_bit_t	rlid[0x00010];         /* Remote (source) LID of the message. Valid in Responder of UD QP CQE only. */
    pseudo_bit_t	ml_path[0x00007];      /* My (destination) LID path bits - these are the lowemost LMC bits of the DLID in an incoming UD packet, higher bits of this field, that are not part of the LMC bits are zeroed by HW.
                                                 Valid in responder of UD QP CQE only.
                                                 Invalid if incoming message DLID is the permissive LID or incoming message is multicast. */
    pseudo_bit_t	g[0x00001];            /* GRH present indicator. Valid in Responder of UD QP CQE only. */
    pseudo_bit_t	reserved3[0x00001];
    pseudo_bit_t	reserved4[0x00003];
    pseudo_bit_t	sl[0x00004];           /* Service Level of the message. Valid in Responder of UD QP CQE only. */
/* -------------- */
    pseudo_bit_t	immediate_ethertype_pkey_indx_eecredits[0x00020];/* Valid for receive queue completion only. 
                                                 If Opcode field indicates that this was send/write with immediate, this field contains immediate field of the packet. 
                                                 If completion corresponds to RAW receive queue, bits 15:0 contain Ethertype field of the packet. 
                                                 If completion corresponds to GSI receive queue, bits 31:16 contain index in PKey table that matches PKey of the message arrived. 
                                                 For CQE of send queue of the reliable connection service, bits [4:0] of this field contain the encoded EEcredits received in last ACK of the message.
                                                  */
/* -------------- */
    pseudo_bit_t	byte_cnt[0x00020];     /* Byte count of data actually transferred (valid for receive queue completions only) */
/* -------------- */
    pseudo_bit_t	wqe_size[0x00006];     /* Size (in 16-byte chunks) of WQE completion is reported for */
    pseudo_bit_t	wqe_adr[0x0001a];      /* Bits 31:6 of WQE virtual address completion is reported for. The 6 least significant bits are zero. */
/* -------------- */
    pseudo_bit_t	reserved5[0x00007];
    pseudo_bit_t	owner[0x00001];        /* Owner field. Zero value of this field means SW ownership of CQE. */
    pseudo_bit_t	reserved6[0x0000d];
    pseudo_bit_t	reserved7[0x00001];
    pseudo_bit_t	reserved8[0x00001];
    pseudo_bit_t	s[0x00001];            /* If set, completion is reported for Send queue, if cleared - receive queue. */
    pseudo_bit_t	opcode[0x00008];       /* The opcode of WQE completion is reported for.
                                                 For CQEs corresponding to send completion, NOPCODE field of the WQE is copied to this field.
                                                 For CQEs corresponding to receive completions, opcode field of last packet in the message copied to this field.
                                                 For CQEs corresponding to the receive queue of QPs mapped to QP1, the opcode will be SEND with Immediate (messages are guaranteed to be SEND only)
                                                 
                                                 The following values are reported in case of completion with error:
                                                 0xFE - For completion with error on Receive Queues
                                                 0xFF - For completion with error on Send Queues */
/* -------------- */
}; 

/*  */

struct tavorprm_ecc_detect_event_data_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00080];
/* -------------- */
    pseudo_bit_t	cause_lsb[0x00001];
    pseudo_bit_t	reserved1[0x00002];
    pseudo_bit_t	cause_msb[0x00001];
    pseudo_bit_t	reserved2[0x00002];
    pseudo_bit_t	err_rmw[0x00001];
    pseudo_bit_t	err_src_id[0x00003];
    pseudo_bit_t	err_da[0x00002];
    pseudo_bit_t	err_ba[0x00002];
    pseudo_bit_t	reserved3[0x00011];
    pseudo_bit_t	overflow[0x00001];
/* -------------- */
    pseudo_bit_t	err_ra[0x00010];
    pseudo_bit_t	err_ca[0x00010];
/* -------------- */
}; 

/* MAD_IFC Input Mailbox */

struct tavorprm_mad_ifc_st {	/* Little Endian */
    pseudo_bit_t	request_mad_packet[64][0x00020];/* Request MAD Packet (256bytes) */
/* -------------- */
    pseudo_bit_t	my_qpn[0x00018];       /* Destination QP number from the received MAD. 
                                                 This field is reserved if Mad_extended_info indication in the input modifier is clear. */
    pseudo_bit_t	reserved0[0x00008];
/* -------------- */
    pseudo_bit_t	rqpn[0x00018];         /* Remote (source) QP number  from the received MAD.
                                                 This field is reserved if Mad_extended_info indication in the input modifier is clear. */
    pseudo_bit_t	reserved1[0x00008];
/* -------------- */
    pseudo_bit_t	rlid[0x00010];         /* Remote (source) LID  from the received MAD.
                                                 This field is reserved if Mad_extended_info indication in the input modifier is clear. */
    pseudo_bit_t	ml_path[0x00007];      /* My (destination) LID path bits  from the received MAD.
                                                 This field is reserved if Mad_extended_info indication in the input modifier is clear. */
    pseudo_bit_t	g[0x00001];            /* If set, the GRH field in valid. 
                                                 This field is reserved if Mad_extended_info indication in the input modifier is clear. */
    pseudo_bit_t	reserved2[0x00004];
    pseudo_bit_t	sl[0x00004];           /* Service Level of the received MAD.
                                                 This field is reserved if Mad_extended_info indication in the input modifier is clear. */
/* -------------- */
    pseudo_bit_t	pkey_indx[0x00010];    /* Index in PKey table that matches PKey of the received MAD. 
                                                 This field is reserved if Mad_extended_info indication in the input modifier is clear. */
    pseudo_bit_t	reserved3[0x00010];
/* -------------- */
    pseudo_bit_t	reserved4[0x00180];
/* -------------- */
    pseudo_bit_t	grh[10][0x00020];      /* The GRH field of the MAD packet that was scattered to the first 40 bytes pointed to by the scatter list. 
                                                 Valid if Mad_extended_info bit (in the input modifier) and g bit are set. 
                                                 Otherwise this field is reserved. */
/* -------------- */
    pseudo_bit_t	reserved5[0x004c0];
/* -------------- */
}; 

/* Event_data Field - ECC Detection Event */

struct tavorprm_scrubbing_event_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x00080];
/* -------------- */
    pseudo_bit_t	cause_lsb[0x00001];    /* data integrity error cause:
                                                 single ECC error in the 64bit lsb data, on the rise edge of the clock */
    pseudo_bit_t	reserved1[0x00002];
    pseudo_bit_t	cause_msb[0x00001];    /* data integrity error cause:
                                                 single ECC error in the 64bit msb data, on the fall edge of the clock */
    pseudo_bit_t	reserved2[0x00002];
    pseudo_bit_t	err_rmw[0x00001];      /* transaction type:
                                                 0 - read
                                                 1 - read/modify/write */
    pseudo_bit_t	err_src_id[0x00003];   /* source of the transaction: 0x4 - PCI, other - internal or IB */
    pseudo_bit_t	err_da[0x00002];       /* Error DIMM address */
    pseudo_bit_t	err_ba[0x00002];       /* Error bank address */
    pseudo_bit_t	reserved3[0x00011];
    pseudo_bit_t	overflow[0x00001];     /* Fatal: ECC error FIFO overflow - ECC errors were detected, which may or may not have been corrected by InfiniHost */
/* -------------- */
    pseudo_bit_t	err_ra[0x00010];       /* Error row address */
    pseudo_bit_t	err_ca[0x00010];       /* Error column address */
/* -------------- */
}; 

/* PBL */

struct tavorprm_pbl_st {	/* Little Endian */
    pseudo_bit_t	mtt_0_h[0x00020];      /* First MTT[63:32] */
/* -------------- */
    pseudo_bit_t	mtt_0_l[0x00020];      /* First MTT[31:0] */
/* -------------- */
    pseudo_bit_t	mtt_1_h[0x00020];      /* Second MTT[63:32] */
/* -------------- */
    pseudo_bit_t	mtt_1_l[0x00020];      /* Second MTT[31:0] */
/* -------------- */
    pseudo_bit_t	mtt_2_h[0x00020];      /* Third MTT[63:32] */
/* -------------- */
    pseudo_bit_t	mtt_2_l[0x00020];      /* Third MTT[31:0] */
/* -------------- */
    pseudo_bit_t	mtt_3_h[0x00020];      /* Fourth MTT[63:32] */
/* -------------- */
    pseudo_bit_t	mtt_3_l[0x00020];      /* Fourth MTT[31:0] */
/* -------------- */
}; 

/* Miscellaneous Counters */

struct tavorprm_misc_counters_st {	/* Little Endian */
    pseudo_bit_t	ddr_scan_cnt[0x00020]; /* Number of times whole of DDR was scanned */
/* -------------- */
    pseudo_bit_t	reserved0[0x007e0];
/* -------------- */
}; 

/* MAD_IFC Opcode Modifier */

struct tavorprm_mad_ifc_opcode_modifier_st {	/* Little Endian */
    pseudo_bit_t	mkey[0x00001];         /* Enable MKey validation. */
    pseudo_bit_t	bkey[0x00001];         /* Enable BKey validation. */
    pseudo_bit_t	reserved0[0x0001d];
    pseudo_bit_t	mad_extended_info[0x00001];/* Mad_Extended_Info valid bit.
                                                 Requeried for for trap generation when BKey check is enabled. */
/* -------------- */
}; 

/* MAD_IFC Input Modifier */

struct tavorprm_mad_ifc_input_modifier_st {	/* Little Endian */
    pseudo_bit_t	port_number[0x00008];  /* Port number (1 or 2). */
    pseudo_bit_t	reserved0[0x00008];
    pseudo_bit_t	rlid[0x00001];         /* Remote (source) LID  from the received MAD.
                                                 This field is required for trap generation upon MKey/BKey validation. */
    pseudo_bit_t	reserved1[0x0000f];
/* -------------- */
}; 

/* Fast_Registration_Segment */

struct tavorprm_fast_registration_segment_st {	/* Little Endian */
    pseudo_bit_t	reserved0[0x0001b];
    pseudo_bit_t	lr[0x00001];           /* If set - Local Read access will be enabled */
    pseudo_bit_t	lw[0x00001];           /* If set - Local Write access will be enabled */
    pseudo_bit_t	rr[0x00001];           /* If set - Remote Read access will be enabled */
    pseudo_bit_t	rw[0x00001];           /* If set - Remote Write access will be enabled */
    pseudo_bit_t	a[0x00001];            /* If set - Remote Atomic access will be enabled */
/* -------------- */
    pseudo_bit_t	pbl_ptr_63_32[0x00020];/* Physical address pointer [63:32] to the physical block list */
/* -------------- */
    pseudo_bit_t	mem_key[0x00020];      /* Memory Key on which the fast registration is executed on. */
/* -------------- */
    pseudo_bit_t	page_size[0x00005];    /* Page size used for the region. Actual size is [4K]*2^Page_size bytes.
                                                 page_size should be less than 20. */
    pseudo_bit_t	reserved1[0x00002];
    pseudo_bit_t	zb[0x00001];           /* Zero Based Region */
    pseudo_bit_t	pbl_ptr_31_8[0x00018]; /* Physical address pointer [31:8] to the physical block list */
/* -------------- */
    pseudo_bit_t	start_address_h[0x00020];/* Start Address[63:32] - Virtual Address where this region starts */
/* -------------- */
    pseudo_bit_t	start_address_l[0x00020];/* Start Address[31:0] - Virtual Address where this region starts */
/* -------------- */
    pseudo_bit_t	reg_len_h[0x00020];    /* Region Length[63:32] */
/* -------------- */
    pseudo_bit_t	reg_len_l[0x00020];    /* Region Length[31:0] */
/* -------------- */
}; 

/* 0 */

struct tavorprm_tavor_prm_st {	/* Little Endian */
    struct tavorprm_completion_queue_entry_st	completion_queue_entry;/* Completion Queue Entry Format */
/* -------------- */
    pseudo_bit_t	reserved0[0x7ff00];
/* -------------- */
    struct tavorprm_qp_ee_state_transitions_st	qp_ee_state_transitions;/* QP/EE State Transitions Command Parameters */
/* -------------- */
    pseudo_bit_t	reserved1[0x7f000];
/* -------------- */
    struct tavorprm_event_queue_entry_st	event_queue_entry;/* Event Queue Entry */
/* -------------- */
    pseudo_bit_t	reserved2[0x7ff00];
/* -------------- */
    struct tavorprm_completion_event_st	completion_event;/* Event_data Field - Completion Event */
/* -------------- */
    pseudo_bit_t	reserved3[0x7ff40];
/* -------------- */
    struct tavorprm_completion_queue_error_st	completion_queue_error;/* Event_data Field - Completion Queue Error */
/* -------------- */
    pseudo_bit_t	reserved4[0x7ff40];
/* -------------- */
    struct tavorprm_port_state_change_st	port_state_change;/* Event_data Field - Port State Change */
/* -------------- */
    pseudo_bit_t	reserved5[0xfff40];
/* -------------- */
    struct tavorprm_page_fault_event_data_st	page_fault_event_data;/* Event_data Field - Page Faults */
/* -------------- */
    pseudo_bit_t	reserved6[0x7ff40];
/* -------------- */
    struct tavorprm_performance_monitor_event_st	performance_monitor_event;/* Event Data Field - Performance Monitor */
/* -------------- */
    pseudo_bit_t	reserved7[0x7ff20];
/* -------------- */
    struct tavorprm_ntu_qpm_st	ntu_qpm; /* NTU QP Map Table Entry */
/* -------------- */
    pseudo_bit_t	reserved8[0x7ff80];
/* -------------- */
    struct tavorprm_mt23108_type0_st	mt23108_type0;/* InfiniHost Type0 Configuration Header */
/* -------------- */
    pseudo_bit_t	reserved9[0x7f800];
/* -------------- */
    struct tavorprm_qp_ee_event_st	qp_ee_event;/* Event_data Field - QP/EE Events */
/* -------------- */
    pseudo_bit_t	reserved10[0x00040];
/* -------------- */
    struct tavorprm_gpio_event_data_st	gpio_event_data;
/* -------------- */
    pseudo_bit_t	reserved11[0x7fe40];
/* -------------- */
    struct tavorprm_ud_address_vector_st	ud_address_vector;/* UD Address Vector */
/* -------------- */
    pseudo_bit_t	reserved12[0x7ff00];
/* -------------- */
    struct tavorprm_queue_pair_ee_context_entry_st	queue_pair_ee_context_entry;/* QP and EE Context Entry */
/* -------------- */
    pseudo_bit_t	reserved13[0x7f800];
/* -------------- */
    struct tavorprm_address_path_st	address_path;/* Address Path */
/* -------------- */
    pseudo_bit_t	reserved14[0x7ff00];
/* -------------- */
    struct tavorprm_completion_queue_context_st	completion_queue_context;/* Completion Queue Context Table Entry */
/* -------------- */
    pseudo_bit_t	reserved15[0x7fe00];
/* -------------- */
    struct tavorprm_mpt_st	mpt;         /* Memory Protection Table (MPT) Entry */
/* -------------- */
    pseudo_bit_t	reserved16[0x7fe00];
/* -------------- */
    struct tavorprm_mtt_st	mtt;         /* Memory Translation Table (MTT) Entry */
/* -------------- */
    pseudo_bit_t	reserved17[0x7ffc0];
/* -------------- */
    struct tavorprm_eqc_st	eqc;         /* Event Queue Context Table Entry */
/* -------------- */
    pseudo_bit_t	reserved18[0x7fe00];
/* -------------- */
    struct tavorprm_performance_monitors_st	performance_monitors;/* Performance Monitors */
/* -------------- */
    pseudo_bit_t	reserved19[0x7ff80];
/* -------------- */
    struct tavorprm_hca_command_register_st	hca_command_register;/* HCA Command Register (HCR) */
/* -------------- */
    pseudo_bit_t	reserved20[0xfff20];
/* -------------- */
    struct tavorprm_init_hca_st	init_hca;/* INIT_HCA & QUERY_HCA Parameters Block */
/* -------------- */
    pseudo_bit_t	reserved21[0x7f000];
/* -------------- */
    struct tavorprm_qpcbaseaddr_st	qpcbaseaddr;/* QPC/EEC/CQC/EQC/RDB Parameters */
/* -------------- */
    pseudo_bit_t	reserved22[0x7fc00];
/* -------------- */
    struct tavorprm_udavtable_memory_parameters_st	udavtable_memory_parameters;/* Memory Access Parameters for UD Address Vector Table */
/* -------------- */
    pseudo_bit_t	reserved23[0x7ffc0];
/* -------------- */
    struct tavorprm_multicastparam_st	multicastparam;/* Multicast Support Parameters */
/* -------------- */
    pseudo_bit_t	reserved24[0x7ff00];
/* -------------- */
    struct tavorprm_tptparams_st	tptparams;/* Translation and Protection Tables Parameters */
/* -------------- */
    pseudo_bit_t	reserved25[0x7ff00];
/* -------------- */
    struct tavorprm_query_ddr_st	query_ddr;/* QUERY_DDR Parameters Block */
/* -------------- */
    struct tavorprm_access_ddr_st	access_ddr;
/* -------------- */
    pseudo_bit_t	reserved26[0x7f700];
/* -------------- */
    struct tavorprm_dimminfo_st	dimminfo;/* Logical DIMM Information */
/* -------------- */
    pseudo_bit_t	reserved27[0x7ff00];
/* -------------- */
    struct tavorprm_query_fw_st	query_fw;/* QUERY_FW Parameters Block */
/* -------------- */
    pseudo_bit_t	reserved28[0x7f800];
/* -------------- */
    struct tavorprm_query_adapter_st	query_adapter;/* QUERY_ADAPTER Parameters Block */
/* -------------- */
    pseudo_bit_t	reserved29[0x7f800];
/* -------------- */
    struct tavorprm_query_dev_lim_st	query_dev_lim;/* Query Device Limitations */
/* -------------- */
    pseudo_bit_t	reserved30[0x7f800];
/* -------------- */
    struct tavorprm_uar_params_st	uar_params;/* UAR Parameters */
/* -------------- */
    pseudo_bit_t	reserved31[0x7ff00];
/* -------------- */
    struct tavorprm_init_ib_st	init_ib; /* INIT_IB Parameters */
/* -------------- */
    pseudo_bit_t	reserved32[0x7f800];
/* -------------- */
    struct tavorprm_mgm_entry_st	mgm_entry;/* Multicast Group Member */
/* -------------- */
    pseudo_bit_t	reserved33[0x7fe00];
/* -------------- */
    struct tavorprm_set_ib_st	set_ib;   /* SET_IB Parameters */
/* -------------- */
    pseudo_bit_t	reserved34[0x7fe00];
/* -------------- */
    struct tavorprm_rd_send_doorbell_st	rd_send_doorbell;/* RD-send doorbell */
/* -------------- */
    pseudo_bit_t	reserved35[0x7ff80];
/* -------------- */
    struct tavorprm_send_doorbell_st	send_doorbell;/* Send doorbell */
/* -------------- */
    pseudo_bit_t	reserved36[0x7ffc0];
/* -------------- */
    struct tavorprm_receive_doorbell_st	receive_doorbell;/* Receive doorbell */
/* -------------- */
    pseudo_bit_t	reserved37[0x7ffc0];
/* -------------- */
    struct tavorprm_cq_cmd_doorbell_st	cq_cmd_doorbell;/* CQ Doorbell */
/* -------------- */
    pseudo_bit_t	reserved38[0x7ffc0];
/* -------------- */
    struct tavorprm_eq_cmd_doorbell_st	eq_cmd_doorbell;/* EQ Doorbell */
/* -------------- */
    pseudo_bit_t	reserved39[0x7ffc0];
/* -------------- */
    struct tavorprm_uar_st	uar;         /* User Access Region */
/* -------------- */
    pseudo_bit_t	reserved40[0x7c000];
/* -------------- */
    struct tavorprm_mgmqp_st	mgmqp;     /* Multicast Group Member QP */
/* -------------- */
    pseudo_bit_t	reserved41[0x7ffe0];
/* -------------- */
    struct tavorprm_query_debug_msg_st	query_debug_msg;/* Query Debug Message */
/* -------------- */
    pseudo_bit_t	reserved42[0x7f800];
/* -------------- */
    struct tavorprm_sys_en_out_param_st	sys_en_out_param;/* SYS_EN Output Parameter */
/* -------------- */
    pseudo_bit_t	reserved43[0x7ffc0];
/* -------------- */
    struct tavorprm_resize_cq_st	resize_cq;/* Resize CQ Input Mailbox */
/* -------------- */
    pseudo_bit_t	reserved44[0x7fe00];
/* -------------- */
    struct tavorprm_completion_with_error_st	completion_with_error;/* Completion with Error CQE */
/* -------------- */
    pseudo_bit_t	reserved45[0x7ff00];
/* -------------- */
    struct tavorprm_hcr_completion_event_st	hcr_completion_event;/* Event_data Field - HCR Completion Event */
/* -------------- */
    pseudo_bit_t	reserved46[0x7ff40];
/* -------------- */
    struct tavorprm_transport_and_ci_error_counters_st	transport_and_ci_error_counters;/* Transport and CI Error Counters */
/* -------------- */
    pseudo_bit_t	reserved47[0x7f000];
/* -------------- */
    struct tavorprm_performance_counters_st	performance_counters;/* Performance Counters */
/* -------------- */
    pseudo_bit_t	reserved48[0x7f800];
/* -------------- */
    struct tavorprm_query_bar_st	query_bar;/* Query BAR */
/* -------------- */
    pseudo_bit_t	reserved49[0x7ffc0];
/* -------------- */
    struct tavorprm_cfg_schq_st	cfg_schq;/* Schedule queues configuration */
/* -------------- */
    pseudo_bit_t	reserved50[0x7f800];
/* -------------- */
    struct tavorprm_mt23108_configuration_registers_st	mt23108_configuration_registers;/* InfiniHost Configuration Registers - Used in Mem-Free mode only */
/* -------------- */
    pseudo_bit_t	reserved51[0x80000];
/* -------------- */
    pseudo_bit_t	reserved52[0x00100];
/* -------------- */
    pseudo_bit_t	reserved53[0x7ff00];
/* -------------- */
    pseudo_bit_t	reserved54[0x00100];
/* -------------- */
    pseudo_bit_t	reserved55[0x7ff00];
/* -------------- */
    struct tavorprm_srq_context_st	srq_context;/* SRQ Context */
/* -------------- */
    pseudo_bit_t	reserved56[0x7ff00];
/* -------------- */
    struct tavorprm_mod_stat_cfg_st	mod_stat_cfg;/* MOD_STAT_CFG */
/* -------------- */
    pseudo_bit_t	reserved57[0x00080];
/* -------------- */
    pseudo_bit_t	reserved58[0x00040];
/* -------------- */
    pseudo_bit_t	reserved59[0x1bff740];
/* -------------- */
}; 

#include "MT23108_PRM_append.h"

#endif /* H_prefix_tavorprm_bits_fixnames_MT23108_PRM_csp_H */
