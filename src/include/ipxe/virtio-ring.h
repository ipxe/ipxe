#ifndef _VIRTIO_RING_H_
# define _VIRTIO_RING_H_

#include <ipxe/virtio-pci.h>
#include <ipxe/dma.h>

/* Status byte for guest to report progress, and synchronize features. */
/* We have seen device and processed generic fields (VIRTIO_CONFIG_F_VIRTIO) */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE     1
/* We have found a driver for the device. */
#define VIRTIO_CONFIG_S_DRIVER          2
/* Driver has used its parts of the config, and is happy */
#define VIRTIO_CONFIG_S_DRIVER_OK       4
/* Driver has finished configuring features */
#define VIRTIO_CONFIG_S_FEATURES_OK     8
/* We've given up on this device. */
#define VIRTIO_CONFIG_S_FAILED          0x80

/* Virtio feature flags used to negotiate device and driver features. */
/* Can the device handle any descriptor layout? */
#define VIRTIO_F_ANY_LAYOUT             27
/* v1.0 compliant. */
#define VIRTIO_F_VERSION_1              32
#define VIRTIO_F_IOMMU_PLATFORM         33

#define MAX_QUEUE_NUM      (256)

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

#define VRING_AVAIL_F_NO_INTERRUPT 1

#define VRING_USED_F_NO_NOTIFY     1

struct vring_desc
{
   u64 addr;
   u32 len;
   u16 flags;
   u16 next;
};

struct vring_avail
{
   u16 flags;
   u16 idx;
   u16 ring[0];
};

struct vring_used_elem
{
   u32 id;
   u32 len;
};

struct vring_used
{
   u16 flags;
   u16 idx;
   struct vring_used_elem ring[];
};

struct vring {
   unsigned int num;
   struct vring_desc *desc;
   struct vring_avail *avail;
   struct vring_used *used;
};

#define vring_size(num) \
   (((((sizeof(struct vring_desc) * num) + \
      (sizeof(struct vring_avail) + sizeof(u16) * num)) \
         + PAGE_MASK) & ~PAGE_MASK) + \
         (sizeof(struct vring_used) + sizeof(struct vring_used_elem) * num))

struct vring_virtqueue {
   unsigned char *queue;
   size_t queue_size;
   struct dma_mapping map;
   struct dma_device *dma;
   struct vring vring;
   u16 free_head;
   u16 last_used_idx;
   void **vdata;
   struct virtio_net_hdr_modern *empty_header;
   /* PCI */
   int queue_index;
   struct virtio_pci_region notification;
};

struct vring_list {
  physaddr_t addr;
  unsigned int length;
};

static inline void vring_init(struct vring *vr,
                         unsigned int num, unsigned char *queue)
{
   unsigned int i;
   unsigned long pa;

   vr->num = num;

   /* physical address of desc must be page aligned */

   pa = virt_to_phys(queue);
   pa = (pa + PAGE_MASK) & ~PAGE_MASK;
   vr->desc = phys_to_virt(pa);

   vr->avail = (struct vring_avail *)&vr->desc[num];

   /* physical address of used must be page aligned */

   pa = virt_to_phys(&vr->avail->ring[num]);
   pa = (pa + PAGE_MASK) & ~PAGE_MASK;
   vr->used = phys_to_virt(pa);

   for (i = 0; i < num - 1; i++)
           vr->desc[i].next = i + 1;
   vr->desc[i].next = 0;
}

static inline void vring_enable_cb(struct vring_virtqueue *vq)
{
   vq->vring.avail->flags &= ~VRING_AVAIL_F_NO_INTERRUPT;
}

static inline void vring_disable_cb(struct vring_virtqueue *vq)
{
   vq->vring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;
}


/*
 * vring_more_used
 *
 * is there some used buffers ?
 *
 */

static inline int vring_more_used(struct vring_virtqueue *vq)
{
   wmb();
   return vq->last_used_idx != vq->vring.used->idx;
}

void vring_detach(struct vring_virtqueue *vq, unsigned int head);
void *vring_get_buf(struct vring_virtqueue *vq, unsigned int *len);
void vring_add_buf(struct vring_virtqueue *vq, struct vring_list list[],
                   unsigned int out, unsigned int in,
                   void *index, int num_added);
void vring_kick(struct virtio_pci_modern_device *vdev, unsigned int ioaddr,
                struct vring_virtqueue *vq, int num_added);

#endif /* _VIRTIO_RING_H_ */
