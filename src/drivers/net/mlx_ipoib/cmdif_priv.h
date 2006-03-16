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

#ifndef __cmdif_priv__h__
#define __cmdif_priv__h__

typedef enum {
	TRANS_NA,
	TRANS_IMMEDIATE,
	TRANS_MAILBOX
} trans_type_t;

typedef struct {
	__u32 *in_param;	/* holds the virtually contigious buffer of the parameter block passed */
	unsigned int in_param_size;
	trans_type_t in_trans;

	__u32 input_modifier;

	__u32 *out_param;	/* holds the virtually contigious buffer of the parameter block passed */
	unsigned int out_param_size;
	trans_type_t out_trans;

	__u32 opcode;
	__u8 opcode_modifier;
} command_fields_t;

typedef int XHH_cmd_status_t;

static XHH_cmd_status_t cmd_invoke(command_fields_t * cmd_prms);

#endif
