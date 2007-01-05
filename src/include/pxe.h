#ifndef PXE_H
#define PXE_H

#include "pxe_types.h"
#include "pxe_api.h"
#include <gpxe/device.h>

/* Parameter block for pxenv_unknown() */
struct s_PXENV_UNKNOWN {
	PXENV_STATUS_t Status;			/**< PXE status code */
} PACKED;

typedef struct s_PXENV_UNKNOWN PXENV_UNKNOWN_t;

/* Union used for PXE API calls; we don't know the type of the
 * structure until we interpret the opcode.  Also, Status is available
 * in the same location for any opcode, and it's convenient to have
 * non-specific access to it.
 */
union u_PXENV_ANY {
	/* Make it easy to read status for any operation */
	PXENV_STATUS_t				Status;
	struct s_PXENV_UNKNOWN			unknown;
	struct s_PXENV_UNLOAD_STACK		unload_stack;
	struct s_PXENV_GET_CACHED_INFO		get_cached_info;
	struct s_PXENV_TFTP_READ_FILE		restart_tftp;
	struct s_PXENV_START_UNDI		start_undi;
	struct s_PXENV_STOP_UNDI		stop_undi;
	struct s_PXENV_START_BASE		start_base;
	struct s_PXENV_STOP_BASE		stop_base;
	struct s_PXENV_TFTP_OPEN		tftp_open;
	struct s_PXENV_TFTP_CLOSE		tftp_close;
	struct s_PXENV_TFTP_READ		tftp_read;
	struct s_PXENV_TFTP_READ_FILE		tftp_read_file;
	struct s_PXENV_TFTP_GET_FSIZE		tftp_get_fsize;
	struct s_PXENV_UDP_OPEN			udp_open;
	struct s_PXENV_UDP_CLOSE		udp_close;
	struct s_PXENV_UDP_WRITE		udp_write;
	struct s_PXENV_UDP_READ			udp_read;
	struct s_PXENV_UNDI_STARTUP		undi_startup;
	struct s_PXENV_UNDI_CLEANUP		undi_cleanup;
	struct s_PXENV_UNDI_INITIALIZE		undi_initialize;
	struct s_PXENV_UNDI_RESET		undi_reset_adapter;
	struct s_PXENV_UNDI_SHUTDOWN		undi_shutdown;
	struct s_PXENV_UNDI_OPEN		undi_open;
	struct s_PXENV_UNDI_CLOSE		undi_close;
	struct s_PXENV_UNDI_TRANSMIT		undi_transmit;
	struct s_PXENV_UNDI_SET_MCAST_ADDRESS	undi_set_mcast_address;
	struct s_PXENV_UNDI_SET_STATION_ADDRESS undi_set_station_address;
	struct s_PXENV_UNDI_SET_PACKET_FILTER	undi_set_packet_filter;
	struct s_PXENV_UNDI_GET_INFORMATION	undi_get_information;
	struct s_PXENV_UNDI_GET_STATISTICS	undi_get_statistics;
	struct s_PXENV_UNDI_CLEAR_STATISTICS	undi_clear_statistics;
	struct s_PXENV_UNDI_INITIATE_DIAGS	undi_initiate_diags;
	struct s_PXENV_UNDI_FORCE_INTERRUPT	undi_force_interrupt;
	struct s_PXENV_UNDI_GET_MCAST_ADDRESS	undi_get_mcast_address;
	struct s_PXENV_UNDI_GET_NIC_TYPE	undi_get_nic_type;
	struct s_PXENV_UNDI_GET_IFACE_INFO	undi_get_iface_info;
	struct s_PXENV_UNDI_GET_STATE		undi_get_state;
	struct s_PXENV_UNDI_ISR			undi_isr;
};

typedef union u_PXENV_ANY PXENV_ANY_t;

/** An UNDI expansion ROM */
struct undi_rom {
	/** Signature
	 *
	 * Must be equal to @c ROM_SIGNATURE
	 */
	UINT16_t Signature;
	/** ROM length in 512-byte blocks */
	UINT8_t ROMLength;
	/** Unused */
	UINT8_t unused[0x13];
	/** Offset of the PXE ROM ID structure */
	UINT16_t PXEROMID;
	/** Offset of the PCI ROM structure */
	UINT16_t PCIRHeader;
} PACKED;

