#ifndef __E132_XS_BOARD_H
#define __E132_XS_BOARD_H

#define CONFIG_HYPERSTONE_OSC_FREQ_MHZ	15

#define NR_MEMORY_REGNS	3
#define BASEMEM			0x0

/* SDRAM mapping */
#define	SDRAM_SIZE		0x01000000
#define SDRAM_BASEMEM	BASEMEM	

/* SRAM mapping */
#define SRAM_BASEMEM	0x40000000
#define SRAM_SIZE		0x0003FFFF

/* IRAM mapping */
#define IRAM_BASEMEM	0xC0000000
#define IRAM_SIZE		0x00003FFF


#endif /* __E132_XS_BOARD_H */
