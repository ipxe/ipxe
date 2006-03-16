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

#ifndef __mad_attrib_h_
#define __mad_attrib_h_

#include "ib_mad.h"

#define IB_SA_ATTR_MC_MEMBER_REC 0x38
#define IB_SA_ATTR_PATH_REC 0x35

#define IB_SA_MCMEMBER_REC_MGID						(1<<0)
#define IB_SA_MCMEMBER_REC_PORT_GID					(1<<1)
#define IB_SA_MCMEMBER_REC_QKEY						(1<<2)
#define IB_SA_MCMEMBER_REC_MLID						(1<<3)
#define IB_SA_MCMEMBER_REC_MTU_SELECTOR				(1<<4)
#define IB_SA_MCMEMBER_REC_MTU						(1<<5)
#define IB_SA_MCMEMBER_REC_TRAFFIC_CLASS			(1<<6)
#define IB_SA_MCMEMBER_REC_PKEY						(1<<7)
#define IB_SA_MCMEMBER_REC_RATE_SELECTOR			(1<<8)
#define IB_SA_MCMEMBER_REC_RATE						(1<<9)
#define IB_SA_MCMEMBER_REC_PACKET_LIFE_TIME_SELECTOR	(1<<10)
#define IB_SA_MCMEMBER_REC_PACKET_LIFE_TIME			(1<<11)
#define IB_SA_MCMEMBER_REC_SL						(1<<12)
#define IB_SA_MCMEMBER_REC_FLOW_LABEL				(1<<13)
#define IB_SA_MCMEMBER_REC_HOP_LIMIT				(1<<14)
#define IB_SA_MCMEMBER_REC_SCOPE					(1<<15)
#define IB_SA_MCMEMBER_REC_JOIN_STATE				(1<<16)
#define IB_SA_MCMEMBER_REC_PROXY_JOIN				(1<<17)

#define IB_SA_PATH_REC_DGID		(1<<2)
#define IB_SA_PATH_REC_SGID		(1<<3)

struct port_info_st {
	__u32 mkey[2];
	__u32 gid_prefix[2];
	__u16 mastersm_lid;
	__u16 lid;
	__u32 cap_mask;
	__u32 combined2;
			/*__u32 mkey_lease_period:16;
			__u32 diag_code:16;*/
	__u32 combined3;
			/*__u32 link_width_active:8;
			__u32 link_width_supported:8;
			__u32 link_width_enabled:8;
			__u32 local_port_num:8;*/
	__u32 combined4;
			/*__u32 link_speed_enabled:4;
			__u32 link_speed_active:4;
			__u32 lmc:3;
			__u32 r1:3;
			__u32 mkey_prot_bits:2;
			__u32 link_down_def_state:4;
			__u32 port_phys_state:4;
			__u32 port_state:4;
			__u32 link_speed_supported:4;*/
	__u32 combined5;
			/*__u32 vl_arb_hi_cap:8;
			__u32 vl_hi_limit:8;
			__u32 init_type:4;
			__u32 vl_cap:4;
			__u32 master_smsl:4;
			__u32 neigh_mtu:4;*/
	__u32 combined6;
			/*__u32 filt_raw_oub:1;
			__u32 filt_raw_inb:1;
			__u32 part_enf_oub:1;
			__u32 part_enf_inb:1;
			__u32 op_vls:4;
			__u32 hoq_life:5;
			__u32 vl_stall_count:3;
			__u32 mtu_cap:4;
			__u32 init_type_reply:4;
			__u32 vl_arb_lo_cap:8;*/
	__u32 combined7;
			/*__u32 pkey_viol:16;
			__u32 mkey_viol:16;*/
	__u32 combined8;
			/*__u32 subn_tout:5;
			__u32 r2:2;
			__u32 client_rereg:1;
			__u32 guid_cap:8;
			__u32 qkey_viol:16;*/
	__u32 combined9;
			/*__u32 max_cred_hint:16;
			__u32 overrun_err:4;
			__u32 local_phy_err:4;
			__u32 resp_t_val:5;
			__u32 r3:3;*/
	__u32 combined10;
			/*__u32 r4:8;
			__u32 link_rtrip_lat:24;*/
} __attribute__ ((packed));

