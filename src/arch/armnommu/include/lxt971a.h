/*
 *  Copyright (C) 2004 Tobias Lorenz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Intel LXT971ALE (MII-compatible PHY)
 */

#define Adr_LXT971A_Control		0	/* Control Register */
#define Adr_LXT971A_Status1		1	/* MII Status Register #1 */
#define Adr_LXT971A_PHY_ID1		2	/* PHY Identification Register 1 */
#define Adr_LXT971A_PHY_ID2		3	/* PHY Identification Register 2 */
#define Adr_LXT971A_AN_Advertise	4	/* Auto Negotiation Advertisement Register */
#define Adr_LXT971A_AN_Link_Ability	5	/* Auto Negotiation Link Partner Base Page Ability Register */
#define Adr_LXT971A_AN_Expansion	6	/* Auto Negotiation Expansion */
#define Adr_LXT971A_AN_Next_Page_Txmit	7	/* Auto Negotiation Next Page Transmit Register */
#define Adr_LXT971A_AN_Link_Next_Page	8	/* Auto Negotiation Link Partner Next Page Receive Register */
#define Adr_LXT971A_Fast_Control	9	/* Not Implemented */
#define Adr_LXT971A_Fast_Status		10	/* Not Implemented */
#define Adr_LXT971A_Extended_Status	15	/* Not Implemented */
#define Adr_LXT971A_Port_Config		16	/* Configuration Register */
#define Adr_LXT971A_Status2		17	/* Status Register #2 */
#define Adr_LXT971A_Interrupt_Enable	18	/* Interrupt Enable Register */
#define Adr_LXT971A_Interrupt_Status	19	/* Interrupt Status Register */
#define Adr_LXT971A_LED_Config		20	/* LED Configuration Register */
#define Adr_LXT971A_Transmit_Control	30	/* Transmit Control Register */
