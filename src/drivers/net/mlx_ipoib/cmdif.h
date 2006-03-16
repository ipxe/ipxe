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

#ifndef __cmdif_h_
#define __cmdif_h_

#include "ib_mad.h"

static int cmd_init_hca(__u32 * inprm, __u32 in_prm_size);
static int cmd_close_hca(int panic);
static int cmd_sw2hw_eq(__u32 inprm_sz);
static int cmd_hw2sw_eq(__u8 eqn);
static int cmd_map_eq(__u8 eqn, __u32 mask, int map);
static int cmd_sw2hw_mpt(__u32 * lkey, __u32 in_key, __u32 * inprm,
			 __u32 inprm_sz);
static int cmd_hw2sw_mpt(__u32 key);
static int cmd_init_ib(__u32 port, __u32 * inprm, __u32 inprm_sz);
static int cmd_close_ib(__u32 port);
static int cmd_sw2hw_cq(__u32 cqn, __u32 * inprm, __u32 inprm_sz);
static int cmd_hw2sw_cq(__u32 cqn);
static int cmd_rst2init_qpee(__u32 qpn, __u32 * inprm, __u32 inprm_sz);
static int cmd_init2rtr_qpee(__u32 qpn, __u32 * inprm, __u32 inprm_sz);
static int cmd_rtr2rts_qpee(__u32 qpn, __u32 * inprm, __u32 inprm_sz);
static int cmd_2rst_qpee(__u32 qpn);
static int cmd_2err_qpee(__u32 qpn);
static int cmd_post_doorbell(void *inprm, __u32 offset);
static int cmd_mad_ifc(void *inprm, struct ib_mad_st *mad, __u8 port);
static int cmd_write_mgm( /*struct mg_member_layout_st */ void *mg,
			 __u16 index);
static int cmd_mgid_hash(__u8 * gid, __u16 * mgid_hash_p);

#endif				/* __cmdif_h_ */
