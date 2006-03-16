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

#ifndef __ib_mad_h__
#define __ib_mad_h__

#include "ib_driver.h"

/* Management base version */
#define IB_MGMT_BASE_VERSION			1

/* Management classes */
#define IB_MGMT_CLASS_SUBN_LID_ROUTED		0x01
#define IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE	0x81
#define IB_MGMT_CLASS_SUBN_ADM			0x03
#define IB_MGMT_CLASS_PERF_MGMT			0x04
#define IB_MGMT_CLASS_BM			0x05
#define IB_MGMT_CLASS_DEVICE_MGMT		0x06
#define IB_MGMT_CLASS_CM			0x07
#define IB_MGMT_CLASS_SNMP			0x08
#define IB_MGMT_CLASS_VENDOR_RANGE2_START	0x30
#define IB_MGMT_CLASS_VENDOR_RANGE2_END		0x4F

/* Management methods */
#define IB_MGMT_METHOD_GET			0x01
#define IB_MGMT_METHOD_SET			0x02
#define IB_MGMT_METHOD_GET_RESP			0x81
#define IB_MGMT_METHOD_SEND			0x03
#define IB_MGMT_METHOD_TRAP			0x05
#define IB_MGMT_METHOD_REPORT			0x06
#define IB_MGMT_METHOD_REPORT_RESP		0x86
#define IB_MGMT_METHOD_TRAP_REPRESS		0x07
#define IB_MGMT_METHOD_DELETE			0x15

#define IB_MGMT_METHOD_RESP			0x80

/* Subnet management attributes */
#define IB_SMP_ATTR_NOTICE					0x0002
#define IB_SMP_ATTR_NODE_DESC				0x0010
#define IB_SMP_ATTR_NODE_INFO				0x0011
#define IB_SMP_ATTR_SWITCH_INFO				0x0012
#define IB_SMP_ATTR_GUID_INFO				0x0014
#define IB_SMP_ATTR_PORT_INFO				0x0015
#define IB_SMP_ATTR_PKEY_TABLE				0x0016
#define IB_SMP_ATTR_SL_TO_VL_TABLE			0x0017
#define IB_SMP_ATTR_VL_ARB_TABLE			0x0018
#define IB_SMP_ATTR_LINEAR_FORWARD_TABLE	0x0019
#define IB_SMP_ATTR_RANDOM_FORWARD_TABLE	0x001A
#define IB_SMP_ATTR_MCAST_FORWARD_TABLE		0x001B
#define IB_SMP_ATTR_SM_INFO					0x0020
#define IB_SMP_ATTR_VENDOR_DIAG				0x0030
#define IB_SMP_ATTR_LED_INFO				0x0031
#define IB_SMP_ATTR_VENDOR_MASK				0xFF00

struct ib_mad_hdr_st {
	__u8 method;
	__u8 class_version;
	__u8 mgmt_class;
	__u8 base_version;
	__u16 class_specific;
	__u16 status;
	__u32 tid[2];
	__u16 resv;
	__u16 attr_id;
	__u32 attr_mod;
} __attribute__ ((packed));

struct rmpp_hdr_st {
	__u32 raw[3];
} __attribute__ ((packed));

struct sa_header_st {
	__u32 sm_key[2];
	__u16 attrib_offset;
	__u16 r0;
	__u32 comp_mask[2];
} __attribute__ ((packed));

struct ib_mad_st {
	struct ib_mad_hdr_st mad_hdr;
	__u8 data[232];
} __attribute__ ((packed));

union mad_u {
	__u8 raw[256];
	struct ib_mad_st mad;
} __attribute__ ((packed));

static int get_path_record(union ib_gid_u *dgid, __u16 * dlid_p, __u8 * sl_p,
			   __u8 * rate_p);

#endif				/* __ib_mad_h__ */
