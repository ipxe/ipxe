/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Architecture: ARM9TDMI
 * Processor   : P2001
 */

#ifndef ARCH_HARDWARE_H
#define ARCH_HARDWARE_H

#ifndef __ASSEMBLY__

/* DMA descriptor */
typedef struct {
	unsigned int stat;			/* status: own, start, end, offset, status */
	unsigned int cntl;			/* control: loop, int, type, channel, length */
	char	*buf;				/* buffer */
	void	*next;				/* nextdsc */
} DMA_DSC;


/* The address definitions are from asic_bf.h */
typedef struct {					// 0x00100000U
	volatile unsigned int reserved1[0x3];
	volatile unsigned int ArmDmaPri;		// 0x0000000CU
	volatile unsigned int SDRAM_Ctrl;		// 0x00000010U
	volatile unsigned int ExtMem_Ctrl;		// 0x00000014U
	volatile unsigned int WaitState_Ext;		// 0x00000018U
	volatile unsigned int WaitState_Asic;		// 0x0000001CU
	volatile unsigned int TOP;			// 0x00000020U
	volatile unsigned int reserved2[0x3];
	volatile unsigned int Adr1_EQ_30Bit;		// 0x00000030U
	volatile unsigned int Adr2_EQ_30Bit;		// 0x00000034U
	volatile unsigned int Adr3_EQ_30Bit;		// 0x00000038U
	volatile unsigned int Dat3_EQ_32Bit;		// 0x0000003CU
	volatile unsigned int Adr4_HE_20Bit;		// 0x00000040U
	volatile unsigned int Adr4_LT_20Bit;		// 0x00000044U
	volatile unsigned int Adr5_HE_20Bit;		// 0x00000048U
	volatile unsigned int Adr5_LT_20Bit;		// 0x0000004CU
	volatile unsigned int Adr_Control;		// 0x00000050U
	volatile unsigned int ABORT_IA_32Bit;		// 0x00000054U
} *P2001_SYS_regs_ptr;
#define P2001_SYS ((volatile P2001_SYS_regs_ptr) 0x00100000)

typedef struct {					// 0x00110000U
	volatile unsigned int Timer1;			// 0x00000000U
	volatile unsigned int Timer2;			// 0x00000004U
	volatile unsigned int TIMER_PRELOAD;		// 0x00000008U
	volatile unsigned int Timer12_PreDiv;		// 0x0000000CU
	volatile unsigned int TIMER_INT;		// 0x00000010U
	volatile unsigned int Freerun_Timer;		// 0x00000014U
	volatile unsigned int WatchDog_Timer;		// 0x00000018U
	volatile unsigned int PWM_CNT;			// 0x00000020U
	volatile unsigned int PWM_CNT2;			// 0x00000024U
	volatile unsigned int PLL_12000_config;		// 0x00000030U
	volatile unsigned int PLL_12288_config;		// 0x00000034U
	volatile unsigned int DIV_12288_config;		// 0x00000038U
	volatile unsigned int MOD_CNT_768;		// 0x0000003CU
	volatile unsigned int FSC_IRQ_STATUS;		// 0x00000040U
	volatile unsigned int FSC_CONFIG;		// 0x00000044U
	volatile unsigned int FSC_CONSTRUCT;		// 0x00000048U
	volatile unsigned int FSC_base_clk_reg;		// 0x0000004CU
	volatile unsigned int SYSCLK_SHAPE;		// 0x00000050U
	volatile unsigned int SDRAMCLK_SHAPE;		// 0x00000054U
	volatile unsigned int RING_OSZI;		// 0x00000058U
} *P2001_TIMER_regs_ptr;
#define P2001_TIMER ((volatile P2001_TIMER_regs_ptr) 0x00110000)

typedef struct {					// 0x00120000U
	volatile unsigned int reserved1[0x5];
	volatile unsigned int GPIO_Config;		// 0x00000014U
	volatile unsigned int GPIO_INT;			// 0x00000018U
	volatile unsigned int GPIO_Out;			// 0x0000001CU
	volatile unsigned int GPIO_IN;			// 0x00000020U
	volatile unsigned int GPIO_En;			// 0x00000024U
	volatile unsigned int PIN_MUX;			// 0x00000028U
	volatile unsigned int NRES_OUT;			// 0x0000002CU
	volatile unsigned int GPIO2_Out;		// 0x00000030U
	volatile unsigned int GPIO2_IN;			// 0x00000034U
	volatile unsigned int GPIO2_En;			// 0x00000038U
	volatile unsigned int GPIO_INT_SEL;		// 0x0000003CU
	volatile unsigned int GPI3_IN;			// 0x00000040U
	volatile unsigned int GPO4_OUT;			// 0x00000044U
} *P2001_GPIO_regs_ptr;
#define P2001_GPIO ((volatile P2001_GPIO_regs_ptr) 0x00120000)

