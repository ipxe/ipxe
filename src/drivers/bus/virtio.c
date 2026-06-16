/*
 * Copyright (C) 2026 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );
FILE_SECBOOT ( PERMITTED );

/** @file
 *
 * Virtual I/O device
 *
 */

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <ipxe/pci.h>
#include <ipxe/virtio.h>

/******************************************************************************
 *
 * Original ("legacy") device operations
 *
 ******************************************************************************
 */

/**
 * Reset device
 *
 * @v virtio		Virtio device
 * @ret rc		Return status code
 */
static int virtio_legacy_reset ( struct virtio_device *virtio ) {
	uint8_t stat;
	unsigned int i;

	/* Reset device */
	iowrite8 ( 0, virtio->common + VIRTIO_LEG_STAT );

	/* Wait for reset to complete */
	for ( i = 0 ; i < VIRTIO_RESET_MAX_WAIT_MS ; i++ ) {
		stat = ioread8 ( virtio->common + VIRTIO_LEG_STAT );
		if ( ! stat )
			return 0;
		mdelay ( 1 );
	}

	DBGC ( virtio, "VIRTIO %s could not reset device\n", virtio->name );
	return -ETIMEDOUT;
}

/**
 * Report driver status
 *
 * @v virtio		Virtio device
 * @ret stat		Actual device status
 */
static unsigned int virtio_legacy_status ( struct virtio_device *virtio ) {

	/* Report device status */
	iowrite8 ( virtio->stat, virtio->common + VIRTIO_LEG_STAT );

	/* Read back device status */
	return ioread8 ( virtio->common + VIRTIO_LEG_STAT );
}

/**
 * Get supported features
 *
 * @v virtio		Virtio device
 */
static void virtio_legacy_supported ( struct virtio_device *virtio ) {
	struct virtio_features *supported = &virtio->supported;
	unsigned int i;

	/* Get device supported features */
	supported->word[0] = ioread32 ( virtio->common + VIRTIO_LEG_FEAT );

	/* Legacy devices have only a single 32-bit feature register */
	for ( i = 1 ; i < VIRTIO_FEATURE_WORDS ; i++ )
		supported->word[i] = 0;
}

/**
 * Negotiate device features
 *
 * @v virtio		Virtio device
 */
static void virtio_legacy_negotiate ( struct virtio_device *virtio ) {
	struct virtio_features *features = &virtio->features;
	unsigned int i;

	/* Set in-use features */
	iowrite32 ( features->word[0], virtio->common + VIRTIO_LEG_USED );

	/* Legacy devices have only a single 32-bit feature register */
	for ( i = 1 ; i < VIRTIO_FEATURE_WORDS ; i++ )
		assert ( features->word[i] == 0 );
}

/**
 * Set queue size
 *
 * @v virtio		Virtio device
 * @v queue		Virtio queue
 * @v count		Requested size
 */
static void virtio_legacy_size ( struct virtio_device *virtio,
				 struct virtio_queue *queue,
				 unsigned int count ) {
	size_t len;

	/* Select queue */
	iowrite16 ( queue->index, virtio->common + VIRTIO_LEG_SEL );

	/* Get (fixed) queue size */
	count = ioread16 ( virtio->common + VIRTIO_LEG_SIZE );

	/* Calculate queue length */
	len = virtio_desc_size ( count );
	len = virtio_align ( len + virtio_sq_size ( count ) );
	len = virtio_align ( len + virtio_cq_size ( count ) );

	/* Record queue size */
	queue->count = count;
	queue->len = len;
}

/**
 * Enable queue
 *
 * @v virtio		Virtio device
 * @v queue		Virtio queue
 */
static void virtio_legacy_enable ( struct virtio_device *virtio,
				   struct virtio_queue *queue ) {
	unsigned int count = queue->count;
	void *base = queue->desc;
	size_t len;

	/* Select queue */
	iowrite16 ( queue->index, virtio->common + VIRTIO_LEG_SEL );

	/* Lay out queue regions */
	len = virtio_desc_size ( count );
	queue->sq = ( base + len );
	len = virtio_align ( len + virtio_sq_size ( count ) );
	queue->cq = ( base + len );
	len = virtio_align ( len + virtio_cq_size ( count ) );
	assert ( len == queue->len );

	/* Program queue base page address */
	iowrite32 ( ( dma ( &queue->map, queue->desc ) / VIRTIO_PAGE ),
		    virtio->common + VIRTIO_LEG_BASE );
}

