#ifndef	_TCP_H
#define	_TCP_H

#define TCP_INITIAL_TIMEOUT     (3*TICKS_PER_SEC)
#define TCP_MAX_TIMEOUT         (60*TICKS_PER_SEC)
#define TCP_MIN_TIMEOUT         (TICKS_PER_SEC)
#define TCP_MAX_RETRY           10
#define TCP_MAX_HEADER          ((int)sizeof(struct iphdr)+64)
#define TCP_MIN_WINDOW          (1500-TCP_MAX_HEADER)
#define TCP_MAX_WINDOW          (65535-TCP_MAX_HEADER)


#define MAX_URL                 80


#define FIN             1
#define SYN             2
#define RST             4
#define PSH             8
#define ACK             16
#define URG             32


struct tcphdr {
       uint16_t src;
       uint16_t dst;
       int32_t  seq;
       int32_t  ack;
       uint16_t ctrl;
       uint16_t window;
       uint16_t chksum;
       uint16_t urgent;
};

#endif	/* _TCP_H */