typedef struct {					// 0x00130000U
	volatile unsigned int Main_NFIQ_Int_Ctrl;	// 0x00000000U
	volatile unsigned int Main_NIRQ_Int_Ctrl;	// 0x00000004U
	volatile unsigned int Status_NFIQ;		// 0x00000008U
	volatile unsigned int Status_NIRQ;		// 0x0000000CU
} *P2001_INT_CTRL_regs_ptr;
#define P2001_INT_CTRL ((volatile P2001_INT_CTRL_regs_ptr) 0x00130000)

typedef union {						// 0x00140000U
	struct {	// write
		volatile unsigned int TX[4];		// 0x00000000-0x000CU
		volatile unsigned int Baudrate;		// 0x00000010U
		volatile unsigned int reserved1[0x3];
		volatile unsigned int Config;		// 0x00000020U
		volatile unsigned int Clear;		// 0x00000024U
		volatile unsigned int Echo_EN;		// 0x00000028U
		volatile unsigned int IRQ_Status;	// 0x0000002CU
	} w;		// write
	
	struct {	// read
		volatile unsigned int RX[4];		// 0x00000000-0x000CU
		volatile unsigned int reserved1[0x4];
		volatile unsigned int PRE_STATUS;	// 0x00000020U
		volatile unsigned int STATUS;		// 0x00000024U
		volatile unsigned int reserved2[0x1];
		volatile unsigned int IRQ_Status;	// 0x0000002CU
	} r;		// read
} *P2001_UART_regs_ptr;
#define P2001_UART ((volatile P2001_UART_regs_ptr) 0x00140000)

typedef struct {				// 0x0018_000U _=0,1,2,3
	volatile DMA_DSC *    RMAC_DMA_DESC;	// 0x00000000U
	volatile unsigned int RMAC_DMA_CNTL;	// 0x00000004U
	volatile unsigned int RMAC_DMA_STAT;	// 0x00000008U
	volatile unsigned int RMAC_DMA_EN;	// 0x0000000CU
	volatile unsigned int RMAC_CNTL;	// 0x00000010U
	volatile unsigned int RMAC_TLEN;	// 0x00000014U
	volatile unsigned int RMAC_PHYU;	// 0x00000018U
	volatile unsigned int RMAC_PHYL;	// 0x0000001CU
	volatile unsigned int RMAC_PFM0;	// 0x00000020U
	volatile unsigned int RMAC_PFM1;	// 0x00000024U
	volatile unsigned int RMAC_PFM2;	// 0x00000028U
	volatile unsigned int RMAC_PFM3;	// 0x0000002CU
	volatile unsigned int RMAC_PFM4;	// 0x00000030U
	volatile unsigned int RMAC_PFM5;	// 0x00000034U
	volatile unsigned int RMAC_PFM6;	// 0x00000038U
	volatile unsigned int RMAC_PFM7;	// 0x0000003CU
	volatile unsigned int RMAC_MIB0;	// 0x00000040U
	volatile unsigned int RMAC_MIB1;	// 0x00000044U
	volatile unsigned int RMAC_MIB2;	// 0x00000048U
	volatile unsigned int RMAC_MIB3;	// 0x0000004CU
	volatile unsigned int RMAC_MIB4;	// 0x00000050U
	volatile unsigned int RMAC_MIB5;	// 0x00000054U
	volatile unsigned int reserved1[0x1e8];
	volatile unsigned int RMAC_DMA_DATA;	// 0x000007F8U
	volatile unsigned int RMAC_DMA_ADR;	// 0x000007FCU
	volatile DMA_DSC *    TMAC_DMA_DESC;	// 0x00000800U
	volatile unsigned int TMAC_DMA_CNTL;	// 0x00000804U
	volatile unsigned int TMAC_DMA_STAT;	// 0x00000808U
	volatile unsigned int TMAC_DMA_EN;	// 0x0000080CU
	volatile unsigned int TMAC_CNTL;	// 0x00000810U
	volatile unsigned int TMAC_MIB6;	// 0x00000814U
	volatile unsigned int TMAC_MIB7;	// 0x00000818U
	volatile unsigned int reserved2[0x1];
	volatile unsigned int MU_CNTL;		// 0x00000820U
	volatile unsigned int MU_DATA;		// 0x00000824U
	volatile unsigned int MU_DIV;		// 0x00000828U
	volatile unsigned int CONF_RMII;	// 0x0000082CU
	volatile unsigned int reserved3[0x1f2];
	volatile unsigned int TMAC_DMA_DATA;	// 0x00000FF8U
	volatile unsigned int TMAC_DMA_ADR;	// 0x00000FFCU
} *P2001_ETH_regs_ptr;
#define P2001_EU(x) ((volatile P2001_ETH_regs_ptr) ((unsigned int) 0x00180000UL+(0x1000UL*(x)))) /* x = 0..3 */
#define P2001_MU  P2001_EU(0)

#endif

#endif  /* ARCH_HARDWARE_H */
