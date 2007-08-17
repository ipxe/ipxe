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

#include "mt25218.h"
#include "ib_driver.h"
#include "pci.h"

#define MOD_INC(counter, max_count) (counter) = ((counter)+1) & ((max_count) - 1)

#define breakpoint {volatile __u32 *p=(__u32 *)0x1234;printf("breakpoint\n");do {} while((*p) != 0x1234);}

#define WRITE_BYTE_VOL(addr, off, val) \
    do { \
        (*((volatile __u8 *)(((volatile __u8 *)(addr)) + off))) = (val); \
    } while(0)

#define WRITE_WORD_VOL(addr, off, val) \
    do { \
        (*((volatile __u16 *)(((volatile __u8 *)(addr)) + off))) = (val); \
    } while(0)

#define WRITE_DWORD_VOL(addr, off, val) \
    do { \
        (*((volatile __u32 *)(((volatile __u8 *)(addr)) + off))) = (val); \
    } while(0)

struct device_buffers_st {
	/* inprm and outprm do not have alignnemet constraint sice that
	   is acheived programatically */
	u8 inprm_buf[INPRM_BUF_SZ];
	u8 outprm_buf[OUTPRM_BUF_SZ];
	union recv_wqe_u mads_qp_rcv_queue[NUM_MADS_RCV_WQES]
	    __attribute__ ((aligned(RECV_WQE_U_ALIGN)));
	union recv_wqe_u ipoib_qp_rcv_queue[NUM_IPOIB_RCV_WQES]
	    __attribute__ ((aligned(RECV_WQE_U_ALIGN)));
	union ud_send_wqe_u mads_qp_snd_queue[NUM_MADS_SND_WQES]
	    __attribute__ ((aligned(UD_SEND_WQE_U_ALIGN)));
	union ud_send_wqe_u ipoib_qp_snd_queue[NUM_IPOIB_SND_WQES]
	    __attribute__ ((aligned(UD_SEND_WQE_U_ALIGN)));
	struct eqe_t eq_buf[1 << LOG2_EQ_SZ]
	    __attribute__ ((aligned(sizeof(struct eqe_t))));
	union cqe_st mads_snd_cq_buf[NUM_MADS_SND_CQES]
	    __attribute__ ((aligned(sizeof(union cqe_st))));
	union cqe_st ipoib_snd_cq_buf[NUM_IPOIB_SND_CQES]
	    __attribute__ ((aligned(sizeof(union cqe_st))));
	union cqe_st mads_rcv_cq_buf[NUM_MADS_RCV_CQES]
	    __attribute__ ((aligned(sizeof(union cqe_st))));
	union cqe_st ipoib_rcv_cq_buf[NUM_IPOIB_RCV_CQES]
	    __attribute__ ((aligned(sizeof(union cqe_st))));
	union ud_av_u av_array[NUM_AVS];
} __attribute__ ((packed));

#define STRUCT_ALIGN_SZ 4096
#define SRC_BUF_SZ (sizeof(struct device_buffers_st) + STRUCT_ALIGN_SZ - 1)

/* the following must be kept in this order
   for the memory region to cover the buffers */
static u8 src_buf[SRC_BUF_SZ];
static struct ib_buffers_st ib_buffers;
static __u32 memreg_size;
/* end of order constraint */

struct phys_mem_desc_st {
	unsigned long base;
	unsigned long offset;
};

static struct phys_mem_desc_st phys_mem;

static struct dev_pci_struct memfree_pci_dev;
static struct device_buffers_st *dev_buffers_p;
static struct device_ib_data_st dev_ib_data;



struct map_icm_st icm_map_obj;

static int gw_write_cr(__u32 addr, __u32 data)
{
	writel(htonl(data), memfree_pci_dev.cr_space + addr);
	return 0;
}

static int gw_read_cr(__u32 addr, __u32 * result)
{
	*result = ntohl(readl(memfree_pci_dev.cr_space + addr));
	return 0;
}

static int reset_hca(void)
{
	return gw_write_cr(MEMFREE_RESET_OFFSET, 1);
}

static int ib_device_init(struct pci_device *dev)
{
	int i;
	int rc;

	tprintf("");

	memset(&dev_ib_data, 0, sizeof dev_ib_data);

	/* save bars */
	tprintf("bus=%d devfn=0x%x", dev->bus, dev->devfn);
	for (i = 0; i < 6; ++i) {
		memfree_pci_dev.dev.bar[i] =
		    pci_bar_start(dev, PCI_BASE_ADDRESS_0 + (i << 2));
		tprintf("bar[%d]= 0x%08lx", i, memfree_pci_dev.dev.bar[i]);
	}

	tprintf("");
	/* save config space */
	for (i = 0; i < 64; ++i) {
		rc = pci_read_config_dword(dev, i << 2,
					   &memfree_pci_dev.dev.
					   dev_config_space[i]);
		if (rc) {
			eprintf("");
			return rc;
		}
		tprintf("config[%d]= 0x%08lx", i << 2,
			memfree_pci_dev.dev.dev_config_space[i]);
	}

	tprintf("");
	memfree_pci_dev.dev.dev = dev;

	/* map cr-space */
	memfree_pci_dev.cr_space =
	    ioremap(memfree_pci_dev.dev.bar[0], 0x100000);
	if (!memfree_pci_dev.cr_space) {
		eprintf("");
		return -1;
	}

	/* map uar */
	memfree_pci_dev.uar =
	    ioremap(memfree_pci_dev.dev.bar[2] + UAR_IDX * 0x1000, 0x1000);
	if (!memfree_pci_dev.uar) {
		eprintf("");
		return -1;
	}
	tprintf("uar_base (pa:va) = 0x%lx 0x%lx",
		memfree_pci_dev.dev.bar[2] + UAR_IDX * 0x1000,
		memfree_pci_dev.uar);

	tprintf("");

	return 0;
}

static inline unsigned long lalign(unsigned long buf, unsigned long align)
{
	return (unsigned long)((buf + align - 1) &
			       (~(((unsigned long)align) - 1)));
}

static int init_dev_data(void)
{
	unsigned long tmp;
	unsigned long reserve_size = 32 * 1024 * 1024;

	tmp = lalign(virt_to_bus(src_buf), STRUCT_ALIGN_SZ);

	dev_buffers_p = bus_to_virt(tmp);
	memreg_size = (__u32) (&memreg_size) - (__u32) dev_buffers_p;
	tprintf("src_buf=0x%lx, dev_buffers_p=0x%lx, memreg_size=0x%x", src_buf,
		dev_buffers_p, memreg_size);

	tprintf("inprm: va=0x%lx, pa=0x%lx", dev_buffers_p->inprm_buf,
		virt_to_bus(dev_buffers_p->inprm_buf));
	tprintf("outprm: va=0x%lx, pa=0x%lx", dev_buffers_p->outprm_buf,
		virt_to_bus(dev_buffers_p->outprm_buf));

	phys_mem.base =
	    (virt_to_phys(_text) - reserve_size) & (~(reserve_size - 1));

	phys_mem.offset = 0;

	return 0;
}

static int restore_config(void)
{
	int i;
	int rc;

	for (i = 0; i < 64; ++i) {
		if (i != 22 && i != 23) {
			rc = pci_write_config_dword(memfree_pci_dev.dev.dev,
						    i << 2,
						    memfree_pci_dev.dev.
						    dev_config_space[i]);
			if (rc) {
				return rc;
			}
		}
	}
	return 0;
}