/** Signature for an expansion ROM */
#define ROM_SIGNATURE 0xaa55

/** An UNDI ROM ID structure */
struct undi_rom_id {
	/** Signature
	 *
	 * Must be equal to @c UNDI_ROM_ID_SIGNATURE
	 */
	UINT32_t Signature;
	/** Length of structure */
	UINT8_t StructLength;
	/** Checksum */
	UINT8_t StructCksum;
	/** Structure revision
	 *
	 * Must be zero.
	 */
	UINT8_t StructRev;
	/** UNDI revision
	 *
	 * Version 2.1.0 is encoded as the byte sequence 0x00, 0x01, 0x02.
	 */
	UINT8_t UNDIRev[3];
	/** Offset to UNDI loader */
	UINT16_t UNDILoader;
	/** Minimum required stack segment size */
	UINT16_t StackSize;
	/** Minimum required data segment size */
	UINT16_t DataSize;
	/** Minimum required code segment size */
	UINT16_t CodeSize;
} PACKED;

/** Signature for an UNDI ROM ID structure */
#define UNDI_ROM_ID_SIGNATURE \
	( ( 'U' << 0 ) + ( 'N' << 8 ) + ( 'D' << 16 ) + ( 'I' << 24 ) )

/** A PCI expansion header */
struct pcir_header {
	/** Signature
	 *
	 * Must be equal to @c PCIR_SIGNATURE
	 */
	uint32_t signature;
	/** PCI vendor ID */
	uint16_t vendor_id;
	/** PCI device ID */
	uint16_t device_id;
} PACKED;

/** Signature for an UNDI ROM ID structure */
#define PCIR_SIGNATURE \
	( ( 'P' << 0 ) + ( 'C' << 8 ) + ( 'I' << 16 ) + ( 'R' << 24 ) )

/** A PXE PCI device ID */
struct pxe_pci_device_id {
	/** PCI vendor ID */
	unsigned int vendor_id;
	/** PCI device ID */
	unsigned int device_id;
};

/** A PXE device ID */
union pxe_device_id {
	/** PCI device ID */
	struct pxe_pci_device_id pci;
};

/** A PXE driver */
struct pxe_driver {
	/** List of PXE drivers */
	struct list_head list;
	/** ROM segment address */
	unsigned int rom_segment;
	/** UNDI loader entry point */
	SEGOFF16_t loader;
	/** Code segment size */
	size_t code_size;
	/** Data segment size */
	size_t data_size;
	/** Bus type
	 *
	 * Values are as used by @c PXENV_UNDI_GET_NIC_TYPE
	 */
	unsigned int bus_type;
	/** Device ID */
	union pxe_device_id bus_id;
};

/** A PXE device */
struct pxe_device {
	/** Generic device */
	struct device dev;
	/** Driver-private data
	 *
	 * Use pxe_set_drvdata() and pxe_get_drvdata() to access this
	 * field.
	 */
	void *priv;

	/** PXENV+ structure address */
	SEGOFF16_t pxenv;
	/** !PXE structure address */
	SEGOFF16_t ppxe;
	/** Entry point */
	SEGOFF16_t entry;
	/** MAC address */
	MAC_ADDR_t hwaddr;
	/** Assigned IRQ number */
	UINT16_t irq;
	/** ROM segment address */
	SEGSEL_t rom_segment;
};

/**
 * Set PXE driver-private data
 *
 * @v pxe		PXE device
 * @v priv		Private data
 */
static inline void pxe_set_drvdata ( struct pxe_device *pxe, void *priv ) {
	pxe->priv = priv;
}

/**
 * Get PXE driver-private data
 *
 * @v pxe		PXE device
 * @ret priv		Private data
 */
static inline void * pxe_get_drvdata ( struct pxe_device *pxe ) {
	return pxe->priv;
}

extern int pxe_call ( struct pxe_device *pxe, unsigned int function,
		      void *params, size_t params_len );
extern int undi_probe ( struct pxe_device *pxe );
extern void undi_remove ( struct pxe_device *pxe );

extern struct pxe_driver * pxedrv_find_pci_driver ( unsigned int vendor_id,
						    unsigned int device_id,
						    unsigned int rombase );

extern struct net_device *pxe_netdev;

#endif /* PXE_H */
