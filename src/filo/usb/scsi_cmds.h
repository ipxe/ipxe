#ifndef _SCSI_CMDS_H
#define _SCSI_CMDS_H

#define devhandle uint8_t

#define uchar uint8_t
#define ushort uint16_t

void PrintSense(uchar *sense, int len);
int ll_read_block(devhandle sgd, char *buffer, int blocknum, int count);

int get_capacity(devhandle sgd, unsigned long *block_count, unsigned int *blk_len);
int UnitReady(uchar sgd);
#endif