static void prep_init_hca_buf(struct init_hca_st *init_hca_p, void *buf)
{
	unsigned long ptr;
	__u8 shift;

	memset(buf, 0, MT_STRUCT_SIZE(arbelprm_init_hca_st));

	ptr = (unsigned long)buf +
	    MT_BYTE_OFFSET(arbelprm_init_hca_st,
			   qpc_eec_cqc_eqc_rdb_parameters);

	shift = 32 - MT_BIT_SIZE(arbelprm_qpcbaseaddr_st, qpc_base_addr_l);
	INS_FLD(init_hca_p->qpc_base_addr_h, ptr, arbelprm_qpcbaseaddr_st,
		qpc_base_addr_h);
	INS_FLD(init_hca_p->qpc_base_addr_l >> shift, ptr,
		arbelprm_qpcbaseaddr_st, qpc_base_addr_l);
	INS_FLD(init_hca_p->log_num_of_qp, ptr, arbelprm_qpcbaseaddr_st,
		log_num_of_qp);

	shift = 32 - MT_BIT_SIZE(arbelprm_qpcbaseaddr_st, eec_base_addr_l);
	INS_FLD(init_hca_p->eec_base_addr_h, ptr, arbelprm_qpcbaseaddr_st,
		eec_base_addr_h);
	INS_FLD(init_hca_p->eec_base_addr_l >> shift, ptr,
		arbelprm_qpcbaseaddr_st, eec_base_addr_l);
	INS_FLD(init_hca_p->log_num_of_ee, ptr, arbelprm_qpcbaseaddr_st,
		log_num_of_ee);

	shift = 32 - MT_BIT_SIZE(arbelprm_qpcbaseaddr_st, srqc_base_addr_l);
	INS_FLD(init_hca_p->srqc_base_addr_h, ptr, arbelprm_qpcbaseaddr_st,
		srqc_base_addr_h);
	INS_FLD(init_hca_p->srqc_base_addr_l >> shift, ptr,
		arbelprm_qpcbaseaddr_st, srqc_base_addr_l);
	INS_FLD(init_hca_p->log_num_of_srq, ptr, arbelprm_qpcbaseaddr_st,
		log_num_of_srq);

	shift = 32 - MT_BIT_SIZE(arbelprm_qpcbaseaddr_st, cqc_base_addr_l);
	INS_FLD(init_hca_p->cqc_base_addr_h, ptr, arbelprm_qpcbaseaddr_st,
		cqc_base_addr_h);
	INS_FLD(init_hca_p->cqc_base_addr_l >> shift, ptr,
		arbelprm_qpcbaseaddr_st, cqc_base_addr_l);
	INS_FLD(init_hca_p->log_num_of_cq, ptr, arbelprm_qpcbaseaddr_st,
		log_num_of_cq);

	INS_FLD(init_hca_p->eqpc_base_addr_h, ptr, arbelprm_qpcbaseaddr_st,
		eqpc_base_addr_h);
	INS_FLD(init_hca_p->eqpc_base_addr_l, ptr, arbelprm_qpcbaseaddr_st,
		eqpc_base_addr_l);

	INS_FLD(init_hca_p->eeec_base_addr_h, ptr, arbelprm_qpcbaseaddr_st,
		eeec_base_addr_h);
	INS_FLD(init_hca_p->eeec_base_addr_l, ptr, arbelprm_qpcbaseaddr_st,
		eeec_base_addr_l);

	shift = 32 - MT_BIT_SIZE(arbelprm_qpcbaseaddr_st, eqc_base_addr_l);
	INS_FLD(init_hca_p->eqc_base_addr_h, ptr, arbelprm_qpcbaseaddr_st,
		eqc_base_addr_h);
	INS_FLD(init_hca_p->eqc_base_addr_l >> shift, ptr,
		arbelprm_qpcbaseaddr_st, eqc_base_addr_l);
	INS_FLD(init_hca_p->log_num_of_eq, ptr, arbelprm_qpcbaseaddr_st,
		log_num_eq);

	INS_FLD(init_hca_p->rdb_base_addr_h, ptr, arbelprm_qpcbaseaddr_st,
		rdb_base_addr_h);
	INS_FLD(init_hca_p->rdb_base_addr_l, ptr, arbelprm_qpcbaseaddr_st,
		rdb_base_addr_l);

	ptr = (unsigned long)buf +
	    MT_BYTE_OFFSET(arbelprm_init_hca_st, multicast_parameters);

	INS_FLD(init_hca_p->mc_base_addr_h, ptr, arbelprm_multicastparam_st,
		mc_base_addr_h);
	INS_FLD(init_hca_p->mc_base_addr_l, ptr, arbelprm_multicastparam_st,
		mc_base_addr_l);
	INS_FLD(init_hca_p->log_mc_table_entry_sz, ptr,
		arbelprm_multicastparam_st, log_mc_table_entry_sz);
	INS_FLD(init_hca_p->mc_table_hash_sz, ptr, arbelprm_multicastparam_st,
		mc_table_hash_sz);
	INS_FLD(init_hca_p->log_mc_table_sz, ptr, arbelprm_multicastparam_st,
		log_mc_table_sz);

	ptr = (unsigned long)buf +
	    MT_BYTE_OFFSET(arbelprm_init_hca_st, tpt_parameters);

	INS_FLD(init_hca_p->mpt_base_addr_h, ptr, arbelprm_tptparams_st,
		mpt_base_adr_h);
	INS_FLD(init_hca_p->mpt_base_addr_l, ptr, arbelprm_tptparams_st,
		mpt_base_adr_l);
	INS_FLD(init_hca_p->log_mpt_sz, ptr, arbelprm_tptparams_st, log_mpt_sz);
	INS_FLD(init_hca_p->mtt_base_addr_h, ptr, arbelprm_tptparams_st,
		mtt_base_addr_h);
	INS_FLD(init_hca_p->mtt_base_addr_l, ptr, arbelprm_tptparams_st,
		mtt_base_addr_l);

	ptr = (unsigned long)buf +
	    MT_BYTE_OFFSET(arbelprm_init_hca_st, uar_parameters);

	INS_FLD(init_hca_p->log_max_uars, ptr, arbelprm_uar_params_st,
		log_max_uars);

}

static void prep_sw2hw_mpt_buf(void *buf, __u32 mkey)
{
	INS_FLD(1, buf, arbelprm_mpt_st, lw);
	INS_FLD(1, buf, arbelprm_mpt_st, lr);
	INS_FLD(1, buf, arbelprm_mpt_st, pa);
	INS_FLD(1, buf, arbelprm_mpt_st, r_w);
	INS_FLD(mkey, buf, arbelprm_mpt_st, mem_key);
	INS_FLD(GLOBAL_PD, buf, arbelprm_mpt_st, pd);
	INS_FLD(virt_to_bus(dev_buffers_p), buf, arbelprm_mpt_st,
		start_address_l);
	INS_FLD(memreg_size, buf, arbelprm_mpt_st, reg_wnd_len_l);
}

static void prep_sw2hw_eq_buf(void *buf, struct eqe_t *eq_buf)
{
	memset(buf, 0, MT_STRUCT_SIZE(arbelprm_eqc_st));

	INS_FLD(0xa, buf, arbelprm_eqc_st, st);	/* fired */
	INS_FLD(virt_to_bus(eq_buf), buf, arbelprm_eqc_st, start_address_l);
	INS_FLD(LOG2_EQ_SZ, buf, arbelprm_eqc_st, log_eq_size);
	INS_FLD(GLOBAL_PD, buf, arbelprm_eqc_st, pd);
	INS_FLD(dev_ib_data.mkey, buf, arbelprm_eqc_st, lkey);
}

static void init_eq_buf(void *eq_buf)
{
	struct eqe_t *eq = eq_buf;
	int i, num_eqes = 1 << LOG2_EQ_SZ;

	memset(eq, 0, num_eqes * sizeof eq[0]);
	for (i = 0; i < num_eqes; ++i)
		WRITE_BYTE_VOL(&eq[i], EQE_OWNER_OFFSET, EQE_OWNER_VAL_HW);
}

static void prep_init_ib_buf(void *buf)
{
	memset(buf, 0, MT_STRUCT_SIZE(arbelprm_init_ib_st));

	INS_FLD(MTU_2048, buf, arbelprm_init_ib_st, mtu_cap);
	INS_FLD(3, buf, arbelprm_init_ib_st, port_width_cap);
	INS_FLD(1, buf, arbelprm_init_ib_st, vl_cap);
	INS_FLD(1, buf, arbelprm_init_ib_st, max_gid);
	INS_FLD(64, buf, arbelprm_init_ib_st, max_pkey);
}

static void prep_sw2hw_cq_buf(void *buf, __u8 eqn,
			      __u32 cqn,
			      union cqe_st *cq_buf,
			      __u32 cq_ci_db_record, __u32 cq_state_db_record)
{
	memset(buf, 0, MT_STRUCT_SIZE(arbelprm_completion_queue_context_st));

	INS_FLD(0xA, buf, arbelprm_completion_queue_context_st, st);
	INS_FLD(virt_to_bus(cq_buf), buf, arbelprm_completion_queue_context_st,
		start_address_l);
	INS_FLD(LOG2_CQ_SZ, buf, arbelprm_completion_queue_context_st,
		log_cq_size);
	INS_FLD(dev_ib_data.uar_idx, buf, arbelprm_completion_queue_context_st,
		usr_page);
	INS_FLD(eqn, buf, arbelprm_completion_queue_context_st, c_eqn);
	INS_FLD(GLOBAL_PD, buf, arbelprm_completion_queue_context_st, pd);
	INS_FLD(dev_ib_data.mkey, buf, arbelprm_completion_queue_context_st,
		l_key);
	INS_FLD(cqn, buf, arbelprm_completion_queue_context_st, cqn);
	INS_FLD(cq_ci_db_record, buf, arbelprm_completion_queue_context_st,
		cq_ci_db_record);
	INS_FLD(cq_state_db_record, buf, arbelprm_completion_queue_context_st,
		cq_state_db_record);
}

static void prep_rst2init_qpee_buf(void *buf,
				   __u32 snd_cqn,
				   __u32 rcv_cqn,
				   __u32 qkey,
				   __u32 log_rq_size,
				   __u32 log_rq_stride,
				   __u32 log_sq_size,
				   __u32 log_sq_stride,
				   __u32 snd_wqe_base_adr_l,
				   __u32 snd_db_record_index,
				   __u32 rcv_wqe_base_adr_l,
				   __u32 rcv_db_record_index)
{
	void *tmp;
	int shift;
	struct qp_ee_state_tarnisition_st *prm = buf;

	memset(buf, 0, sizeof *prm);

	tprintf("snd_cqn=0x%lx", snd_cqn);
	tprintf("rcv_cqn=0x%lx", rcv_cqn);
	tprintf("qkey=0x%lx", qkey);
	tprintf("log_rq_size=0x%lx", log_rq_size);
	tprintf("log_rq_stride=0x%lx", log_rq_stride);
	tprintf("log_sq_size=0x%lx", log_sq_size);
	tprintf("log_sq_stride=0x%lx", log_sq_stride);
	tprintf("snd_wqe_base_adr_l=0x%lx", snd_wqe_base_adr_l);
	tprintf("snd_db_record_index=0x%lx", snd_db_record_index);
	tprintf("rcv_wqe_base_adr_l=0x%lx", rcv_wqe_base_adr_l);
	tprintf("rcv_db_record_index=0x%lx", rcv_db_record_index);

	tmp = &prm->ctx;
	INS_FLD(TS_UD, tmp, arbelprm_queue_pair_ee_context_entry_st, st);
	INS_FLD(PM_STATE_MIGRATED, tmp, arbelprm_queue_pair_ee_context_entry_st,
		pm_state);
	INS_FLD(1, tmp, arbelprm_queue_pair_ee_context_entry_st, de);
	INS_FLD(MTU_2048, tmp, arbelprm_queue_pair_ee_context_entry_st, mtu);
	INS_FLD(11, tmp, arbelprm_queue_pair_ee_context_entry_st, msg_max);
	INS_FLD(log_rq_size, tmp, arbelprm_queue_pair_ee_context_entry_st,
		log_rq_size);
	INS_FLD(log_rq_stride, tmp, arbelprm_queue_pair_ee_context_entry_st,
		log_rq_stride);
	INS_FLD(log_sq_size, tmp, arbelprm_queue_pair_ee_context_entry_st,
		log_sq_size);
	INS_FLD(log_sq_stride, tmp, arbelprm_queue_pair_ee_context_entry_st,
		log_sq_stride);
	INS_FLD(dev_ib_data.uar_idx, tmp,
		arbelprm_queue_pair_ee_context_entry_st, usr_page);
	INS_FLD(GLOBAL_PD, tmp, arbelprm_queue_pair_ee_context_entry_st, pd);
	INS_FLD(dev_ib_data.mkey, tmp, arbelprm_queue_pair_ee_context_entry_st,
		wqe_lkey);
	INS_FLD(1, tmp, arbelprm_queue_pair_ee_context_entry_st, ssc);
	INS_FLD(snd_cqn, tmp, arbelprm_queue_pair_ee_context_entry_st, cqn_snd);
	shift =
	    32 - MT_BIT_SIZE(arbelprm_queue_pair_ee_context_entry_st,
			     snd_wqe_base_adr_l);
	INS_FLD(snd_wqe_base_adr_l >> shift, tmp,
		arbelprm_queue_pair_ee_context_entry_st, snd_wqe_base_adr_l);
	INS_FLD(snd_db_record_index, tmp,
		arbelprm_queue_pair_ee_context_entry_st, snd_db_record_index);
	INS_FLD(1, tmp, arbelprm_queue_pair_ee_context_entry_st, rsc);
	INS_FLD(rcv_cqn, tmp, arbelprm_queue_pair_ee_context_entry_st, cqn_rcv);
	shift =
	    32 - MT_BIT_SIZE(arbelprm_queue_pair_ee_context_entry_st,
			     rcv_wqe_base_adr_l);
	INS_FLD(rcv_wqe_base_adr_l >> shift, tmp,
		arbelprm_queue_pair_ee_context_entry_st, rcv_wqe_base_adr_l);
	INS_FLD(rcv_db_record_index, tmp,
		arbelprm_queue_pair_ee_context_entry_st, rcv_db_record_index);
	INS_FLD(qkey, tmp, arbelprm_queue_pair_ee_context_entry_st, q_key);

	tmp =
	    (__u8 *) (&prm->ctx) +
	    MT_BYTE_OFFSET(arbelprm_queue_pair_ee_context_entry_st,
			   primary_address_path);
	INS_FLD(dev_ib_data.port, tmp, arbelprm_address_path_st, port_number);

}

