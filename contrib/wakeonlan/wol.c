/*
 * Copyright (C) 2008 William Stewart.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <gpxe/features.h>
#include <gpxe/netdevice.h>
#include <gpxe/if_ether.h>
#include <gpxe/iobuf.h>
#include <usr/ifmgmt.h>
#include <usr/wol.h>
#include <timer.h>

/** @file
 *
 * Wake on lan
 *
 */

/**
 * Boot from a network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
#define WOL_MSG_LEN (6 + 16*6)
 
void wakeup_server(char *server_adr)
{
	int rc, i,j;
	unsigned char *buf;
	uint8_t eth_broadcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	unsigned int len;
        struct io_buffer *iobuf;
        struct ethhdr *ethhdr;
        struct net_device *netdev;
	
	for_each_netdev ( netdev ) {
	   break;
	}

        if (netdev == NULL)
        {
          printf("Could not find netdev\n");
          return;
        }
               
	/* Open device and display device status */
	if ( (ifopen ( netdev ) ) != 0 )
	{
	  printf("Could not open netdev\n");
	  return;
	}

	/* Create outgoing I/O buffer */
	iobuf = alloc_iob ((ETH_HLEN + WOL_MSG_LEN)*2);
	if (!iobuf)
	{
	   printf("Could not allocate iob\n");
	   return;
	}

        ethhdr = iob_put(iobuf, sizeof(*ethhdr));
        
	/* Build Ethernet header */
	memcpy (ethhdr->h_dest, eth_broadcast, ETH_ALEN );
	memcpy (ethhdr->h_source, netdev->ll_addr, ETH_ALEN );
	ethhdr->h_protocol = htons (0x0842);
	
	buf    = iob_put (iobuf, WOL_MSG_LEN);

        /* Build the message to send - 6 x 0xff then 16 x dest address */
        len =0;
        for (i = 0; i < 6; i++)
           buf[len++] = 0xff;
        for (j = 0; j < 16; j++)
           for (i = 0; i < 6; i++)
               buf[len++] = server_adr[i];      

	rc = netdev_tx (netdev, iobuf);

	if (rc !=0)
	   printf("Failed to transmit WOL packet\n");

        /* Give the controller a chance to send it before checking */
        mdelay(100);

	netdev_poll(netdev); 
}

