#include "etherboot.h"
#include "ip.h"
#include "tcp.h"


void build_tcp_hdr(unsigned long destip, unsigned int srcsock,
		  unsigned int destsock, long send_seq, long recv_seq,
		  int window, int flags, int ttl, int len, const void *buf)
{
       struct iphdr *ip;
       struct tcphdr *tcp;
       ip = (struct iphdr *)buf;
       build_ip_hdr(destip, ttl, IP_TCP, 0, len, buf);
       tcp = (struct tcphdr *)(ip + 1);
       tcp->src = htons(srcsock);
       tcp->dst = htons(destsock);
       tcp->seq = htonl(send_seq);
       tcp->ack = htonl(recv_seq);
       tcp->ctrl = htons(flags + (5 << 12)); /* No TCP options */
       tcp->window = htons(window);
       tcp->chksum = 0;
       if ((tcp->chksum = tcpudpchksum(ip)) == 0)
	       tcp->chksum = 0xffff;
}

/**************************************************************************
TCP_TRANSMIT - Send a TCP packet
**************************************************************************/
int tcp_transmit(unsigned long destip, unsigned int srcsock,
		unsigned int destsock, long send_seq, long recv_seq,
		int window, int flags, int len, const void *buf)
{
       build_tcp_hdr(destip, srcsock, destsock, send_seq, recv_seq,
		     window, flags, 60, len, buf);
       return ip_transmit(len, buf);
}

int tcp_reset(struct iphdr *ip) {
       struct tcphdr *tcp = (struct tcphdr *)(ip + 1);
       char buf[sizeof(struct iphdr) + sizeof(struct tcphdr)];

       if (!(tcp->ctrl & htons(RST))) {
	      long seq = ntohl(tcp->seq) + ntohs(ip->len) -
			 sizeof(struct iphdr) -
			 ((ntohs(tcp->ctrl) >> 10) & 0x3C);
	      if (tcp->ctrl & htons(SYN|FIN))
		      seq++;
	      return tcp_transmit(ntohl(ip->src.s_addr),
				  ntohs(tcp->dst), ntohs(tcp->src),
				  tcp->ctrl&htons(ACK) ? ntohl(tcp->ack) : 0,
				  seq, TCP_MAX_WINDOW, RST, sizeof(buf), buf);
       }
       return (1);
}

/**************************************************************************
TCP - Simple-minded TCP stack. Can only send data once and then
      receive the response. The algorithm for computing window
      sizes and delaying ack's is currently broken, and thus
      disabled. Performance would probably improve a little, if
      this gets fixed. FIXME
**************************************************************************/
static int await_tcp(int ival, void *ptr, unsigned short ptype __unused,
		    struct iphdr *ip, struct udphdr *udp __unused,
		    struct tcphdr *tcp)
{
       if (!tcp) {
	       return 0;
       }
       if (arptable[ARP_CLIENT].ipaddr.s_addr != ip->dest.s_addr)
	       return 0;
       if (ntohs(tcp->dst) != ival) {
	       tcp_reset(ip);
	       return 0;
       }
       *(void **)ptr = tcp;
       return 1;
}

