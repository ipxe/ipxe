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
#include "mt25218.h"

/*
 *  cmd_write_mgm
 */
static int cmd_write_mgm(void *mg, __u16 index)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = MEMFREE_CMD_WRITE_MGM;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param_size = MT_STRUCT_SIZE(arbelprm_mgm_entry_st);
	cmd_desc.in_param = (__u32 *) mg;
	cmd_desc.input_modifier = index;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_mod_stat_cfg
 */
static int cmd_mod_stat_cfg(void)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);
	cmd_desc.opcode = MEMFREE_CMD_MOD_STAT_CFG;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param_size = MT_STRUCT_SIZE(arbelprm_mod_stat_cfg_st);
	cmd_desc.in_param = get_inprm_buf();
	memset(cmd_desc.in_param, 0, cmd_desc.in_param_size);

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

	cmd_desc.opcode = MEMFREE_CMD_QUERY_FW;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = get_outprm_buf();
	cmd_desc.out_param_size = MT_STRUCT_SIZE(arbelprm_query_fw_st);

	rc = cmd_invoke(&cmd_desc);
	if (!rc) {
		qfw->fw_rev_major =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st, fw_rev_major);
		qfw->fw_rev_minor =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st, fw_rev_minor);
		qfw->fw_rev_subminor =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st, fw_rev_subminor);

		qfw->error_buf_start_h =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st, error_buf_start_h);
		qfw->error_buf_start_l =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st, error_buf_start_l);
		qfw->error_buf_size =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st, error_buf_size);

		qfw->fw_pages =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st, fw_pages);
		qfw->eq_ci_table.addr_h =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st,
			   eq_set_ci_base_addr_h);
		qfw->eq_ci_table.addr_l =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st,
			   eq_set_ci_base_addr_l);
		qfw->clear_int_addr.addr_h =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st,
			   clr_int_base_addr_h);
		qfw->clear_int_addr.addr_l =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_fw_st,
			   clr_int_base_addr_l);
	}

	return rc;
}

/*
 *  cmd_query_adapter
 */
static int cmd_query_adapter(struct query_adapter_st *qa)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_QUERY_ADAPTER;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = get_outprm_buf();
	cmd_desc.out_param_size = MT_STRUCT_SIZE(arbelprm_query_adapter_st);

	rc = cmd_invoke(&cmd_desc);
	if (!rc) {
		qa->intapin =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_adapter_st,
			   intapin);
	}

	return rc;
}

/*
 *  cmd_enable_lam
 */
static int cmd_enable_lam(void)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_ENABLE_LAM;
	cmd_desc.opcode_modifier = 1;	/* zero locally attached memory */
	cmd_desc.input_modifier = 0;	/* disable fast refresh */
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = get_outprm_buf();
	cmd_desc.out_param_size = MT_STRUCT_SIZE(arbelprm_enable_lam_st);

	rc = cmd_invoke(&cmd_desc);
	if (rc) {
	}

	return rc;
}

/*
 *  cmd_map_fa
 */
