#ifndef	_IF_ETHER_H
#define	_IF_ETHER_H

/*
   I'm moving towards the defined names in linux/if_ether.h for clarity.
   The confusion between 60/64 and 1514/1518 arose because the NS8390
   counts the 4 byte frame checksum in the incoming packet, but not
   in the outgoing packet. 60/1514 are the correct numbers for most
   if not all of the other NIC controllers.
*/

#define ETH_ALEN		6	/* Size of Ethernet address */
#define ETH_HLEN		14	/* Size of ethernet header */
#define	ETH_ZLEN		60	/* Minimum packet */
#define	ETH_FRAME_LEN		1514	/* Maximum packet */
#define ETH_DATA_ALIGN		2	/* Amount needed to align the data after an ethernet header */
#ifndef	ETH_MAX_MTU
#define	ETH_MAX_MTU		(ETH_FRAME_LEN-ETH_HLEN)
#endif

#define ETH_P_IP	0x0800		/* Internet Protocl Packet */
#define ETH_P_ARP	0x0806		/* Address Resolution Protocol */
#define ETH_P_RARP	0x8035		/* Reverse Address resolution Protocol */
#define ETH_P_IPV6	0x86DD		/* IPv6 over blueblook */
#define ETH_P_SLOW	0x8809		/* Ethernet slow protocols */

#endif	/* _IF_ETHER_H */
