#ifndef MAIN_H
#define MAIN_H

#include "dev.h"

extern struct dev dev;

extern int main ( void );
extern void set_pci_device ( uint16_t busdevfn );

#endif /* MAIN_H */
