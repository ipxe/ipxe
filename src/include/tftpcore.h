#ifndef TFTPCORE_H
#define TFTPCORE_H

#include "tftp.h"

extern int await_tftp ( int ival, void *ptr, unsigned short ptype,
			struct iphdr *ip, struct udphdr *udp,
			struct tcphdr *tcp );

extern int tftp_open ( struct tftp_state *state, const char *filename,
		       union tftp_any **reply );

extern int tftp_process_opts ( struct tftp_state *state,
			       struct tftp_oack *oack );

extern int tftp_ack_nowait ( struct tftp_state *state );

extern int tftp_ack ( struct tftp_state *state, union tftp_any **reply );

extern int tftp_error ( struct tftp_state *state, int errcode,
			const char *errmsg );

extern void tftp_set_errno ( struct tftp_error *error );

#endif /* TFTPCORE_H */