/** Original ("legacy") device operations */
static struct virtio_operations virtio_legacy_operations = {
	.reset = virtio_legacy_reset,
	.status = virtio_legacy_status,
	.supported = virtio_legacy_supported,
	.negotiate = virtio_legacy_negotiate,
	.size = virtio_legacy_size,
	.enable = virtio_legacy_enable,
};

/******************************************************************************
 *
 * PCI ("modern") device operations
 *
 ******************************************************************************
 */

/**
 * Reset device
 *
 * @v virtio		Virtio device
 * @ret rc		Return status code
 */
static int virtio_pci_reset ( struct virtio_device *virtio ) {
	uint8_t stat;
	unsigned int i;

	/* Reset device */
	iowrite8 ( 0, virtio->common + VIRTIO_PCI_STAT );

	/* Wait for reset to complete */
	for ( i = 0 ; i < VIRTIO_RESET_MAX_WAIT_MS ; i++ ) {
		stat = ioread8 ( virtio->common + VIRTIO_PCI_STAT );
		if ( ! stat )
			return 0;
		mdelay ( 1 );
	}

	DBGC ( virtio, "VIRTIO %s could not reset device\n", virtio->name );
	return -ETIMEDOUT;
}

/**
 * Report driver status
 *
 * @v virtio		Virtio device
 * @ret stat		Actual device status
 */
static unsigned int virtio_pci_status ( struct virtio_device *virtio ) {

	/* Report device status */
	iowrite8 ( virtio->stat, virtio->common + VIRTIO_PCI_STAT );

	/* Read back device status */
	return ioread8 ( virtio->common + VIRTIO_PCI_STAT );
}

/**
 * Get supported features
 *
 * @v virtio		Virtio device
 */
static void virtio_pci_supported ( struct virtio_device *virtio ) {
	struct virtio_features *supported = &virtio->supported;
	unsigned int i;

	/* Get device supported features */
	for ( i = 0 ; i < VIRTIO_FEATURE_WORDS ; i++ ) {
		iowrite32 ( i, virtio->common + VIRTIO_PCI_FEAT_SEL );
		supported->word[i] =
			ioread32 ( virtio->common + VIRTIO_PCI_FEAT );
	}
}

/**
 * Negotiate device features
 *
 * @v virtio		Virtio device
 */
static void virtio_pci_negotiate ( struct virtio_device *virtio ) {
	struct virtio_features *features = &virtio->features;
	unsigned int i;

	/* Set in-use features */
	for ( i = 0 ; i < VIRTIO_FEATURE_WORDS ; i++ ) {
		iowrite32 ( i, virtio->common + VIRTIO_PCI_USED_SEL );
		iowrite32 ( features->word[i],
			    virtio->common + VIRTIO_PCI_USED );
	}
}

/**
 * Set queue size
 *
 * @v virtio		Virtio device
 * @v queue		Virtio queue
 * @v count		Requested size
 */
static void virtio_pci_size ( struct virtio_device *virtio,
			      struct virtio_queue *queue,
			      unsigned int count ) {
	unsigned int max;
	size_t len;

	/* Select queue */
	iowrite16 ( queue->index, virtio->common + VIRTIO_PCI_SEL );

	/* Set queue size */
	max = ioread16 ( virtio->common + VIRTIO_PCI_SIZE );
	if ( count > max )
		count = max;
	iowrite16 ( count, virtio->common + VIRTIO_PCI_SIZE );

	/* Calculate queue length */
	len = virtio_align ( virtio_desc_size ( count ) );
	len = virtio_align ( len + virtio_sq_size ( count ) );
	len = virtio_align ( len + virtio_cq_size ( count ) );

	/* Record queue size */
	queue->count = count;
	queue->len = len;
}

