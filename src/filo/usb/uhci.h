#ifndef _UHCI_H
#define _UHCI_H

/*
 * The link pointer is multi use. Some fields are valid only for some uses. 
 * In other cases, they must be 0
 *
 */

#define MAX_POLLDEV 10

#define MAX_TRANSACTIONS 10
#define MAX_QUEUEHEAD 255
#define MAX_TD 1024


typedef struct link_pointer {
        unsigned long terminate:1;
        unsigned long queue:1;
        unsigned long depth:1;
        unsigned long reserved:1;
        unsigned long link:28;
} __attribute__((packed)) link_pointer_t;

extern link_pointer_t *frame_list[];

void init_framelist(uchar dev);


#define SETUP_TOKEN 0x2d
#define IN_TOKEN 0x69
#define OUT_TOKEN 0xe1

#define CTRL_RETRIES 3
#define CONTROL_STS_RETRIES 0


// some port features
#define PORT_CONNECTION 0
#define PORT_ENABLE 1
#define PORT_SUSPEND 2
#define PORT_OVER_CURRENT 3
#define PORT_RESET 4
#define PORT_POWER 8
#define PORT_LOW_SPEED 9
#define C_PORT_CONNECTION 16
#define C_PORT_ENABLE 17
#define C_PORT_SUSPEND 18
#define C_PORT_OVER_CURRENT 19
#define C_PORT_RESET 20

// features
#define FEATURE_HALT 0

typedef struct td {
	
	link_pointer_t link;

	unsigned long actual:11;	// actual length
	unsigned long reserved2:5;

// status/error flags
	unsigned long res1:1;
	unsigned long bitstuff:1;
	unsigned long crc:1;
	unsigned long nak:1;
	unsigned long babble:1;
	unsigned long buffer_error:1;
	unsigned long stall:1;
	unsigned long active:1;

	unsigned long interrupt:1;	// interrupt on complete
	unsigned long isochronous:1;
	unsigned long lowspeed:1;
	unsigned long retrys:2;
	unsigned long detect_short:1;
	unsigned long reserved3:2;

	unsigned long packet_type:8;	// one of in (0x69), out (0xe1) or setup (0x2d)
	unsigned long device_addr:7;
	unsigned long endpoint:4;
	unsigned long data_toggle:1;
	unsigned long reserved:1;
	unsigned long max_transfer:11;	// misnamed. Desired length might be better

	void *buffer;
	unsigned long data[4];	// free use by driver
} __attribute__((packed)) td_t;

typedef struct queue_head {
	link_pointer_t bredth;	// depth must = 0
	link_pointer_t depth;	// depth may vary randomly, ignore
	unsigned long int udata[2];
} __attribute__((packed)) queue_head_t;

typedef struct transaction {
	queue_head_t	*qh;
	td_t		*td_list;
	struct transaction *next;
} transaction_t;

//#####################################################
int wait_head( queue_head_t *head, int count);

extern queue_head_t *free_qh;
extern queue_head_t *queue_heads;

queue_head_t *new_queue_head(void);
void free_queue_head( queue_head_t *qh);
void init_qh(void);

extern td_t *free_td_list;
extern td_t *tds;

void init_td(void);
td_t *new_td(void);
td_t *find_last_td(td_t *td);
void free_td( td_t *td);
link_pointer_t *queue_end( queue_head_t *queue);
void add_td( queue_head_t *head, td_t *td);

extern transaction_t transactions[MAX_TRANSACTIONS];
extern transaction_t *free_transactions;

void init_transactions(void);
void free_transaction( transaction_t *trans );
transaction_t *new_transaction(td_t *td);
transaction_t *add_transaction( transaction_t *trans, td_t *td);


#define USBCMD(x) hc_base[x]
#define USBSTS(x) (hc_base[x] + 0x02)
#define USBINTR(x) (hc_base[x] + 0x04)
#define FRNUM(x) ( hc_base[x] + 0x06)
#define FLBASE(x) ( hc_base[x] + 0x08)
#define SOFMOD(x) ( hc_base[x] + 0x0c)
#define PORTSC1(x) ( hc_base[x] + 0x10)
#define PORTSC2(x) ( hc_base[x] + 0x12)

#define USBCMDRUN 0x01
#define USBCMD_DEBUG 0x20

#define USBSTSHALTED 0x20


void hc_reset(uchar dev);
int hc_stop(void);
int hc_start(uchar dev);

extern queue_head_t *sched_queue[];

void init_sched(uchar dev);
int poll_queue_head( queue_head_t *qh);
int wait_queue_complete( queue_head_t *qh);

extern int num_polls;
extern int (*devpoll[MAX_POLLDEV])(uchar);
extern uchar parm[MAX_POLLDEV];

transaction_t *_bulk_transfer( uchar devnum, uchar ep, unsigned int len, uchar *data);
transaction_t *ctrl_msg(uchar devnum, uchar request_type, uchar request, unsigned short wValue, unsigned short wIndex, unsigned short wLength, uchar *data);
int schedule_transaction( uchar dev, transaction_t *trans);
int wait_transaction( transaction_t *trans);
void unlink_transaction( uchar dev, transaction_t *trans);
int uhci_bulk_transfer( uchar devnum, uchar ep, unsigned int len, uchar *data);
int uhci_control_msg( uchar devnum, uchar request_type, uchar request, unsigned short wValue, unsigned short wIndex, unsigned short wLength, void *data);


// defined in uhci.c
int uhc_init(struct pci_device *dev);
void uhci_init(void);
void clear_uport_stat(unsigned short port);
int poll_u_root_hub(unsigned short port, uchar controller);

#endif