int tcp_transaction(unsigned long destip, unsigned int destsock, void *ptr,
		   int (*send)(int len, void *buf, void *ptr),
		   int (*recv)(int len, const void *buf, void *ptr)) {
       static uint16_t srcsock = 0;
       int	       rc = 1;
       long	       send_seq = currticks();
       long	       recv_seq = 0;
       int	       can_send = 0;
       int	       sent_all = 0;
       struct iphdr   *ip;
       struct tcphdr  *tcp;
       int	       ctrl = SYN;
       char	       buf[128]; /* Small outgoing buffer */
       long	       payload;
       int	       header_size;
       int	       window = 3*TCP_MIN_WINDOW;
       long	       last_ack = 0;
       long	       last_sent = 0;
       long	       rtt = 0;
       long	       srtt = 0;
       long	       rto = TCP_INITIAL_TIMEOUT;
       int	       retry = TCP_MAX_TIMEOUT/TCP_INITIAL_TIMEOUT;
       enum { CLOSED, SYN_RCVD, ESTABLISHED,
	      FIN_WAIT_1, FIN_WAIT_2 } state = CLOSED;

       if (!srcsock) {
	       srcsock = currticks();
       }
       if (++srcsock < 1024)
	       srcsock += 1024;

       rx_qdrain();

 send_data:
       if (ctrl & ACK)
	       last_ack = recv_seq;
       if (!tcp_transmit(destip, srcsock, destsock, send_seq,
			 recv_seq, window, ctrl,
			 sizeof(struct iphdr) + sizeof(struct tcphdr)+
			 can_send, buf)) {
	       return (0);
       }
       last_sent = currticks();

 recv_data:
       if (!await_reply(await_tcp, srcsock, &tcp,
			(state == ESTABLISHED && !can_send)
			? TCP_MAX_TIMEOUT : rto)) {
	       if (state == ESTABLISHED) {
 close:
		       ctrl = FIN|ACK;
		       state = FIN_WAIT_1;
		       rc = 0;
		       goto send_data;
	       }

	       if (state == FIN_WAIT_1 || state == FIN_WAIT_2)
		       return (rc);

	       if (--retry <= 0) {
		       /* time out */
		       if (state == SYN_RCVD) {
			       tcp_transmit(destip, srcsock, destsock,
					    send_seq, 0, window, RST,
					    sizeof(struct iphdr) +
					    sizeof(struct tcphdr), buf);
		       }
		       return (0);
	       }
	       /* retransmit */
	       goto send_data;
       }
       /* got_data: */
       retry = TCP_MAX_RETRY;

       if (tcp->ctrl & htons(ACK) ) {
	       char *data;
	       int syn_ack, consumed;

	       if (state == FIN_WAIT_1 || state == FIN_WAIT_2) {
		       state = FIN_WAIT_2;
		       ctrl = ACK;
		       goto consume_data;
	       }
	       syn_ack = state == CLOSED || state == SYN_RCVD;
	       consumed = ntohl(tcp->ack) - send_seq - syn_ack;
	       if (consumed < 0 || consumed > can_send) {
		       tcp_reset((struct iphdr *)&nic.packet[ETH_HLEN]);
		       goto recv_data;
	       }

	       rtt = currticks() - last_sent;
	       srtt = !srtt ? rtt : (srtt*4 + rtt)/5;
	       rto = srtt + srtt/2;
	       if (rto < TCP_MIN_TIMEOUT)
		       rto = TCP_MIN_TIMEOUT;
	       else if (rto > TCP_MAX_TIMEOUT)
		       rto = TCP_MAX_TIMEOUT;

	       can_send -= consumed;
	       send_seq += consumed + syn_ack;
	       data = buf + sizeof(struct iphdr) + sizeof(struct tcphdr);
	       if (can_send) {
		       memmove(data, data + consumed, can_send);
	       }
	       if (!sent_all) {
		       int more_data;
		       data += can_send;
		       more_data = buf + sizeof(buf) - data;
		       if (more_data > 0) {
			       more_data = send(more_data, data, ptr);
			       can_send += more_data;
		       }
		       sent_all = !more_data;
	       }
	       if (state == SYN_RCVD) {
		       state = ESTABLISHED;
		       ctrl = PSH|ACK;
		       goto consume_data;
	       }
	       if (tcp->ctrl & htons(RST))
		       return (0);
       } else if (tcp->ctrl & htons(RST)) {
	       if (state == CLOSED)
		       goto recv_data;
	       return (0);
       }

 consume_data:
       ip  = (struct iphdr *)&nic.packet[ETH_HLEN];
       header_size = sizeof(struct iphdr) + ((ntohs(tcp->ctrl)>>10)&0x3C);
       payload = ntohs(ip->len) - header_size;
       if (payload > 0 && state == ESTABLISHED) {
	       int old_bytes = recv_seq - (long)ntohl(tcp->seq);
	       if (old_bytes >= 0 && payload - old_bytes > 0) {
		       recv_seq += payload - old_bytes;
		       if (state != FIN_WAIT_1 && state != FIN_WAIT_2 &&
			   !recv(payload - old_bytes,
				 &nic.packet[ETH_HLEN+header_size+old_bytes],
				 ptr)) {
			       goto close;
		       }
		       if ((state == ESTABLISHED || state == SYN_RCVD) &&
			   !(tcp->ctrl & htons(FIN))) {
			       int in_window = window - 2*TCP_MIN_WINDOW >
						recv_seq - last_ack;
			       ctrl = can_send ? PSH|ACK : ACK;
			       if (!can_send && in_window) {
/* Window scaling is broken right now, just fall back to acknowledging every */
/* packet immediately and unconditionally. FIXME		       */ /***/
/*				       if (await_reply(await_tcp, srcsock,
						       &tcp, rto))
					       goto got_data;
				       else */
					       goto send_data;
			       }
			       if (!in_window) {
				       window += TCP_MIN_WINDOW;
				       if (window > TCP_MAX_WINDOW)
					       window = TCP_MAX_WINDOW;
			       }
			       goto send_data;
		       }
	       } else {
		       /* saw old data again, must have lost packets */
		       window /= 2;
		       if (window < 2*TCP_MIN_WINDOW)
			       window = 2*TCP_MIN_WINDOW;
	       }
       }

       if (tcp->ctrl & htons(FIN)) {
	       if (state == ESTABLISHED) {
		       ctrl = FIN|ACK;
	       } else if (state == FIN_WAIT_1 || state == FIN_WAIT_2) {
		       ctrl = ACK;
	       } else {
		       ctrl = RST;
	       }
	       return (tcp_transmit(destip, srcsock, destsock,
				    send_seq, recv_seq + 1, window, ctrl,
				    sizeof(struct iphdr) +
				    sizeof(struct tcphdr), buf) &&
		       (state == ESTABLISHED ||
			state == FIN_WAIT_1 || state == FIN_WAIT_2) &&
		       !can_send);
       }

       if (state == CLOSED) {
	       if (tcp->ctrl & htons(SYN)) {
		       recv_seq = ntohl(tcp->seq) + 1;
		       if (!(tcp->ctrl & htons(ACK))) {
			       state = SYN_RCVD;
			       ctrl = SYN|ACK|PSH;
			       goto send_data;
		       } else {
			       state = ESTABLISHED;
			       ctrl = PSH|ACK;
		       }
	       }
       }

       if (can_send || payload) {
	       goto send_data;
       }
       goto recv_data;
}