static int cmd_map_fa(struct map_icm_st *map_fa_p)
{
	int rc;
	command_fields_t cmd_desc;
	unsigned int in_param_size, i;
	unsigned long off;

	if (map_fa_p->num_vpm > MAX_VPM_PER_CALL) {
		return -1;
	}

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_MAP_FA;
	cmd_desc.input_modifier = map_fa_p->num_vpm;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = get_inprm_buf();
	in_param_size =
	    MT_STRUCT_SIZE(arbelprm_virtual_physical_mapping_st) *
	    map_fa_p->num_vpm;
	cmd_desc.in_param_size = in_param_size;
	memset(cmd_desc.in_param, 0, in_param_size);

	for (i = 0; i < map_fa_p->num_vpm; ++i) {
		off = (unsigned long)(cmd_desc.in_param) +
		    MT_STRUCT_SIZE(arbelprm_virtual_physical_mapping_st) * i;
		INS_FLD(map_fa_p->vpm_arr[i].va_h, off,
			arbelprm_virtual_physical_mapping_st, va_h);
		INS_FLD(map_fa_p->vpm_arr[i].va_l >> 12, off,
			arbelprm_virtual_physical_mapping_st, va_l);
		INS_FLD(map_fa_p->vpm_arr[i].pa_h, off,
			arbelprm_virtual_physical_mapping_st, pa_h);
		INS_FLD(map_fa_p->vpm_arr[i].pa_l >> 12, off,
			arbelprm_virtual_physical_mapping_st, pa_l);
		INS_FLD(map_fa_p->vpm_arr[i].log2_size, off,
			arbelprm_virtual_physical_mapping_st, log2size);
	}

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_unmap_fa
 */
static int cmd_unmap_fa(void)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_UNMAP_FA;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_run_fw
 */
static int cmd_run_fw(void)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_RUN_FW;
	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_set_icm_size
 */
static int cmd_set_icm_size(__u32 icm_size, __u32 * aux_pages_p)
{
	int rc;
	command_fields_t cmd_desc;
	__u32 iprm[2], oprm[2];

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_SET_ICM_SIZE;

	iprm[1] = icm_size;
	iprm[0] = 0;
	cmd_desc.in_trans = TRANS_IMMEDIATE;
	cmd_desc.in_param = iprm;
	cmd_desc.out_trans = TRANS_IMMEDIATE;
	cmd_desc.out_param = oprm;
	rc = cmd_invoke(&cmd_desc);
	if (!rc) {
		if (oprm[0]) {
			/* too many pages required */
			return -1;
		}
		*aux_pages_p = oprm[1];
	}

	return rc;
}

/*
 *  cmd_map_icm_aux
 */
static int cmd_map_icm_aux(struct map_icm_st *map_icm_aux_p)
{
	int rc;
	command_fields_t cmd_desc;
	unsigned int in_param_size, i;
	unsigned long off;

	if (map_icm_aux_p->num_vpm > MAX_VPM_PER_CALL) {
		return -1;
	}

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_MAP_ICM_AUX;
	cmd_desc.input_modifier = map_icm_aux_p->num_vpm;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = get_inprm_buf();
	in_param_size =
	    MT_STRUCT_SIZE(arbelprm_virtual_physical_mapping_st) *
	    map_icm_aux_p->num_vpm;
	cmd_desc.in_param_size = in_param_size;
	memset(cmd_desc.in_param, 0, in_param_size);

	for (i = 0; i < map_icm_aux_p->num_vpm; ++i) {
		off = (unsigned long)(cmd_desc.in_param) +
		    MT_STRUCT_SIZE(arbelprm_virtual_physical_mapping_st) * i;
		INS_FLD(map_icm_aux_p->vpm_arr[i].va_h, off,
			arbelprm_virtual_physical_mapping_st, va_h);
		INS_FLD(map_icm_aux_p->vpm_arr[i].va_l >> 12, off,
			arbelprm_virtual_physical_mapping_st, va_l);
		INS_FLD(map_icm_aux_p->vpm_arr[i].pa_h, off,
			arbelprm_virtual_physical_mapping_st, pa_h);
		INS_FLD(map_icm_aux_p->vpm_arr[i].pa_l >> 12, off,
			arbelprm_virtual_physical_mapping_st, pa_l);
		INS_FLD(map_icm_aux_p->vpm_arr[i].log2_size, off,
			arbelprm_virtual_physical_mapping_st, log2size);
	}

	rc = cmd_invoke(&cmd_desc);

	return rc;
}


/*
 *  cmd_unmap_icm_aux
 */
static int cmd_unmap_icm_aux(void)
{
	int rc;
	command_fields_t cmd_desc;

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_UNMAP_ICM_AUX;

	rc = cmd_invoke(&cmd_desc);

	return rc;
}

/*
 *  cmd_map_icm
 */
static int cmd_map_icm(struct map_icm_st *map_icm_p)
{
	int rc;
	command_fields_t cmd_desc;
	unsigned int in_param_size, i;
	unsigned long off;

	if (map_icm_p->num_vpm > MAX_VPM_PER_CALL) {
		return -1;
	}

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_MAP_ICM;
	cmd_desc.input_modifier = map_icm_p->num_vpm;
	cmd_desc.in_trans = TRANS_MAILBOX;
	cmd_desc.in_param = get_inprm_buf();
	in_param_size =
	    MT_STRUCT_SIZE(arbelprm_virtual_physical_mapping_st) *
	    map_icm_p->num_vpm;
	cmd_desc.in_param_size = in_param_size;
	memset(cmd_desc.in_param, 0, in_param_size);

	for (i = 0; i < map_icm_p->num_vpm; ++i) {
		off = (unsigned long)(cmd_desc.in_param) +
		    MT_STRUCT_SIZE(arbelprm_virtual_physical_mapping_st) * i;
		INS_FLD(map_icm_p->vpm_arr[i].va_h, off,
			arbelprm_virtual_physical_mapping_st, va_h);
		INS_FLD(map_icm_p->vpm_arr[i].va_l >> 12, off,
			arbelprm_virtual_physical_mapping_st, va_l);
		INS_FLD(map_icm_p->vpm_arr[i].pa_h, off,
			arbelprm_virtual_physical_mapping_st, pa_h);
		INS_FLD(map_icm_p->vpm_arr[i].pa_l >> 12, off,
			arbelprm_virtual_physical_mapping_st, pa_l);
		INS_FLD(map_icm_p->vpm_arr[i].log2_size, off,
			arbelprm_virtual_physical_mapping_st, log2size);
	}

	rc = cmd_invoke(&cmd_desc);

	return rc;
}



/*
 *  cmd_unmap_icm
 */
static int cmd_unmap_icm(struct map_icm_st *map_icm_p)
{
	int rc;
	command_fields_t cmd_desc;
	__u32 iprm[2];

	memset(&cmd_desc, 0, sizeof cmd_desc);

	cmd_desc.opcode = MEMFREE_CMD_UNMAP_ICM;
	iprm[0] = map_icm_p->vpm_arr[0].va_h;
	iprm[1] = map_icm_p->vpm_arr[0].va_l;
	cmd_desc.in_param = iprm;
	cmd_desc.in_trans = TRANS_IMMEDIATE;
	cmd_desc.input_modifier = 1 << map_icm_p->vpm_arr[0].log2_size;

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

	cmd_desc.opcode = MEMFREE_CMD_QUERY_DEV_LIM;
	cmd_desc.out_trans = TRANS_MAILBOX;
	cmd_desc.out_param = get_outprm_buf();
	cmd_desc.out_param_size = MT_STRUCT_SIZE(arbelprm_query_dev_lim_st);

	rc = cmd_invoke(&cmd_desc);
	if (!rc) {
		dev_lim_p->log2_rsvd_qps =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   log2_rsvd_qps);
		dev_lim_p->qpc_entry_sz =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   qpc_entry_sz);

		dev_lim_p->log2_rsvd_srqs =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   log2_rsvd_srqs);
		dev_lim_p->srq_entry_sz =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   srq_entry_sz);

		dev_lim_p->log2_rsvd_ees =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   log2_rsvd_ees);
		dev_lim_p->eec_entry_sz =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   eec_entry_sz);

		dev_lim_p->log2_rsvd_cqs =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   log2_rsvd_cqs);
		dev_lim_p->cqc_entry_sz =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   cqc_entry_sz);

		dev_lim_p->log2_rsvd_mtts =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   log2_rsvd_mtts);
		dev_lim_p->mtt_entry_sz =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   mtt_entry_sz);

		dev_lim_p->log2_rsvd_mrws =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   log2_rsvd_mrws);
		dev_lim_p->mpt_entry_sz =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   mpt_entry_sz);

		dev_lim_p->log2_rsvd_rdbs =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   log2_rsvd_rdbs);

		dev_lim_p->eqc_entry_sz =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   eqc_entry_sz);

		dev_lim_p->max_icm_size_l =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   max_icm_size_l);
		dev_lim_p->max_icm_size_h =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   max_icm_size_h);

		dev_lim_p->num_rsvd_uars =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   num_rsvd_uars);
		dev_lim_p->uar_sz =
		    EX_FLD(cmd_desc.out_param, arbelprm_query_dev_lim_st,
			   uar_sz);
	}

	return rc;
}