static void prep_init2rtr_qpee_buf(void *buf)
{
	struct qp_ee_state_tarnisition_st *prm;

	prm = (struct qp_ee_state_tarnisition_st *)buf;

	memset(prm, 0, sizeof *prm);

	INS_FLD(MTU_2048, &prm->ctx, arbelprm_queue_pair_ee_context_entry_st,
		mtu);
	INS_FLD(11, &prm->ctx, arbelprm_queue_pair_ee_context_entry_st,
		msg_max);
}

static void init_av_array(void)
{
}

/*
 * my_log2()
 */
static int my_log2(unsigned long arg)
{
	int i;
	__u32 tmp;

	if (arg == 0) {
		return INT_MIN;	/* log2(0) = -infinity */
	}

	tmp = 1;
	i = 0;
	while (tmp < arg) {
		tmp = tmp << 1;
		++i;
	}

	return i;
}

/*
 * get_req_icm_pages
 */
static unsigned long get_req_icm_pages(unsigned long log2_reserved,
				       unsigned long app_rsrc,
				       unsigned long entry_size,
				       unsigned long *log2_entries_p)
{
	unsigned long size;
	unsigned long log2_entries;

	log2_entries = my_log2((1 << log2_reserved) + app_rsrc);
	*log2_entries_p = log2_entries;
	size = (1 << log2_entries) * entry_size;

	return (size + 4095) >> 12;
}

static void init_uar_context(void *uar_context_va)
{
	void *ptr;
	/* clear all uar context */
	memset(uar_context_va, 0, 4096);

	ptr = uar_context_va + MADS_RCV_CQ_ARM_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_CQ_ARM, ptr, arbelprm_cq_arm_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.mads_qp.rcv_cq.cqn, ptr,
		      arbelprm_cq_arm_db_record_st, cq_number);

	ptr = uar_context_va + MADS_SND_CQ_ARM_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_CQ_ARM, ptr, arbelprm_cq_arm_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.mads_qp.snd_cq.cqn, ptr,
		      arbelprm_cq_arm_db_record_st, cq_number);

	ptr = uar_context_va + IPOIB_RCV_CQ_ARM_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_CQ_ARM, ptr, arbelprm_cq_arm_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.ipoib_qp.rcv_cq.cqn, ptr,
		      arbelprm_cq_arm_db_record_st, cq_number);

	ptr = uar_context_va + IPOIB_SND_CQ_ARM_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_CQ_ARM, ptr, arbelprm_cq_arm_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.ipoib_qp.snd_cq.cqn, ptr,
		      arbelprm_cq_arm_db_record_st, cq_number);

	ptr = uar_context_va + MADS_SND_QP_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_SQ_DBELL, ptr, arbelprm_qp_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.mads_qp.qpn, ptr, arbelprm_qp_db_record_st,
		      qp_number);

	ptr = uar_context_va + IPOIB_SND_QP_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_SQ_DBELL, ptr, arbelprm_qp_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.ipoib_qp.qpn, ptr, arbelprm_qp_db_record_st,
		      qp_number);

	ptr = uar_context_va + GROUP_SEP_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_GROUP_SEP, ptr, arbelprm_cq_arm_db_record_st,
		      res);

	ptr = uar_context_va + MADS_RCV_QP_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_RQ_DBELL, ptr, arbelprm_qp_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.mads_qp.qpn, ptr, arbelprm_qp_db_record_st,
		      qp_number);

	ptr = uar_context_va + IPOIB_RCV_QP_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_RQ_DBELL, ptr, arbelprm_qp_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.ipoib_qp.qpn, ptr, arbelprm_qp_db_record_st,
		      qp_number);

	ptr = uar_context_va + MADS_RCV_CQ_CI_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_CQ_SET_CI, ptr, arbelprm_cq_ci_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.mads_qp.rcv_cq.cqn, ptr,
		      arbelprm_cq_ci_db_record_st, cq_number);

	ptr = uar_context_va + MADS_SND_CQ_CI_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_CQ_SET_CI, ptr, arbelprm_cq_ci_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.mads_qp.snd_cq.cqn, ptr,
		      arbelprm_cq_ci_db_record_st, cq_number);

	ptr = uar_context_va + IPOIB_RCV_CQ_CI_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_CQ_SET_CI, ptr, arbelprm_cq_ci_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.ipoib_qp.rcv_cq.cqn, ptr,
		      arbelprm_cq_ci_db_record_st, cq_number);

	ptr = uar_context_va + IPOIB_SND_CQ_CI_DB_IDX * 8;
	INS_FLD_TO_BE(UAR_RES_CQ_SET_CI, ptr, arbelprm_cq_ci_db_record_st, res);
	INS_FLD_TO_BE(dev_ib_data.ipoib_qp.snd_cq.cqn, ptr,
		      arbelprm_cq_ci_db_record_st, cq_number);

}

