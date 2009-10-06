/*****************************************************************************
 * ROM prefix that relocates to HIGHMEM_LOADPOINT during POST if PMM allocation
 * fails. Intended to be used, with caution, on BIOSes that support PCI3.00 but
 * have limited PMM support, such as most AMI BIOSes.
 *****************************************************************************
 */

FILE_LICENCE ( GPL2_OR_LATER )

#define SHRINK_WITHOUT_PMM

#include "romprefix.S"
