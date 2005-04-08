#ifndef BIOS_H
#define BIOS_H

extern unsigned long currticks ( void );
extern void cpu_nap ( void );
extern void disk_init ( void );
extern unsigned int pcbios_disk_read ( int drive, int cylinder, int head,
				       int sector, char *fixme_buf );

#endif /* BIOS_H */