static int setup_hca(__u8 port, void **eq_p)
{
	int ret;
	int rc;
	struct query_fw_st qfw;
	struct map_icm_st map_obj;
	struct dev_lim_st dev_lim;
	struct init_hca_st init_hca;
	__u8 log2_pages;
	unsigned long icm_start, icm_size, tmp;
	unsigned long log2_entries;
	__u32 aux_pages;
	__u32 mem_key, key, tmp_key;
	__u8 eqn;
	__u32 event_mask;
	struct eqe_t *eq_buf;
	void *inprm;
	unsigned long bus_addr;
	struct query_adapter_st qa;
	__u8 log_max_uars = 1;
	void *uar_context_va;
	__u32 uar_context_pa;

	tprintf("called");
	init_dev_data();
	inprm = get_inprm_buf();

	rc = reset_hca();
	if (rc) {
		eprintf("");
		return rc;
	} else {
		tprintf("reset_hca() success");
	}

	mdelay(1000);		/* wait for 1 sec */

	rc = restore_config();
	if (rc) {
		eprintf("");
		return rc;
	} else {
		tprintf("restore_config() success");
	}

	dev_ib_data.pd = GLOBAL_PD;
	dev_ib_data.port = port;
	dev_ib_data.qkey = GLOBAL_QKEY;

	rc = cmd_query_fw(&qfw);
	if (rc) {
		eprintf("");
		return rc;
	}
	else {
		tprintf("cmd_query_fw() success");

		if (print_info) {
			printf("FW ver = %d.%d.%d\n",
			qfw.fw_rev_major,
			qfw.fw_rev_minor,
			qfw.fw_rev_subminor);
		}

		tprintf("fw_rev_major=%d", qfw.fw_rev_major);
		tprintf("fw_rev_minor=%d", qfw.fw_rev_minor);
		tprintf("fw_rev_subminor=%d", qfw.fw_rev_subminor);
		tprintf("error_buf_start_h=0x%x", qfw.error_buf_start_h);
		tprintf("error_buf_start_l=0x%x", qfw.error_buf_start_l);
		tprintf("error_buf_size=%d", qfw.error_buf_size);
	}



	bus_addr =
	    ((unsigned long)((u64) qfw.error_buf_start_h << 32) | qfw.
	     error_buf_start_l);
    dev_ib_data.error_buf_addr= ioremap(bus_addr,
										qfw.error_buf_size*4);
	dev_ib_data.error_buf_size= qfw.error_buf_size;
	if (!dev_ib_data.error_buf_addr) {
		eprintf("");
		return -1;
	}


	bus_addr =
	    ((unsigned long)((u64) qfw.clear_int_addr.addr_h << 32) | qfw.
	     clear_int_addr.addr_l);
	dev_ib_data.clr_int_addr = bus_to_virt(bus_addr);

	rc = cmd_enable_lam();
	if (rc == 0x22 /* LAM_NOT_PRE -- need to put a name here */ ) {
		// ??????
	} else if (rc == 0) {
		// ??????
	} else {
		eprintf("");
		return rc;
	}

	log2_pages = my_log2(qfw.fw_pages);

	memset(&map_obj, 0, sizeof map_obj);
	map_obj.num_vpm = 1;
	map_obj.vpm_arr[0].log2_size = log2_pages;
	map_obj.vpm_arr[0].pa_l = phys_mem.base + phys_mem.offset;
	rc = cmd_map_fa(&map_obj);
	if (rc) {
		eprintf("");
		return rc;
	}
	phys_mem.offset += 1 << (log2_pages + 12);

	rc = cmd_run_fw();
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_map_fa;
	}

	rc = cmd_mod_stat_cfg();
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_map_fa;
	}

	rc = cmd_query_dev_lim(&dev_lim);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_map_fa;
	}

	dev_ib_data.uar_idx = dev_lim.num_rsvd_uars;

	tprintf("max_icm_size_h=0x%lx", dev_lim.max_icm_size_h);
	tprintf("max_icm_size_l=0x%lx", dev_lim.max_icm_size_l);

	memset(&init_hca, 0, sizeof init_hca);
	icm_start = 0;
	icm_size = 0;

	icm_start += ((dev_lim.num_rsvd_uars + 1) << 12);
	icm_size += ((dev_lim.num_rsvd_uars + 1) << 12);

	tmp = get_req_icm_pages(dev_lim.log2_rsvd_qps,
				MAX_APP_QPS,
				dev_lim.qpc_entry_sz, &log2_entries);
	init_hca.qpc_base_addr_l = icm_start;
	init_hca.log_num_of_qp = log2_entries;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	init_hca.eqpc_base_addr_l = icm_start;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	tmp = get_req_icm_pages(dev_lim.log2_rsvd_srqs,
				0, dev_lim.srq_entry_sz, &log2_entries);
	init_hca.srqc_base_addr_l = icm_start;
	init_hca.log_num_of_srq = log2_entries;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	tmp = get_req_icm_pages(dev_lim.log2_rsvd_ees,
				0, dev_lim.eec_entry_sz, &log2_entries);
	init_hca.eec_base_addr_l = icm_start;
	init_hca.log_num_of_ee = log2_entries;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	init_hca.eeec_base_addr_l = icm_start;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	tmp = get_req_icm_pages(dev_lim.log2_rsvd_cqs,
				MAX_APP_CQS,
				dev_lim.cqc_entry_sz, &log2_entries);
	init_hca.cqc_base_addr_l = icm_start;
	init_hca.log_num_of_cq = log2_entries;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	tmp = get_req_icm_pages(dev_lim.log2_rsvd_mtts,
				0, dev_lim.mtt_entry_sz, &log2_entries);
	init_hca.mtt_base_addr_l = icm_start;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	tmp = get_req_icm_pages(dev_lim.log2_rsvd_mrws,
				1, dev_lim.mpt_entry_sz, &log2_entries);
	init_hca.mpt_base_addr_l = icm_start;
	init_hca.log_mpt_sz = log2_entries;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	tmp = get_req_icm_pages(dev_lim.log2_rsvd_rdbs, 1, 32,	/* size of rdb entry */
				&log2_entries);
	init_hca.rdb_base_addr_l = icm_start;
	icm_start += (tmp << 12);
	icm_size += (tmp << 12);

	init_hca.eqc_base_addr_l = icm_start;
	init_hca.log_num_of_eq = LOG2_EQS;
	tmp = dev_lim.eqc_entry_sz * (1 << LOG2_EQS);
	icm_start += tmp;
	icm_size += tmp;

	init_hca.mc_base_addr_l = icm_start;
	init_hca.log_mc_table_entry_sz =
	    my_log2(MT_STRUCT_SIZE(arbelprm_mgm_entry_st));
	init_hca.mc_table_hash_sz = 8;
	init_hca.log_mc_table_sz = 3;
	icm_size +=
	    (MT_STRUCT_SIZE(arbelprm_mgm_entry_st) * init_hca.mc_table_hash_sz);
	icm_start +=
	    (MT_STRUCT_SIZE(arbelprm_mgm_entry_st) * init_hca.mc_table_hash_sz);

	rc = cmd_set_icm_size(icm_size, &aux_pages);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_map_fa;
	}

	memset(&map_obj, 0, sizeof map_obj);
	map_obj.num_vpm = 1;
	map_obj.vpm_arr[0].pa_l = phys_mem.base + phys_mem.offset;
	map_obj.vpm_arr[0].log2_size = my_log2(aux_pages);
	rc = cmd_map_icm_aux(&map_obj);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_map_fa;
	}
	phys_mem.offset += (1 << (map_obj.vpm_arr[0].log2_size + 12));

	uar_context_pa = phys_mem.base + phys_mem.offset +
	    dev_ib_data.uar_idx * 4096;
	uar_context_va = phys_to_virt(uar_context_pa);
	tprintf("uar_context: va=0x%lx, pa=0x%lx", uar_context_va,
		uar_context_pa);
	dev_ib_data.uar_context_base = uar_context_va;

	memset(&map_obj, 0, sizeof map_obj);
	map_obj.num_vpm = 1;
	map_obj.vpm_arr[0].pa_l = phys_mem.base + phys_mem.offset;
	map_obj.vpm_arr[0].log2_size = my_log2((icm_size + 4095) >> 12);
	rc = cmd_map_icm(&map_obj);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_map_fa;
	}
	icm_map_obj = map_obj;

	phys_mem.offset += (1 << (map_obj.vpm_arr[0].log2_size + 12));

	init_hca.log_max_uars = log_max_uars;
	tprintf("inprm: va=0x%lx, pa=0x%lx", inprm, virt_to_bus(inprm));
	prep_init_hca_buf(&init_hca, inprm);
	rc = cmd_init_hca(inprm, MT_STRUCT_SIZE(arbelprm_init_hca_st));
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_map_fa;
	}

	rc = cmd_query_adapter(&qa);
	if (rc) {
		eprintf("");
		return rc;
	}
	dev_ib_data.clr_int_data = 1 << qa.intapin;

	tmp_key = 1 << dev_lim.log2_rsvd_mrws | MKEY_PREFIX;
	mem_key = 1 << (dev_lim.log2_rsvd_mrws + 8) | (MKEY_PREFIX >> 24);
	prep_sw2hw_mpt_buf(inprm, tmp_key);
	rc = cmd_sw2hw_mpt(&key, 1 << dev_lim.log2_rsvd_mrws, inprm,
			   SW2HW_MPT_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_map_fa;
	} else {
		tprintf("cmd_sw2hw_mpt() success, key=0x%lx", mem_key);
	}
	dev_ib_data.mkey = mem_key;

	eqn = EQN;
	/* allocate a single EQ which will receive 
	   all the events */
	eq_buf = dev_buffers_p->eq_buf;
	init_eq_buf(eq_buf);	/* put in HW ownership */
	prep_sw2hw_eq_buf(inprm, eq_buf);
	rc = cmd_sw2hw_eq(SW2HW_EQ_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_sw2hw_mpt;
	} else
		tprintf("cmd_sw2hw_eq() success");

	event_mask = (1 << XDEV_EV_TYPE_CQ_COMP) |
	    (1 << XDEV_EV_TYPE_CQ_ERR) |
	    (1 << XDEV_EV_TYPE_LOCAL_WQ_CATAS_ERR) |
	    (1 << XDEV_EV_TYPE_PORT_ERR) |
	    (1 << XDEV_EV_TYPE_LOCAL_WQ_INVALID_REQ_ERR) |
	    (1 << XDEV_EV_TYPE_LOCAL_WQ_ACCESS_VIOL_ERR) |
	    (1 << TAVOR_IF_EV_TYPE_OVERRUN);
	rc = cmd_map_eq(eqn, event_mask, 1);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_sw2hw_eq;
	} else
		tprintf("cmd_map_eq() success");

	dev_ib_data.eq.eqn = eqn;
	dev_ib_data.eq.eq_buf = eq_buf;
	dev_ib_data.eq.cons_counter = 0;
	dev_ib_data.eq.eq_size = 1 << LOG2_EQ_SZ;
	bus_addr =
	    ((unsigned long)((u64) qfw.eq_ci_table.addr_h << 32) | qfw.
	     eq_ci_table.addr_l)
	    + eqn * 8;
	dev_ib_data.eq.ci_base_base_addr = bus_to_virt(bus_addr);
	*eq_p = &dev_ib_data.eq;

	prep_init_ib_buf(inprm);
	rc = cmd_init_ib(port, inprm, INIT_IB_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_sw2hw_eq;
	} else
		tprintf("cmd_init_ib() success");

	init_av_array();
	tprintf("init_av_array() done");

	/* set the qp and cq numbers according
	   to the results of query_dev_lim */
	dev_ib_data.mads_qp.qpn = (1 << dev_lim.log2_rsvd_qps) +
	    +QPN_BASE + MADS_QPN_SN;
	dev_ib_data.ipoib_qp.qpn = (1 << dev_lim.log2_rsvd_qps) +
	    +QPN_BASE + IPOIB_QPN_SN;

	dev_ib_data.mads_qp.snd_cq.cqn = (1 << dev_lim.log2_rsvd_cqs) +
	    MADS_SND_CQN_SN;
	dev_ib_data.mads_qp.rcv_cq.cqn = (1 << dev_lim.log2_rsvd_cqs) +
	    MADS_RCV_CQN_SN;

	dev_ib_data.ipoib_qp.snd_cq.cqn = (1 << dev_lim.log2_rsvd_cqs) +
	    IPOIB_SND_CQN_SN;
	dev_ib_data.ipoib_qp.rcv_cq.cqn = (1 << dev_lim.log2_rsvd_cqs) +
	    IPOIB_RCV_CQN_SN;

	init_uar_context(uar_context_va);

	ret = 0;
	goto exit;

      undo_sw2hw_eq:
	rc = cmd_hw2sw_eq(eqn);
	if (rc)
		eprintf("");
	else
		tprintf("cmd_hw2sw_eq() success");

      undo_sw2hw_mpt:
	rc = cmd_hw2sw_mpt(tmp_key);
	if (rc)
		eprintf("");

      undo_map_fa:
	rc = cmd_unmap_fa();
	if (rc)
		eprintf("");

      exit:
	return ret;
}


