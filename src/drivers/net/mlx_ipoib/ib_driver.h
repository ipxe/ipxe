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

#ifndef __ib_driver_h__
#define __ib_driver_h__

#define MELLANOX_VENDOR_ID	0x15b3

#define GLOBAL_PD   0x123456
#define GLOBAL_QKEY   0x80010000

#define MAD_BUF_SZ 256
#define IPOIB_RCV_BUF_SZ 2048
#define IPOIB_SND_BUF_SZ 2048
#define GRH_SIZE 40

#define ARP_BUF_SZ 56

#define FL_EOL 255		/* end of free list */

#define SEND_CQE_POLL_TOUT 38	/* 2 sec */
#define SA_RESP_POLL_TOUT 91	/* 5 seconds */

#define NUM_AVS 10

#define PXE_IB_PORT 1

#define SA_QPN 1
#define BCAST_QPN 0xffffff

#define QPN_BASE 0x550000

enum {
	MADS_QPN_SN,
	IPOIB_QPN_SN,
	MAX_APP_QPS
};

enum {
	MADS_SND_CQN_SN,
	MADS_RCV_CQN_SN,
	IPOIB_SND_CQN_SN,
	IPOIB_RCV_CQN_SN,
	MAX_APP_CQS
};

enum {
	MTU_256 = 1,
	MTU_512 = 2,
	MTU_1024 = 3,
	MTU_2048 = 4,
};

#define HCR_BASE 0x80680
#define HCR_OFFSET_GO 0x80698
#define HCR_OFFSET_STATUS 0x80698
#define HCR_OFFSET_OUTPRM_H 0x8068C
#define HCR_OFFSET_OUTPRM_L 0x80690

#define MKEY_PREFIX 0x77000000
#define MKEY_IDX_MASK 0xffffff

/* event types */
/*=============*/
/* Completion Events */
#define XDEV_EV_TYPE_CQ_COMP					0

  /* IB - affiliated errors CQ  */
#define XDEV_EV_TYPE_CQ_ERR						0x04
#define XDEV_EV_TYPE_LOCAL_WQ_CATAS_ERR			0x05

  /* Unaffiliated errors */
#define XDEV_EV_TYPE_PORT_ERR					0x09
#define XDEV_EV_TYPE_LOCAL_WQ_INVALID_REQ_ERR	0x10
#define XDEV_EV_TYPE_LOCAL_WQ_ACCESS_VIOL_ERR	0x11

/* NOPCODE field enumeration for doorbells and send-WQEs */
#define XDEV_NOPCODE_SEND	10	/* Send */

struct ib_gid_u32_st {
	__u32 dw[4];
};

union ib_gid_u {
	__u8 raw[16];
	struct ib_gid_u32_st as_u32;
} __attribute__ ((packed));

struct ib_cqe_st {
	__u8 is_error;
	__u8 is_send;
	void *wqe;
	__u32 count;
};

typedef void *udqp_t;
typedef void *cq_t;
typedef void *ud_av_t;
typedef void *ud_send_wqe_t;
typedef void *eq_t;

struct ib_data_st {
//      __u32 mkey;
//      __u32 pd;
//      __u32 qkey;
	udqp_t mads_qp;
	udqp_t ipoib_qp;
	cq_t mads_snd_cq;
	cq_t mads_rcv_cq;
	cq_t ipoib_snd_cq;
	cq_t ipoib_rcv_cq;
	eq_t eq;
	__u16 sm_lid;
	__u16 pkey;
	union ib_gid_u port_gid;
	union ib_gid_u bcast_gid;
	ud_av_t bcast_av;	/* av allocated and used solely for broadcast */
	__u8 port;
};

static int setup_hca(__u8 port, void **eq_p);
static int post_send_req(udqp_t qp, ud_send_wqe_t wqe, __u8 num_gather);
static void prep_send_wqe_buf(udqp_t qp,
			      ud_av_t av,
			      ud_send_wqe_t wqe,
			      const void *buf,
			      unsigned int offset, __u16 len, __u8 e);

static int create_mads_qp(void **qp_pp, void **snd_cq_pp, void **rcv_cq_pp);

static int create_ipoib_qp(udqp_t * qp_p,
			   void **snd_cq_pp, void **rcv_cq_pp, __u32 qkey);

static int gw_read_cr(__u32 addr, __u32 * result);
static int gw_write_cr(__u32 addr, __u32 data);
static ud_av_t alloc_ud_av(void);
static void free_ud_av(ud_av_t av);
static int ib_poll_cq(cq_t cq, struct ib_cqe_st *ib_cqe_p, __u8 * num_cqes);
static int add_qp_to_mcast_group(union ib_gid_u mcast_gid, __u8 add);
static int clear_interrupt(void);
static int poll_cqe_tout(cq_t cqh, __u16 tout, void **wqe, int *good_p);

static void *get_inprm_buf(void);
static void *get_outprm_buf(void);
static __u32 ib_get_qpn(udqp_t qph);

static void dev_post_dbell(void *dbell, __u32 offset);

static struct ib_data_st ib_data;

#endif				/* __ib_driver_h__ */