/**
 * Program queue address
 *
 * @v virtio		Virtio device
 * @v queue		Virtio queue
 * @v addr		Address
 * @v offset		Register offset
 */
static void virtio_pci_address ( struct virtio_device *virtio,
				 struct virtio_queue *queue,
				 void *addr, unsigned int offset ) {
	physaddr_t phys;

	/* Program address */
	phys = dma ( &queue->map, addr );
	iowrite32 ( ( phys & 0xffffffffUL ), ( virtio->common + offset + 0 ) );
	if ( sizeof ( physaddr_t ) > sizeof ( uint32_t ) ) {
		iowrite32 ( ( ( ( uint64_t ) phys ) >> 32 ),
			    ( virtio->common + offset + 4 ) );
	} else {
		iowrite32 ( 0, ( virtio->common + offset + 4 ) );
	}
}

/**
 * Enable queue
 *
 * @v virtio		Virtio device
 * @v queue		Virtio queue
 */
static void virtio_pci_enable ( struct virtio_device *virtio,
				struct virtio_queue *queue ) {
	unsigned int count = queue->count;
	void *base = queue->desc;
	size_t len;

	/* Select queue */
	iowrite16 ( queue->index, virtio->common + VIRTIO_PCI_SEL );

	/* Lay out queue regions */
	len = virtio_align ( virtio_desc_size ( count ) );
	queue->sq = ( base + len );
	len = virtio_align ( len + virtio_sq_size ( count ) );
	queue->cq = ( base + len );
	len = virtio_align ( len + virtio_cq_size ( count ) );
	assert ( len == queue->len );

	/* Program queue addresses */
	virtio_pci_address ( virtio, queue, queue->desc, VIRTIO_PCI_DESC );
	virtio_pci_address ( virtio, queue, queue->sq, VIRTIO_PCI_SQ );
	virtio_pci_address ( virtio, queue, queue->cq, VIRTIO_PCI_CQ );

	/* Enable queue */
	iowrite16 ( 1, virtio->common + VIRTIO_PCI_ENABLE );
}

/** PCI ("modern") device operations */
static struct virtio_operations virtio_pci_operations = {
	.reset = virtio_pci_reset,
	.status = virtio_pci_status,
	.supported = virtio_pci_supported,
	.negotiate = virtio_pci_negotiate,
	.size = virtio_pci_size,
	.enable = virtio_pci_enable,
};

/**
 * Find PCI capability
 *
 * @v virtio		Virtio device
 * @v pci		PCI device
 * @v type		Capability type
 * @v cap		Virtio PCI capability to fill in
 * @ret rc		Return status code
 */
static int virtio_pci_cap ( struct virtio_device *virtio,
			    struct pci_device *pci, unsigned int type,
			    struct virtio_pci_capability *cap ) {
	unsigned int reg;
	int pos;

	/* Scan through vendor capabilities */
	for ( pos = pci_find_capability ( pci, PCI_CAP_ID_VNDR ) ; pos > 0 ;
	      pos = pci_find_next_capability ( pci, pos, PCI_CAP_ID_VNDR ) ) {

		/* Check length */
		pci_read_config_byte ( pci, ( pos + PCI_CAP_LEN ), &cap->len );
		if ( cap->len < VIRTIO_PCI_CAP_END ) {
			DBGC ( virtio, "VIRTIO %s capability +%#02x too short "
			       "(%d bytes)\n", virtio->name, pos, cap->len );
			continue;
		}

		/* Read values */
		pci_read_config_byte ( pci, ( pos + VIRTIO_PCI_CAP_TYPE ),
				       &cap->type );
		pci_read_config_byte ( pci, ( pos + VIRTIO_PCI_CAP_BAR ),
				       &cap->bar );
		pci_read_config_dword ( pci, ( pos + VIRTIO_PCI_CAP_OFFSET ),
					&cap->offset );

		/* Check type */
		if ( cap->type != type )
			continue;
		DBGC2 ( virtio, "VIRTIO %s capability type %d BAR%d+%#04x\n",
			virtio->name, type, cap->bar, cap->offset );

		/* Check BAR */
		reg = PCI_BASE_ADDRESS ( cap->bar );
		if ( reg > PCI_BASE_ADDRESS_5 )
			continue;

		/* Check BAR accessibility */
		if ( ! pci_bar_start ( pci, reg ) ) {
			DBGC ( virtio, "VIRTIO %s capability type %d BAR%d is "
			       "not usable\n", virtio->name, type, cap->bar );
			continue;
		}

		/* Success */
		cap->pos = pos;
		return 0;
	}

	DBGC ( virtio, "VIRTIO %s has no usable capability type %d\n",
	       virtio->name, type );
	cap->pos = 0;
	return -ENOENT;
}