static int unset_hca(void)
{
	int rc, ret = 0;

	rc = cmd_unmap_icm(&icm_map_obj);
	if (rc)
		eprintf("");
	ret |= rc;


	rc = cmd_unmap_icm_aux();
	if (rc)
		eprintf("");
	ret |= rc;

	rc = cmd_unmap_fa();
	if (rc)
		eprintf("");
	ret |= rc;

	return ret;
}

static void *get_inprm_buf(void)
{
	return dev_buffers_p->inprm_buf;
}

static void *get_outprm_buf(void)
{
	return dev_buffers_p->outprm_buf;
}

static void *get_send_wqe_buf(void *wqe, __u8 index)
{
	struct ud_send_wqe_st *snd_wqe = wqe;

	return bus_to_virt(be32_to_cpu(snd_wqe->mpointer[index].local_addr_l));
}

static void *get_rcv_wqe_buf(void *wqe, __u8 index)
{
	struct recv_wqe_st *rcv_wqe = wqe;

	return bus_to_virt(be32_to_cpu(rcv_wqe->mpointer[index].local_addr_l));
}

static void modify_av_params(struct ud_av_st *av,
			     __u16 dlid,
			     __u8 g,
			     __u8 sl, __u8 rate, union ib_gid_u *gid, __u32 qpn)
{
	memset(&av->av, 0, sizeof av->av);

	INS_FLD_TO_BE(dev_ib_data.port, &av->av, arbelprm_ud_address_vector_st,
		      port_number);
	INS_FLD_TO_BE(dev_ib_data.pd, &av->av, arbelprm_ud_address_vector_st,
		      pd);
	INS_FLD_TO_BE(dlid, &av->av, arbelprm_ud_address_vector_st, rlid);
	INS_FLD_TO_BE(g, &av->av, arbelprm_ud_address_vector_st, g);
	INS_FLD_TO_BE(sl, &av->av, arbelprm_ud_address_vector_st, sl);
	INS_FLD_TO_BE(3, &av->av, arbelprm_ud_address_vector_st, msg);

	if (rate >= 3)
		INS_FLD_TO_BE(0, &av->av, arbelprm_ud_address_vector_st, max_stat_rate);	/* 4x */
	else
		INS_FLD_TO_BE(1, &av->av, arbelprm_ud_address_vector_st, max_stat_rate);	/* 1x */

	if (g) {
		if (gid) {
			INS_FLD(*((__u32 *) (&gid->raw[0])), &av->av,
				arbelprm_ud_address_vector_st, rgid_127_96);
			INS_FLD(*((__u32 *) (&gid->raw[4])), &av->av,
				arbelprm_ud_address_vector_st, rgid_95_64);
			INS_FLD(*((__u32 *) (&gid->raw[8])), &av->av,
				arbelprm_ud_address_vector_st, rgid_63_32);
			INS_FLD(*((__u32 *) (&gid->raw[12])), &av->av,
				arbelprm_ud_address_vector_st, rgid_31_0);
		} else {
			INS_FLD(0, &av->av, arbelprm_ud_address_vector_st,
				rgid_127_96);
			INS_FLD(0, &av->av, arbelprm_ud_address_vector_st,
				rgid_95_64);
			INS_FLD(0, &av->av, arbelprm_ud_address_vector_st,
				rgid_63_32);
			INS_FLD(0, &av->av, arbelprm_ud_address_vector_st,
				rgid_31_0);
		}
	} else {
		INS_FLD(0, &av->av, arbelprm_ud_address_vector_st, rgid_127_96);
		INS_FLD(0, &av->av, arbelprm_ud_address_vector_st, rgid_95_64);
		INS_FLD(0, &av->av, arbelprm_ud_address_vector_st, rgid_63_32);
		INS_FLD(2, &av->av, arbelprm_ud_address_vector_st, rgid_31_0);
	}
	av->dest_qp = qpn;
	av->qkey = dev_ib_data.qkey;
}

static void init_cq_buf(union cqe_st *cq_buf, __u8 num_cqes)
{
	int i;

	memset(cq_buf, 0, sizeof(union cqe_st) * num_cqes);
	for (i = 0; i < num_cqes; ++i) {
		WRITE_BYTE_VOL(&cq_buf[i], CQE_OWNER_OFFSET, CQE_OWNER_VAL_HW);
	}
}

static int post_rcv_buf(struct udqp_st *qp, struct recv_wqe_st *rcv_wqe)
{
	int i;

	/* put a valid lkey */
	for (i = 0; i < MAX_SCATTER; ++i) {
		rcv_wqe->mpointer[i].lkey = cpu_to_be32(dev_ib_data.mkey);
	}

	qp->post_rcv_counter++;
	WRITE_WORD_VOL(qp->rcv_uar_context, 2, htons(qp->post_rcv_counter));

	return 0;
}

static int post_send_req(void *qph, void *wqeh, __u8 num_gather)
{
	int rc;
	struct udqp_st *qp = qph;
	struct ud_send_wqe_st *snd_wqe = wqeh;
	struct send_doorbell_st dbell;
	__u32 nds;

	qp->post_send_counter++;

	WRITE_WORD_VOL(qp->send_uar_context, 2, htons(qp->post_send_counter));

	memset(&dbell, 0, sizeof dbell);
	INS_FLD(XDEV_NOPCODE_SEND, &dbell, arbelprm_send_doorbell_st, nopcode);
	INS_FLD(1, &dbell, arbelprm_send_doorbell_st, f);
	INS_FLD(qp->post_send_counter - 1, &dbell, arbelprm_send_doorbell_st,
		wqe_counter);
	INS_FLD(1, &dbell, arbelprm_send_doorbell_st, wqe_cnt);
	nds = (sizeof(snd_wqe->next) +
	       sizeof(snd_wqe->udseg) +
	       sizeof(snd_wqe->mpointer[0]) * num_gather) >> 4;
	INS_FLD(nds, &dbell, arbelprm_send_doorbell_st, nds);
	INS_FLD(qp->qpn, &dbell, arbelprm_send_doorbell_st, qpn);

	if (qp->last_posted_snd_wqe) {
		INS_FLD_TO_BE(nds,
			      &qp->last_posted_snd_wqe->next.next,
			      arbelprm_wqe_segment_next_st, nds);
		INS_FLD_TO_BE(1,
			      &qp->last_posted_snd_wqe->next.next,
			      arbelprm_wqe_segment_next_st, f);
		INS_FLD_TO_BE(XDEV_NOPCODE_SEND,
			      &qp->last_posted_snd_wqe->next.next,
			      arbelprm_wqe_segment_next_st, nopcode);
	}

	rc = cmd_post_doorbell(&dbell, POST_SND_OFFSET);
	if (!rc) {
		qp->last_posted_snd_wqe = snd_wqe;
	}

	return rc;

}

