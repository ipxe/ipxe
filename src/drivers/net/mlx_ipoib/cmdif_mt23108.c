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
#include "cmdif.h"
#include "cmdif_priv.h"
#include "mt23108.h"

/*
 *  cmd_sys_en
 */
static int cmd_sys_en(void)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = TAVOR_CMD_SYS_EN;
	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_sys_dis
 */
static int cmd_sys_dis(void)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.in_trans = TRANS_NA;
	cmd_desc.out_trans = TRANS_NA;
	cmd_desc.opcode = TAVOR_CMD_SYS_DIS;
	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_query_dev_lim
 */
static int cmd_query_dev_lim(struct dev_lim_st *dev_lim_p)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = TAVOR_CMD_QUERY_DEV_LIM;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = get_outprm_buf();
	cmd_desc.out_param_size = MT_STRUCT_SIZE(tavorprm_query_dev_lim_st);

	rc = cmd_invoke(&cmd_desc);
	if (!rc) {
		dev_lim_p->log2_rsvd_qps =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   log2_rsvd_qps);
		dev_lim_p->qpc_entry_sz =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   qpc_entry_sz);

		dev_lim_p->log2_rsvd_srqs =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   log2_rsvd_srqs);
		dev_lim_p->srq_entry_sz =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   srq_entry_sz);

		dev_lim_p->log2_rsvd_ees =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   log2_rsvd_ees);
		dev_lim_p->eec_entry_sz =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   eec_entry_sz);

		dev_lim_p->log2_rsvd_cqs =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   log2_rsvd_cqs);
		dev_lim_p->cqc_entry_sz =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   cqc_entry_sz);

		dev_lim_p->log2_rsvd_mtts =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   log2_rsvd_mtts);
		dev_lim_p->mtt_entry_sz = 64;	/* segment size is set to zero in init_hca */

		dev_lim_p->log2_rsvd_mrws =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   log2_rsvd_mrws);
		dev_lim_p->mpt_entry_sz = MT_STRUCT_SIZE(tavorprm_mpt_st);

		dev_lim_p->eqc_entry_sz =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_dev_lim_st,
			   eqc_entry_sz);
	}

	return rc;
}

/*
 *  cmd_write_mgm
 */
static int cmd_write_mgm(void *mg, __u16 index)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = TAVOR_CMD_WRITE_MGM;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param_size = MT_STRUCT_SIZE(tavorprm_mgm_entry_st);
	cmd_desc.in_param = (__u32 *) mg;
	cmd_desc.input_modifier = index;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_mod_stat_cfg
 */
static int cmd_mod_stat_cfg(void *cfg)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = TAVOR_CMD_MOD_STAT_CFG;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param_size = MT_STRUCT_SIZE(tavorprm_mod_stat_cfg_st);
	cmd_desc.in_param = (__u32 *) cfg;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}


/*
 *  cmd_query_fw
 */
static int cmd_query_fw(struct query_fw_st *qfw)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = TAVOR_CMD_QUERY_FW;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = get_outprm_buf();
	cmd_desc.out_param_size = MT_STRUCT_SIZE(tavorprm_query_fw_st);

	rc = cmd_invoke(&cmd_desc);
	if (!rc) {
		qfw->fw_rev_major =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_fw_st, fw_rev_major);
		qfw->fw_rev_minor =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_fw_st, fw_rev_minor);
		qfw->fw_rev_subminor =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_fw_st, fw_rev_subminor);

		qfw->error_buf_start_h =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_fw_st, error_buf_start_h);
		qfw->error_buf_start_l =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_fw_st, error_buf_start_l);
		qfw->error_buf_size =
		    EX_FLD(cmd_desc.out_param, tavorprm_query_fw_st, error_buf_size);
	}

	return rc;
}