struct port_info_mad_st {
	struct ib_mad_hdr_st mad_hdr;
	__u32 mkey[2];
	__u32 r1[8];
	struct port_info_st port_info;
} __attribute__ ((packed));

union port_info_mad_u {
	__u8 raw[256];
	struct port_info_mad_st mad;
} __attribute__ ((packed));

struct guid_info_st {
	union ib_gid_u gid_tbl[8];
} __attribute__ ((packed));

struct guid_info_mad_st {
	struct ib_mad_hdr_st mad_hdr;
	__u32 mkey[2];
	__u32 r1[8];
	struct guid_info_st guid_info;
} __attribute__ ((packed));

union guid_info_mad_u {
	__u8 raw[256];
	struct guid_info_mad_st mad;
} __attribute__ ((packed));

struct mc_member_st {
	__u8 mgid[16];
	__u8 port_gid[16];
	__u32 q_key;
	__u32 combined1;
			/*__u32	tclass:8;
			__u32 mtu:6;
			__u32 mtu_selector:2;
			__u32 mlid:16;*/
	__u32 combined2;
			/*__u32 packet_liftime:6;
			__u32 packet_liftime_selector:2;
			__u32 rate:6;
			__u32 rate_selector:2;
			__u32 pkey:16;*/
	__u32 combined3;
			/*__u32 hop_limit:8;
			__u32 flow_label:20;
			__u32 sl:4;*/
	__u32 combined4;
			/*__u32 r0:23;
			__u32 proxy_join:1;
			__u32 join_state:4;
			__u32 scope:4;*/
} __attribute__ ((packed));

struct mc_member_mad_st {
	struct ib_mad_hdr_st mad_hdr;
	struct rmpp_hdr_st rmpp_hdr;
	struct sa_header_st sa_hdr;
	struct mc_member_st mc_member;
} __attribute__ ((packed));

union mc_member_mad_u {
	struct mc_member_mad_st mc_member;
	__u8 raw[256];
} __attribute__ ((packed));

struct pkey_tbl_st {
	__u16 pkey_tbl[16][2];
} __attribute__ ((packed));

struct pkey_tbl_mad_st {
	struct ib_mad_hdr_st mad_hdr;
	__u32 mkey[2];
	__u32 r1[8];
	struct pkey_tbl_st pkey_tbl;
} __attribute__ ((packed));

union pkey_tbl_mad_u {
	struct pkey_tbl_mad_st mad;
	__u8 raw[256];
} __attribute__ ((packed));

struct path_record_st {
	__u32 r0[2];
	union ib_gid_u dgid;
	union ib_gid_u sgid;
	__u16 slid;
	__u16 dlid;
	__u32 combined1;
			/*__u32	hop_limit:8;
			__u32 flow_label:20;
			__u32 r1:3;
			__u32 raw_traffic:1;*/
	__u32 combined2;
			/*__u32 pkey:16;
			__u32 numb_path:7;
			__u32 reversible:1;
			__u32 tclass:8;*/
	__u32 combined3;
			/*__u32 rate:6;
			__u32 rate_selector:2;
			__u32 mtu:6;
			__u32 mtu_selector:2;
			__u32 sl:4;
			__u32 reserved:12;*/
	__u32 combined4;
			/*__u32 r2:16;
			__u32 preference:8;
			__u32 packet_lifetime:6;
			__u32 packet_lifetime_selector:2;*/
	__u32 r3;
} __attribute__ ((packed));

struct path_record_mad_st {
	struct ib_mad_hdr_st mad_hdr;
	struct rmpp_hdr_st rmpp_hdr;
	struct sa_header_st sa_hdr;
	struct path_record_st path_record;
} __attribute__ ((packed));

union path_record_mad_u {
	struct path_record_mad_st mad;
	__u8 raw[256];
} __attribute__ ((packed));

static int get_port_info(__u8 port, struct port_info_st *buf, __u16 * status);
static int get_guid_info(__u16 * status);
static int get_pkey_tbl(struct pkey_tbl_st *pkey_tbl, __u16 * status);
static int join_mc_group(__u32 * qkey_p, __u16 * mlid_p, __u8 join);

#endif				/* __mad_attrib_h_ */