static int create_mads_qp(void **qp_pp, void **snd_cq_pp, void **rcv_cq_pp)
{
	__u8 i, next_i, j, k;
	int rc;
	struct udqp_st *qp;
	__u32 bus_addr;
	__u8 nds;
	void *ptr;

	qp = &dev_ib_data.mads_qp;

	/* set the pointer to the receive WQEs buffer */
	qp->rcv_wq = dev_buffers_p->mads_qp_rcv_queue;

	qp->send_buf_sz = MAD_BUF_SZ;
	qp->rcv_buf_sz = MAD_BUF_SZ;

	qp->max_recv_wqes = NUM_MADS_RCV_WQES;	/* max wqes in this work queue */
	qp->recv_wqe_cur_free = NUM_MADS_RCV_WQES;	/* current free wqes */
	qp->recv_wqe_alloc_idx = 0;	/* index from wqes can be allocated if there are free wqes */

	qp->rcv_uar_context =
	    dev_ib_data.uar_context_base + 8 * MADS_RCV_QP_DB_IDX;
	qp->send_uar_context =
	    dev_ib_data.uar_context_base + 8 * MADS_SND_QP_DB_IDX;

	memset(&qp->rcv_wq[0], 0, NUM_MADS_RCV_WQES * sizeof(qp->rcv_wq[0]));
	nds = sizeof(qp->rcv_wq[0].wqe) >> 4;
	/* iterrate through the list */
	for (j = 0, i = 0, next_i = 1;
	     j < NUM_MADS_RCV_WQES;
	     MOD_INC(i, NUM_MADS_RCV_WQES), MOD_INC(next_i, NUM_MADS_RCV_WQES),
	     ++j) {

		qp->rcv_bufs[i] = ib_buffers.rcv_mad_buf[i];
		/* link the WQE to the next one */
		bus_addr = virt_to_bus(&qp->rcv_wq[next_i].wqe);
		ptr = qp->rcv_wq[i].wqe.control +
		    MT_BYTE_OFFSET(arbelprm_wqe_segment_ctrl_recv_st,
				   wqe_segment_next);
		INS_FLD(bus_addr >> 6, ptr, arbelprm_recv_wqe_segment_next_st,
			nda_31_6);
		INS_FLD(nds, ptr, arbelprm_recv_wqe_segment_next_st, nds);

		/* set the allocated buffers */
		qp->rcv_bufs[i] = ib_buffers.rcv_mad_buf[i];
		bus_addr = virt_to_bus(qp->rcv_bufs[i]);
		qp->rcv_wq[i].wqe.mpointer[0].local_addr_l = bus_addr;
		qp->rcv_wq[i].wqe.mpointer[0].byte_count = GRH_SIZE;
		bus_addr = virt_to_bus(qp->rcv_bufs[i] + GRH_SIZE);
		qp->rcv_wq[i].wqe.mpointer[1].local_addr_l = bus_addr;
		qp->rcv_wq[i].wqe.mpointer[1].byte_count = MAD_BUF_SZ;

		for (k = 0; k < (((sizeof(qp->rcv_wq[i])) >> 4) - 1); ++k) {
			qp->rcv_wq[i].wqe.mpointer[k].lkey = INVALID_WQE_LKEY;
		}
	}
	cpu_to_be_buf(&qp->rcv_wq[0],
		      NUM_MADS_RCV_WQES * sizeof(qp->rcv_wq[0]));

	for (i = 0; i < qp->max_recv_wqes; ++i) {
		qp->rcv_wq[i].wqe_cont.qp = qp;
	}

	/* set the pointer to the send WQEs buffer */
	qp->snd_wq = dev_buffers_p->mads_qp_snd_queue;

	qp->snd_wqe_alloc_idx = 0;
	qp->max_snd_wqes = NUM_MADS_SND_WQES;
	qp->snd_wqe_cur_free = NUM_MADS_SND_WQES;

	memset(&qp->snd_wq[0], 0, NUM_MADS_SND_WQES * sizeof(qp->snd_wq[i]));
	/* iterrate through the list */
	for (j = 0, i = 0, next_i = 1;
	     j < NUM_MADS_RCV_WQES;
	     MOD_INC(i, NUM_MADS_SND_WQES), MOD_INC(next_i, NUM_MADS_SND_WQES),
	     ++j) {

		/* link the WQE to the next one */
		bus_addr = virt_to_bus(&qp->snd_wq[next_i].wqe_cont.wqe);
		INS_FLD(bus_addr >> 6, &qp->snd_wq[i].wqe_cont.wqe.next.next,
			arbelprm_wqe_segment_next_st, nda_31_6);

		/* set the allocated buffers */
		qp->snd_bufs[i] = ib_buffers.send_mad_buf[i];
		bus_addr = virt_to_bus(qp->snd_bufs[i]);
		qp->snd_wq[i].wqe_cont.wqe.mpointer[0].local_addr_l = bus_addr;
		qp->snd_wq[i].wqe_cont.wqe.mpointer[0].lkey = dev_ib_data.mkey;
		qp->snd_wq[i].wqe_cont.wqe.mpointer[0].byte_count =
		    qp->send_buf_sz;

	}

	cpu_to_be_buf(&qp->snd_wq[0],
		      NUM_MADS_SND_WQES * sizeof(qp->snd_wq[i]));

	for (i = 0; i < qp->max_snd_wqes; ++i) {
		qp->snd_wq[i].wqe_cont.qp = qp;
	}

	/* qp number and cq numbers are already set up */
	qp->snd_cq.cq_buf = dev_buffers_p->mads_snd_cq_buf;
	qp->rcv_cq.cq_buf = dev_buffers_p->mads_rcv_cq_buf;
	qp->snd_cq.num_cqes = NUM_MADS_SND_CQES;
	qp->rcv_cq.num_cqes = NUM_MADS_RCV_CQES;
	qp->snd_cq.arm_db_ctx_idx = MADS_SND_CQ_ARM_DB_IDX;
	qp->snd_cq.ci_db_ctx_idx = MADS_SND_CQ_CI_DB_IDX;
	qp->rcv_cq.arm_db_ctx_idx = MADS_RCV_CQ_ARM_DB_IDX;
	qp->rcv_cq.ci_db_ctx_idx = MADS_RCV_CQ_CI_DB_IDX;
	qp->rcv_db_record_index = MADS_RCV_QP_DB_IDX;
	qp->snd_db_record_index = MADS_SND_QP_DB_IDX;
	qp->qkey = GLOBAL_QKEY;
	rc = create_udqp(qp);
	if (!rc) {
		*qp_pp = qp;
		*snd_cq_pp = &qp->snd_cq;
		*rcv_cq_pp = &qp->rcv_cq;
	}

	return rc;
}

static int create_ipoib_qp(void **qp_pp,
			   void **snd_cq_pp, void **rcv_cq_pp, __u32 qkey)
{
	__u8 i, next_i, j, k;
	int rc;
	struct udqp_st *qp;
	__u32 bus_addr;
	__u8 nds;
	void *ptr;

	qp = &dev_ib_data.ipoib_qp;

	/* set the pointer to the receive WQEs buffer */
	qp->rcv_wq = dev_buffers_p->ipoib_qp_rcv_queue;

	qp->send_buf_sz = IPOIB_SND_BUF_SZ;
	qp->rcv_buf_sz = IPOIB_RCV_BUF_SZ;

	qp->max_recv_wqes = NUM_IPOIB_RCV_WQES;
	qp->recv_wqe_cur_free = NUM_IPOIB_RCV_WQES;

	qp->rcv_uar_context =
	    dev_ib_data.uar_context_base + 8 * IPOIB_RCV_QP_DB_IDX;
	qp->send_uar_context =
	    dev_ib_data.uar_context_base + 8 * IPOIB_SND_QP_DB_IDX;

	memset(&qp->rcv_wq[0], 0, NUM_IPOIB_RCV_WQES * sizeof(qp->rcv_wq[0]));
	nds = sizeof(qp->rcv_wq[0].wqe) >> 4;
	/* iterrate through the list */
	for (j = 0, i = 0, next_i = 1;
	     j < NUM_IPOIB_RCV_WQES;
	     MOD_INC(i, NUM_IPOIB_RCV_WQES), MOD_INC(next_i,
						     NUM_IPOIB_RCV_WQES), ++j) {

		/* link the WQE to the next one */
		bus_addr = virt_to_bus(&qp->rcv_wq[next_i].wqe);
		ptr = qp->rcv_wq[i].wqe.control +
		    MT_BYTE_OFFSET(arbelprm_wqe_segment_ctrl_recv_st,
				   wqe_segment_next);
		INS_FLD(bus_addr >> 6, ptr, arbelprm_recv_wqe_segment_next_st,
			nda_31_6);
		INS_FLD(nds, ptr, arbelprm_recv_wqe_segment_next_st, nds);

		/* set the allocated buffers */
		qp->rcv_bufs[i] = ib_buffers.ipoib_rcv_buf[i];
		bus_addr = virt_to_bus(qp->rcv_bufs[i]);
		qp->rcv_wq[i].wqe.mpointer[0].local_addr_l = bus_addr;
		qp->rcv_wq[i].wqe.mpointer[0].byte_count = GRH_SIZE;
		bus_addr = virt_to_bus(qp->rcv_bufs[i] + GRH_SIZE);
		qp->rcv_wq[i].wqe.mpointer[1].local_addr_l = bus_addr;
		qp->rcv_wq[i].wqe.mpointer[1].byte_count = IPOIB_RCV_BUF_SZ;

		for (k = 0; k < (((sizeof(qp->rcv_wq[i].wqe)) >> 4) - 1); ++k) {
			qp->rcv_wq[i].wqe.mpointer[k].lkey = INVALID_WQE_LKEY;
		}
	}
	cpu_to_be_buf(&qp->rcv_wq[0],
		      NUM_IPOIB_RCV_WQES * sizeof(qp->rcv_wq[0]));

	for (i = 0; i < qp->max_recv_wqes; ++i) {
		qp->rcv_wq[i].wqe_cont.qp = qp;
	}

	/* set the pointer to the send WQEs buffer */
	qp->snd_wq = dev_buffers_p->ipoib_qp_snd_queue;

	qp->snd_wqe_alloc_idx = 0;
	qp->max_snd_wqes = NUM_IPOIB_SND_WQES;
	qp->snd_wqe_cur_free = NUM_IPOIB_SND_WQES;

	memset(&qp->snd_wq[0], 0, NUM_IPOIB_SND_WQES * sizeof(qp->snd_wq[i]));
	/* iterrate through the list */
	for (j = 0, i = 0, next_i = 1;
	     j < NUM_IPOIB_RCV_WQES;
	     MOD_INC(i, NUM_IPOIB_SND_WQES), MOD_INC(next_i,
						     NUM_IPOIB_SND_WQES), ++j) {

		/* link the WQE to the next one */
		bus_addr = virt_to_bus(&qp->snd_wq[next_i].wqe_cont.wqe);
		INS_FLD(bus_addr >> 6, &qp->snd_wq[i].wqe_cont.wqe.next.next,
			arbelprm_wqe_segment_next_st, nda_31_6);

		/* set the allocated buffers */
		qp->snd_bufs[i] = ib_buffers.send_ipoib_buf[i];
		bus_addr = virt_to_bus(qp->snd_bufs[i]);
		qp->snd_wq[i].wqe_cont.wqe.mpointer[0].local_addr_l = bus_addr;
		qp->snd_wq[i].wqe_cont.wqe.mpointer[0].lkey = dev_ib_data.mkey;

	}
	cpu_to_be_buf(&qp->snd_wq[0],
		      NUM_IPOIB_SND_WQES * sizeof(qp->snd_wq[i]));

	for (i = 0; i < qp->max_snd_wqes; ++i) {
		qp->snd_wq[i].wqe_cont.qp = qp;
	}

	/* qp number and cq numbers are already set up */
	qp->snd_cq.cq_buf = dev_buffers_p->ipoib_snd_cq_buf;
	qp->rcv_cq.cq_buf = dev_buffers_p->ipoib_rcv_cq_buf;
	qp->snd_cq.num_cqes = NUM_IPOIB_SND_CQES;
	qp->rcv_cq.num_cqes = NUM_IPOIB_RCV_CQES;
	qp->snd_cq.arm_db_ctx_idx = IPOIB_SND_CQ_ARM_DB_IDX;
	qp->snd_cq.ci_db_ctx_idx = IPOIB_SND_CQ_CI_DB_IDX;
	qp->rcv_cq.arm_db_ctx_idx = IPOIB_RCV_CQ_ARM_DB_IDX;
	qp->rcv_cq.ci_db_ctx_idx = IPOIB_RCV_CQ_CI_DB_IDX;
	qp->rcv_db_record_index = IPOIB_RCV_QP_DB_IDX;
	qp->snd_db_record_index = IPOIB_SND_QP_DB_IDX;
	qp->qkey = qkey;
	rc = create_udqp(qp);
	if (!rc) {
		*qp_pp = qp;
		*snd_cq_pp = &qp->snd_cq;
		*rcv_cq_pp = &qp->rcv_cq;
	}

	return rc;
}

