#ifndef SHARED_H
#define SHARED_H

/*
 * To save space in the binary when multiple-driver images are
 * compiled, uninitialised data areas can be shared between drivers.
 * This will typically be used to share statically-allocated receive
 * and transmit buffers between drivers.
 *
 * Use as e.g.
 *
 *  struct {
 *	char	rx_buf[NUM_RX_BUF][RX_BUF_SIZE];
 *	char	tx_buf[TX_BUF_SIZE];
 *  } my_static_data __shared;
 *
 */

#define __shared __asm__ ( "_shared_bss" );

#endif /* SHARED_H */