/**
 * Map PCI capability
 *
 * @v virtio		Virtio device
 * @v pci		PCI device
 * @v cap		Virtio PCI capability
 * @ret io_addr		I/O address, or NULL on error
 */
static void * virtio_pci_map_cap ( struct virtio_device *virtio,
				   struct pci_device *pci,
				   struct virtio_pci_capability *cap ) {
	unsigned long addr;
	unsigned int reg;
	int is_io_bar;
	void *io_addr;

	/* Get BAR start address and type */
	reg = PCI_BASE_ADDRESS ( cap->bar );
	addr = pci_bar_start ( pci, reg );
	if ( ! addr ) {
		DBGC ( virtio, "VIRTIO %s BAR%d is not usable\n",
		       virtio->name, cap->bar );
		return NULL;
	}

	/* Map memory or I/O BAR */
	addr += cap->offset;
	is_io_bar = pci_bar_is_io ( pci, reg );
	io_addr = ( is_io_bar ? ( ( void * ) addr ) :
		    pci_ioremap ( pci, addr, VIRTIO_PAGE ) );
	if ( ! io_addr ) {
		DBGC ( virtio, "VIRTIO %s could not map BAR%d+%#04x\n",
		       virtio->name, cap->bar, cap->offset );
		return NULL;
	}

	DBGC2 ( virtio, "VIRTIO %s mapped BAR%d+%#04x (%s %#08lx)\n",
		virtio->name, cap->bar, cap->offset,
		( is_io_bar ? "IO" : "MEM" ), addr );
	return io_addr;
}

/**
 * Map PCI device
 *
 * @v virtio		Virtio device
 * @v pci		PCI device
 * @ret rc		Return status code
 */
int virtio_pci_map ( struct virtio_device *virtio, struct pci_device *pci ) {
	struct virtio_pci_capability common;
	struct virtio_pci_capability notify;
	struct virtio_pci_capability device;
	unsigned int msix;
	uint32_t mult;
	uint16_t ctrl;
	int rc;

	/* Initialise device */
	virtio->name = pci->dev.name;
	virtio->dma = &pci->dma;

	/* Fix up PCI device */
	adjust_pci_device ( pci );

	/* Check if MSI-X is enabled */
	msix = pci_find_capability ( pci, PCI_CAP_ID_MSIX );
	if ( msix ) {
		pci_read_config_word ( pci, msix, &ctrl );
		if ( ! ( ctrl & PCI_MSIX_CTRL_ENABLE ) )
			msix = 0;
	}

	/* Locate virtio capabilities */
	virtio_pci_cap ( virtio, pci, VIRTIO_PCI_CAP_TYPE_COMMON, &common );
	virtio_pci_cap ( virtio, pci, VIRTIO_PCI_CAP_TYPE_NOTIFY, &notify );
	virtio_pci_cap ( virtio, pci, VIRTIO_PCI_CAP_TYPE_DEVICE, &device );

	/* Use modern interface if available */
	if ( common.pos && notify.pos && device.pos &&
	     ( notify.len >= VIRTIO_PCI_CAP_NOTIFY_END ) ) {

		/* Use modern interface */
		virtio->op = &virtio_pci_operations;
		dma_set_mask_64bit ( virtio->dma );

		/* Read notification doorbell multiplier */
		pci_read_config_dword ( pci, ( notify.pos +
					       VIRTIO_PCI_CAP_NOTIFY_MULT ),
					&mult );
		virtio->multiplier = mult;
		DBGC ( virtio, "VIRTIO %s using modern interface (mult x%d)\n",
		       virtio->name, virtio->multiplier );

	} else {

		/* Use legacy interface */
		virtio->op = &virtio_legacy_operations;
		common.bar = 0;
		common.offset = 0;
		notify.bar = 0;
		notify.offset = VIRTIO_LEG_DB;
		device.bar = 0;
		device.offset = ( msix ? VIRTIO_LEG_DEV_MSIX :
				  VIRTIO_LEG_DEV );
		DBGC ( virtio, "VIRTIO %s using legacy interface (MSI-X "
		       "%sabled)\n", virtio->name, ( msix ? "en" : "dis" ) );
	}

	/* Map registers */
	virtio->common = virtio_pci_map_cap ( virtio, pci, &common );
	if ( ! virtio->common ) {
		rc = -ENODEV;
		goto err_common;
	}
	virtio->notify = virtio_pci_map_cap ( virtio, pci, &notify );
	if ( ! virtio->notify ) {
		rc = -ENODEV;
		goto err_notify;
	}
	virtio->device = virtio_pci_map_cap ( virtio, pci, &device );
	if ( ! virtio->device ) {
		rc = -ENODEV;
		goto err_device;
	}

	return 0;

	iounmap ( virtio->device );
 err_device:
	iounmap ( virtio->notify );
 err_notify:
	iounmap ( virtio->common );
 err_common:
	return rc;
}