static int create_udqp(struct udqp_st *qp)
{
	int rc, ret = 0;
	void *inprm;
	struct recv_wqe_st *rcv_wqe;

	inprm = dev_buffers_p->inprm_buf;

	qp->rcv_cq.arm_db_ctx_pointer =
	    dev_ib_data.uar_context_base + 8 * qp->rcv_cq.arm_db_ctx_idx;
	qp->rcv_cq.ci_db_ctx_pointer =
	    dev_ib_data.uar_context_base + 8 * qp->rcv_cq.ci_db_ctx_idx;
	qp->snd_cq.arm_db_ctx_pointer =
	    dev_ib_data.uar_context_base + 8 * qp->snd_cq.arm_db_ctx_idx;
	qp->snd_cq.ci_db_ctx_pointer =
	    dev_ib_data.uar_context_base + 8 * qp->snd_cq.ci_db_ctx_idx;

	/* create send CQ */
	init_cq_buf(qp->snd_cq.cq_buf, qp->snd_cq.num_cqes);
	qp->snd_cq.cons_counter = 0;
	prep_sw2hw_cq_buf(inprm,
			  dev_ib_data.eq.eqn,
			  qp->snd_cq.cqn,
			  qp->snd_cq.cq_buf,
			  qp->snd_cq.ci_db_ctx_idx, qp->snd_cq.arm_db_ctx_idx);

	rc = cmd_sw2hw_cq(qp->snd_cq.cqn, inprm, SW2HW_CQ_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto exit;
	}

	/* create receive CQ */
	init_cq_buf(qp->rcv_cq.cq_buf, qp->rcv_cq.num_cqes);
	qp->rcv_cq.cons_counter = 0;
	memset(inprm, 0, SW2HW_CQ_IBUF_SZ);
	prep_sw2hw_cq_buf(inprm,
			  dev_ib_data.eq.eqn,
			  qp->rcv_cq.cqn,
			  qp->rcv_cq.cq_buf,
			  qp->rcv_cq.ci_db_ctx_idx, qp->rcv_cq.arm_db_ctx_idx);

	rc = cmd_sw2hw_cq(qp->rcv_cq.cqn, inprm, SW2HW_CQ_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_snd_cq;
	}

	prep_rst2init_qpee_buf(inprm,
			       qp->snd_cq.cqn,
			       qp->rcv_cq.cqn,
			       qp->qkey,
			       my_log2(qp->max_recv_wqes),
			       my_log2(sizeof(qp->rcv_wq[0])) - 4,
			       my_log2(qp->max_snd_wqes),
			       my_log2(sizeof(qp->snd_wq[0])) - 4,
			       virt_to_bus(qp->snd_wq),
			       qp->snd_db_record_index,
			       virt_to_bus(qp->rcv_wq),
			       qp->rcv_db_record_index);

	rc = cmd_rst2init_qpee(qp->qpn, inprm, QPCTX_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_rcv_cq;
	}

	qp->last_posted_rcv_wqe = NULL;
	qp->last_posted_snd_wqe = NULL;

	/* post all the buffers to the receive queue */
	while (1) {
		/* allocate wqe */
		rcv_wqe = alloc_rcv_wqe(qp);
		if (!rcv_wqe)
			break;

		/* post the buffer */
		rc = post_rcv_buf(qp, rcv_wqe);
		if (rc) {
			ret = -1;
			eprintf("");
			goto undo_rcv_cq;
		}
	}

	prep_init2rtr_qpee_buf(inprm);
	rc = cmd_init2rtr_qpee(qp->qpn, inprm, QPCTX_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_rcv_cq;
	}

	memset(inprm, 0, QPCTX_IBUF_SZ);
	rc = cmd_rtr2rts_qpee(qp->qpn, inprm, QPCTX_IBUF_SZ);
	if (rc) {
		ret = -1;
		eprintf("");
		goto undo_rcv_cq;
	}

	goto exit;

      undo_rcv_cq:
	rc = cmd_hw2sw_cq(qp->rcv_cq.cqn);
	if (rc)
		eprintf("");

      undo_snd_cq:
	rc = cmd_hw2sw_cq(qp->snd_cq.cqn);
	if (rc)
		eprintf("");

      exit:
	return ret;
}

static int destroy_udqp(struct udqp_st *qp)
{
	int rc;

	rc = cmd_2err_qpee(qp->qpn);
	if (rc) {
		eprintf("");
		return rc;
	}
	tprintf("cmd_2err_qpee(0x%lx) success", qp->qpn);

	rc = cmd_2rst_qpee(qp->qpn);
	if (rc) {
		eprintf("");
		return rc;
	}
	tprintf("cmd_2rst_qpee(0x%lx) success", qp->qpn);

	rc = cmd_hw2sw_cq(qp->rcv_cq.cqn);
	if (rc) {
		eprintf("");
		return rc;
	}
	tprintf("cmd_hw2sw_cq(0x%lx) success", qp->snd_cq.cqn);

	rc = cmd_hw2sw_cq(qp->snd_cq.cqn);
	if (rc) {
		eprintf("");
		return rc;
	}
	tprintf("cmd_hw2sw_cq(0x%lx) success", qp->rcv_cq.cqn);

	return rc;
}

static void prep_send_wqe_buf(void *qph,
			      void *avh,
			      void *wqeh,
			      const void *buf,
			      unsigned int offset, __u16 len, __u8 e)
{
	struct ud_send_wqe_st *snd_wqe = wqeh;
	struct ud_av_st *av = avh;

	if (qph) {
	}
	/* suppress warnings */
	INS_FLD_TO_BE(e, &snd_wqe->next.control,
		      arbelprm_wqe_segment_ctrl_send_st, e);
	INS_FLD_TO_BE(1, &snd_wqe->next.control,
		      arbelprm_wqe_segment_ctrl_send_st, always1);
	INS_FLD_TO_BE(1, &snd_wqe->next.next, arbelprm_wqe_segment_next_st,
		      always1);
	memcpy(&snd_wqe->udseg, &av->av, sizeof av->av);
	INS_FLD_TO_BE(av->dest_qp, snd_wqe->udseg.av,
		      arbelprm_wqe_segment_ud_st, destination_qp);
	INS_FLD_TO_BE(av->qkey, snd_wqe->udseg.av, arbelprm_wqe_segment_ud_st,
		      q_key);

	if (buf) {
		memcpy(bus_to_virt
		       (be32_to_cpu(snd_wqe->mpointer[0].local_addr_l)) +
		       offset, buf, len);
		len += offset;
	}
	snd_wqe->mpointer[0].byte_count = cpu_to_be32(len);
}

static void *alloc_ud_av(void)
{
	u8 next_free;

	if (dev_ib_data.udav.udav_next_free == FL_EOL) {
		return NULL;
	}

	next_free = dev_ib_data.udav.udav_next_free;
	dev_ib_data.udav.udav_next_free =
	    dev_buffers_p->av_array[next_free].ud_av.next_free;
	tprintf("allocated udav %d", next_free);
	return &dev_buffers_p->av_array[next_free].ud_av;
}

static void free_ud_av(void *avh)
{
	union ud_av_u *avu;
	__u8 idx, old_idx;
	struct ud_av_st *av = avh;

	avu = (union ud_av_u *)av;

	idx = avu - dev_buffers_p->av_array;
	tprintf("freeing udav idx=%d", idx);
	old_idx = dev_ib_data.udav.udav_next_free;
	dev_ib_data.udav.udav_next_free = idx;
	avu->ud_av.next_free = old_idx;
}

static int update_cq_cons_idx(struct cq_st *cq)
{
	/* write doorbell record */
	WRITE_DWORD_VOL(cq->ci_db_ctx_pointer, 0, htonl(cq->cons_counter));

	/*
	   INS_FLD_TO_BE(cq->cons_counter,
	   cq->ci_db_ctx_pointer,
	   arbelprm_cq_arm_db_record_st,
	   counter);

	   INS_FLD_TO_BE(cq->cqn,
	   cq->ci_db_ctx_pointer,
	   arbelprm_cq_arm_db_record_st,
	   cq_number);

	   INS_FLD_TO_BE(1,
	   cq->ci_db_ctx_pointer,
	   arbelprm_cq_arm_db_record_st,
	   res); */

	return 0;
}

static int poll_cq(void *cqh, union cqe_st *cqe_p, u8 * num_cqes)
{
	union cqe_st cqe;
	int rc;
	u32 *ptr;
	struct cq_st *cq = cqh;
	__u32 cons_idx = cq->cons_counter & (cq->num_cqes - 1);

	ptr = (u32 *) (&(cq->cq_buf[cons_idx]));
	barrier();
	if ((ptr[7] & 0x80000000) == 0) {
		cqe = cq->cq_buf[cons_idx];
		be_to_cpu_buf(&cqe, sizeof(cqe));
		*cqe_p = cqe;
		ptr[7] = 0x80000000;
		barrier();
		cq->cons_counter++;
		rc = update_cq_cons_idx(cq);
		if (rc) {
			return rc;
		}
		*num_cqes = 1;
	} else
		*num_cqes = 0;

	return 0;
}

static void dev2ib_cqe(struct ib_cqe_st *ib_cqe_p, union cqe_st *cqe_p)
{
	__u8 opcode;
	__u32 wqe_addr_ba;

	opcode =
	    EX_FLD(cqe_p->good_cqe, arbelprm_completion_queue_entry_st, opcode);
	if (opcode >= CQE_ERROR_OPCODE)
		ib_cqe_p->is_error = 1;
	else
		ib_cqe_p->is_error = 0;

	ib_cqe_p->is_send =
	    EX_FLD(cqe_p->good_cqe, arbelprm_completion_queue_entry_st, s);
	wqe_addr_ba =
	    EX_FLD(cqe_p->good_cqe, arbelprm_completion_queue_entry_st,
		   wqe_adr) << 6;
	ib_cqe_p->wqe = bus_to_virt(wqe_addr_ba);

	ib_cqe_p->count =
	    EX_FLD(cqe_p->good_cqe, arbelprm_completion_queue_entry_st,
		   byte_cnt);
}

