#ifndef	TFTP_H
#define	TFTP_H

#include "in.h"
#include "nic.h"

#define TFTP_PORT	69
#define	TFTP_DEFAULTSIZE_PACKET	512
#define	TFTP_MAX_PACKET		1432 /* 512 */

#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERROR	5
#define TFTP_OACK	6

#define TFTP_CODE_EOF	1
#define TFTP_CODE_MORE	2
#define TFTP_CODE_ERROR	3
#define TFTP_CODE_BOOT	4
#define TFTP_CODE_CFG	5

struct tftp_t {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
	union {
		uint8_t rrq[TFTP_DEFAULTSIZE_PACKET];
		struct {
			uint16_t block;
			uint8_t  download[TFTP_MAX_PACKET];
		} data;
		struct {
			uint16_t block;
		} ack;
		struct {
			uint16_t errcode;
			uint8_t  errmsg[TFTP_DEFAULTSIZE_PACKET];
		} err;
		struct {
			uint8_t  data[TFTP_DEFAULTSIZE_PACKET+2];
		} oack;
	} u;
} PACKED;

/* define a smaller tftp packet solely for making requests to conserve stack
   512 bytes should be enough */
struct tftpreq_t {
	struct iphdr ip;
	struct udphdr udp;
	uint16_t opcode;
	union {
		uint8_t rrq[512];
		struct {
			uint16_t block;
		} ack;
		struct {
			uint16_t errcode;
			uint8_t  errmsg[512-2];
		} err;
	} u;
} PACKED;

struct tftpreq_info_t {
	struct sockaddr_in *server;
	const char *name;
	unsigned short blksize;
} PACKED;

struct tftpblk_info_t {
	char *data;
	unsigned int block;
	unsigned int len;
	int eof;
} PACKED;

#define TFTP_MIN_PACKET	(sizeof(struct iphdr) + sizeof(struct udphdr) + 4)

/*
 * Functions in tftp.c.  Needed for pxe_export.c
 *
 */
extern int tftp_block ( struct tftpreq_info_t *request,
			struct tftpblk_info_t *block );
extern int tftp ( char *url,
		  struct sockaddr_in *server,
		  char *file,
		  int ( * process ) ( unsigned char *data,
				      unsigned int blocknum,
				      unsigned int len, int eof ) );

#endif	/* TFTP_H */