/******************************************************************************
 *
 * Transport-independent operations
 *
 ******************************************************************************
 */

/**
 * Reset device
 *
 * @v virtio		Virtio device
 * @ret rc		Return status code
 */
int virtio_reset ( struct virtio_device *virtio ) {
	int rc;

	/* Clear driver status */
	virtio->stat = 0;

	/* Reset device */
	if ( ( rc = virtio->op->reset ( virtio ) ) != 0 ) {
		DBGC ( virtio, "VIRTIO %s could not reset: %s\n",
		       virtio->name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Report driver status
 *
 * @v virtio		Virtio device
 * @v stat		Additional driver status bits
 * @ret stat		Actual device status
 */
unsigned int virtio_status ( struct virtio_device *virtio,
			     unsigned int stat ) {

	/* Set new driver status bits */
	virtio->stat |= stat;

	/* Report driver status */
	return virtio->op->status ( virtio );
}

/**
 * Negotiate features
 *
 * @v virtio		Virtio device
 * @v driver		Driver supported features
 */
static void virtio_negotiate ( struct virtio_device *virtio,
			       const struct virtio_features *driver ) {
	struct virtio_features *device = &virtio->supported;
	struct virtio_features *features = &virtio->features;
	unsigned int i;

	/* Get device supported features */
	virtio->op->supported ( virtio );

	/* Negotiate mutually supported features */
	for ( i = 0 ; i < VIRTIO_FEATURE_WORDS ; i++ )
		features->word[i] = ( device->word[i] & driver->word[i] );
	virtio->op->negotiate ( virtio );

	/* Show features */
	DBGC ( virtio, "VIRTIO %s features", virtio->name );
	for ( i = 0 ; i < VIRTIO_FEATURE_WORDS ; i++ )
		DBGC ( virtio, "%s%08x", ( i ? ":" : " " ), device->word[i] );
	DBGC ( virtio, " /" );
	for ( i = 0 ; i < VIRTIO_FEATURE_WORDS ; i++ )
		DBGC ( virtio, "%s%08x", ( i ? ":" : " " ), features->word[i] );
	DBGC ( virtio, "\n" );
}

/**
 * Initialise device
 *
 * @v virtio		Virtio device
 * @v driver		Driver supported features
 * @ret rc		Return status code
 */
int virtio_init ( struct virtio_device *virtio,
		  const struct virtio_features *driver ) {
	unsigned int stat;
	int rc;

	/* Reset device */
	if ( ( rc = virtio_reset ( virtio ) ) != 0 )
		goto err_reset;

	/* Acknowledge device existence */
	virtio_status ( virtio, VIRTIO_STAT_ACKNOWLEDGE );

	/* Report driver existence */
	virtio_status ( virtio, VIRTIO_STAT_DRIVER );

	/* Negotiate features */
	virtio_negotiate ( virtio, driver );

	/* Report feature negotiation completion, if applicable */
	if ( virtio->features.word[1] & VIRTIO_FEAT1_MODERN ) {
		stat = virtio_status ( virtio, VIRTIO_STAT_FEATURES_OK );
		if ( ! ( stat & VIRTIO_STAT_FEATURES_OK ) ) {
			DBGC ( virtio, "VIRTIO %s did not accept features\n",
			       virtio->name );
			rc = -ENOTSUP;
			goto err_features;
		}
	}

	return 0;

 err_features:
	virtio_reset ( virtio );
 err_reset:
	virtio_status ( virtio, VIRTIO_STAT_FAIL );
	return rc;
}

/**
 * Enable queue
 *
 * @v virtio		Virtio device
 * @v queue		Virtio queue
 * @v count		Requested queue size
 * @ret rc		Return status code
 */
int virtio_enable ( struct virtio_device *virtio, struct virtio_queue *queue,
		    unsigned int count ) {
	unsigned int offset;
	int rc;

	/* Reset counters */
	queue->prod = 0;
	queue->cons = 0;

	/* Determine queue size */
	virtio->op->size ( virtio, queue, count );
	if ( ( queue->count == 0 ) ||
	     ( queue->count & ( queue->count - 1 ) ) ) {
		DBGC ( virtio, "VIRTIO %s Q%d invalid size %d\n",
		       virtio->name, queue->index, queue->count );
		rc = -ENODEV;
		goto err_count;
	}
	queue->mask = ( queue->count - 1 );

	/* Allocate and initialise queue */
	queue->desc = dma_alloc ( virtio->dma, &queue->map, queue->len,
				  VIRTIO_PAGE );
	if ( ! queue->desc ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	memset ( queue->desc, 0, queue->len );

	/* Enable queue */
	virtio->op->enable ( virtio, queue );
	DBGC ( virtio, "VIRTIO %s Q%d %dx descriptors at [%#08lx,%#08lx)\n",
	       virtio->name, queue->index, queue->count,
	       virt_to_phys ( queue->desc ),
	       ( virt_to_phys ( queue->desc ) +
		 virtio_desc_size ( queue->count ) ) );
	DBGC ( virtio, "VIRTIO %s Q%d %dx submissions at [%#08lx,%#08lx)\n",
	       virtio->name, queue->index, queue->count,
	       virt_to_phys ( queue->sq ),
	       ( virt_to_phys ( queue->sq ) +
		 virtio_sq_size ( queue->count ) ) );
	DBGC ( virtio, "VIRTIO %s Q%d %dx completions at [%#08lx,%#08lx)\n",
	       virtio->name, queue->index, queue->count,
	       virt_to_phys ( queue->cq ),
	       ( virt_to_phys ( queue->cq ) +
		 virtio_cq_size ( queue->count ) ) );

	/* Calculate doorbell register address */
	offset = ( queue->index * virtio->multiplier );
	queue->db = ( virtio->notify + offset );
	DBGC ( virtio, "VIRTIO %s Q%d doorbell at +%#04x\n",
	       virtio->name, queue->index, offset );

	return 0;

	dma_free ( &queue->map, queue->desc, queue->len );
	queue->desc = NULL;
 err_alloc:
 err_count:
	return rc;
}

/**
 * Free queue
 *
 * @v virtio		Virtio device
 * @v queue		Virtio queue
 */
void virtio_free ( struct virtio_device *virtio, struct virtio_queue *queue ) {

	/* Free queue */
	if ( queue->desc ) {
		dma_free ( &queue->map, queue->desc, queue->len );
		queue->desc = NULL;
		DBGC ( virtio, "VIRTIO %s Q%d freed\n",
		       virtio->name, queue->index );
	}
}

/**
 * Unmap device
 *
 * @v virtio		Virtio device
 */
void virtio_unmap ( struct virtio_device *virtio ) {

	/* Unmap device-specific registers */
	iounmap ( virtio->device );

	/* Unmap notification doorbells */
	iounmap ( virtio->notify );

	/* Unmap common registers */
	iounmap ( virtio->common );
}