static int ib_poll_cq(void *cqh, struct ib_cqe_st *ib_cqe_p, u8 * num_cqes)
{
	int rc;
	union cqe_st cqe;
	struct cq_st *cq = cqh;
	__u8 opcode;

	rc = poll_cq(cq, &cqe, num_cqes);
	if (rc || ((*num_cqes) == 0)) {
		return rc;
	}

	dev2ib_cqe(ib_cqe_p, &cqe);

	opcode =
	    EX_FLD(cqe.good_cqe, arbelprm_completion_queue_entry_st, opcode);
	if (opcode >= CQE_ERROR_OPCODE) {
		struct ud_send_wqe_st *wqe_p, wqe;
		__u32 *ptr;
		unsigned int i;

		wqe_p =
		    bus_to_virt(EX_FLD
				(cqe.error_cqe,
				 arbelprm_completion_with_error_st,
				 wqe_addr) << 6);
		eprintf("syndrome=0x%lx",
			EX_FLD(cqe.error_cqe, arbelprm_completion_with_error_st,
			       syndrome));
		eprintf("vendor_syndrome=0x%lx",
			EX_FLD(cqe.error_cqe, arbelprm_completion_with_error_st,
			       vendor_code));
		eprintf("wqe_addr=0x%lx", wqe_p);
		eprintf("myqpn=0x%lx",
			EX_FLD(cqe.error_cqe, arbelprm_completion_with_error_st,
			       myqpn));
		memcpy(&wqe, wqe_p, sizeof wqe);
		be_to_cpu_buf(&wqe, sizeof wqe);

		eprintf("dumping wqe...");
		ptr = (__u32 *) (&wqe);
		for (i = 0; i < sizeof wqe; i += 4) {
			printf("%lx : ", ptr[i >> 2]);
		}

	}

	return rc;
}

/* always work on ipoib qp */
static int add_qp_to_mcast_group(union ib_gid_u mcast_gid, __u8 add)
{
	void *mg;
	__u8 *tmp;
	int rc;
	__u16 mgid_hash;
	void *mgmqp_p;

	tmp = dev_buffers_p->inprm_buf;
	memcpy(tmp, mcast_gid.raw, 16);
	be_to_cpu_buf(tmp, 16);
	rc = cmd_mgid_hash(tmp, &mgid_hash);
	if (!rc) {
		mg = (void *)dev_buffers_p->inprm_buf;
		memset(mg, 0, MT_STRUCT_SIZE(arbelprm_mgm_entry_st));
		INS_FLD(mcast_gid.as_u32.dw[0], mg, arbelprm_mgm_entry_st,
			mgid_128_96);
		INS_FLD(mcast_gid.as_u32.dw[1], mg, arbelprm_mgm_entry_st,
			mgid_95_64);
		INS_FLD(mcast_gid.as_u32.dw[2], mg, arbelprm_mgm_entry_st,
			mgid_63_32);
		INS_FLD(mcast_gid.as_u32.dw[3], mg, arbelprm_mgm_entry_st,
			mgid_31_0);
		be_to_cpu_buf(mg +
			      MT_BYTE_OFFSET(arbelprm_mgm_entry_st,
					     mgid_128_96), 16);
		mgmqp_p = mg + MT_BYTE_OFFSET(arbelprm_mgm_entry_st, mgmqp_0);
		INS_FLD(dev_ib_data.ipoib_qp.qpn, mgmqp_p, arbelprm_mgmqp_st,
			qpn_i);
		INS_FLD(add, mgmqp_p, arbelprm_mgmqp_st, qi);
		rc = cmd_write_mgm(mg, mgid_hash);
	}
	return rc;
}

static int clear_interrupt(void)
{
	writel(dev_ib_data.clr_int_data, dev_ib_data.clr_int_addr);
	return 0;
}

static struct ud_send_wqe_st *alloc_send_wqe(udqp_t qph)
{
	struct udqp_st *qp = qph;
	__u32 idx;

	if (qp->snd_wqe_cur_free) {
		qp->snd_wqe_cur_free--;
		idx = qp->snd_wqe_alloc_idx;
		qp->snd_wqe_alloc_idx =
		    (qp->snd_wqe_alloc_idx + 1) & (qp->max_snd_wqes - 1);
		return &qp->snd_wq[idx].wqe_cont.wqe;
	}

	return NULL;
}

static struct recv_wqe_st *alloc_rcv_wqe(struct udqp_st *qp)
{
	__u32 idx;

	if (qp->recv_wqe_cur_free) {
		qp->recv_wqe_cur_free--;
		idx = qp->recv_wqe_alloc_idx;
		qp->recv_wqe_alloc_idx =
		    (qp->recv_wqe_alloc_idx + 1) & (qp->max_recv_wqes - 1);
		return &qp->rcv_wq[idx].wqe_cont.wqe;
	}

	return NULL;
}

static int free_send_wqe(struct ud_send_wqe_st *wqe)
{
	struct udqp_st *qp = ((struct ude_send_wqe_cont_st *)wqe)->qp;
	qp->snd_wqe_cur_free++;

	return 0;
}

static int free_rcv_wqe(struct recv_wqe_st *wqe)
{
	struct udqp_st *qp = ((struct recv_wqe_cont_st *)wqe)->qp;
	qp->recv_wqe_cur_free++;

	return 0;
}

static int free_wqe(void *wqe)
{
	int rc = 0;
	struct recv_wqe_st *rcv_wqe;

//      tprintf("free wqe= 0x%x", wqe);
	if ((wqe >= (void *)(dev_ib_data.ipoib_qp.rcv_wq)) &&
	    (wqe <
	     (void *)(&dev_ib_data.ipoib_qp.rcv_wq[NUM_IPOIB_RCV_WQES]))) {
		/* ipoib receive wqe */
		free_rcv_wqe(wqe);
		rcv_wqe = alloc_rcv_wqe(&dev_ib_data.ipoib_qp);
		if (rcv_wqe) {
			rc = post_rcv_buf(&dev_ib_data.ipoib_qp, rcv_wqe);
			if (rc) {
				eprintf("");
			}
		}
	} else if (wqe >= (void *)(dev_ib_data.ipoib_qp.snd_wq) &&
		   wqe <
		   (void *)(&dev_ib_data.ipoib_qp.snd_wq[NUM_IPOIB_SND_WQES])) {
		/* ipoib send wqe */
		free_send_wqe(wqe);
	} else if (wqe >= (void *)(dev_ib_data.mads_qp.rcv_wq) &&
		   wqe <
		   (void *)(&dev_ib_data.mads_qp.rcv_wq[NUM_MADS_RCV_WQES])) {
		/* mads receive wqe */
		free_rcv_wqe(wqe);
		rcv_wqe = alloc_rcv_wqe(&dev_ib_data.mads_qp);
		if (rcv_wqe) {
			rc = post_rcv_buf(&dev_ib_data.mads_qp, rcv_wqe);
			if (rc) {
				eprintf("");
			}
		}
	} else if (wqe >= (void *)(dev_ib_data.mads_qp.snd_wq) &&
		   wqe <
		   (void *)(&dev_ib_data.mads_qp.snd_wq[NUM_MADS_SND_WQES])) {
		/* mads send wqe */
		free_send_wqe(wqe);
	} else {
		rc = -1;
		eprintf("");
	}

	return rc;
}

static int update_eq_cons_idx(struct eq_st *eq)
{
	writel(eq->cons_counter, eq->ci_base_base_addr);
	return 0;
}

static void dev2ib_eqe(struct ib_eqe_st *ib_eqe_p, struct eqe_t *eqe_p)
{
	void *tmp;

	ib_eqe_p->event_type =
	    EX_FLD(eqe_p, arbelprm_event_queue_entry_st, event_type);

	tmp = eqe_p + MT_BYTE_OFFSET(arbelprm_event_queue_entry_st, event_data);
	ib_eqe_p->cqn = EX_FLD(tmp, arbelprm_completion_event_st, cqn);
}

static int poll_eq(struct ib_eqe_st *ib_eqe_p, __u8 * num_eqes)
{
	struct eqe_t eqe;
	u8 owner;
	int rc;
	u32 *ptr;
	struct eq_st *eq = &dev_ib_data.eq;
	__u32 cons_idx = eq->cons_counter & (eq->eq_size - 1);

	ptr = (u32 *) (&(eq->eq_buf[cons_idx]));
	owner = (ptr[7] & 0x80000000) ? OWNER_HW : OWNER_SW;
	if (owner == OWNER_SW) {
		eqe = eq->eq_buf[cons_idx];
		be_to_cpu_buf(&eqe, sizeof(eqe));
		dev2ib_eqe(ib_eqe_p, &eqe);
		ptr[7] |= 0x80000000;
		eq->eq_buf[cons_idx] = eqe;
		eq->cons_counter++;
		rc = update_eq_cons_idx(eq);
		if (rc) {
			return -1;
		}
		*num_eqes = 1;
	} else {
		*num_eqes = 0;
	}
	return 0;
}

static int ib_device_close(void)
{
	iounmap(memfree_pci_dev.uar);
	iounmap(memfree_pci_dev.cr_space);
	return 0;
}

static __u32 dev_get_qpn(void *qph)
{
	struct udqp_st *qp = qph;

	return qp->qpn;
}

static void dev_post_dbell(void *dbell, __u32 offset)
{
	__u32 *ptr;
	unsigned long address;

	ptr = dbell;

	if (((ptr[0] >> 24) & 0xff) != 1) {
		eprintf("");
	}
	tprintf("ptr[0]= 0x%lx", ptr[0]);
	tprintf("ptr[1]= 0x%lx", ptr[1]);
	address = (unsigned long)(memfree_pci_dev.uar) + offset;
	tprintf("va=0x%lx pa=0x%lx", address,
		virt_to_bus((const void *)address));
	writel(htonl(ptr[0]), memfree_pci_dev.uar + offset);
	barrier();
	address += 4;
	tprintf("va=0x%lx pa=0x%lx", address,
		virt_to_bus((const void *)address));
	writel(htonl(ptr[1]), address /*memfree_pci_dev.uar + offset + 4 */ );
}
