#ifndef ETHERBOOT_IO_H
#define ETHERBOOT_IO_H

/* Don't require identity mapped physical memory,
 * osloader.c is the only valid user at the moment.
 */
static inline unsigned long virt_to_phys(volatile const void *virt_addr)
{
	return ((unsigned long)virt_addr);
}

static inline void *phys_to_virt(unsigned long phys_addr)
{
	return (void *)(phys_addr);
}

/* virt_to_bus converts an addresss inside of etherboot [_start, _end]
 * into a memory address cards can use.
 */
#define virt_to_bus virt_to_phys


/* bus_to_virt reverses virt_to_bus, the address must be output
 * from virt_to_bus to be valid.  This function does not work on
 * all bus addresses.
 */
#define bus_to_virt phys_to_virt

/* ioremap converts a random 32bit bus address into something
 * etherboot can access.
 */
static inline void *ioremap(unsigned long bus_addr, unsigned long length __unused)
{
	return bus_to_virt(bus_addr);
}

/* iounmap cleans up anything ioremap had to setup */
static inline void iounmap(void *virt_addr __unused)
{
	return;
}

/* In physical mode the offset of uncached pages */
#define PHYS_BASE (0x8000000000000000UL)

/* Memory mapped IO primitives, we avoid the cache... */
static inline uint8_t readb(unsigned long addr)
{
	return *((volatile uint8_t *)(PHYS_BASE | addr));
}

static inline uint16_t readw(unsigned long addr)
{
	return *((volatile uint16_t *)(PHYS_BASE | addr));
}

static inline uint32_t readl(unsigned long addr)
{
	return *((volatile uint32_t *)(PHYS_BASE | addr));
}

static inline uint64_t readq(unsigned long addr)
{
	return *((volatile uint64_t *)(PHYS_BASE | addr));
}


static inline void writeb(uint8_t val, unsigned long addr)
{
	*((volatile uint8_t *)(PHYS_BASE | addr)) = val;
}

static inline void writew(uint16_t val, unsigned long addr)
{
	*((volatile uint16_t *)(PHYS_BASE | addr)) = val;
}

static inline void writel(uint32_t val, unsigned long addr)
{
	*((volatile uint32_t *)(PHYS_BASE | addr)) = val;
}

static inline void writeq(uint64_t val, unsigned long addr)
{
	*((volatile uint64_t *)(PHYS_BASE | addr)) = val;
}


static inline void memcpy_fromio(void *dest, unsigned long src, size_t n)
{
	size_t i;
	uint8_t *dp = dest;
	for(i = 0; i < n; i++) {
		*dp = readb(src);
		dp++;
		src++;
	}
}

static inline void memcpy_toio(unsigned long dest , const void *src, size_t n)
{
	size_t i;
	const uint8_t *sp = src;
	for(i = 0; i < n; i++) {
		writeb(*sp, dest);
		sp++;
		dest++;
	}
}

/* IO space IO primitives, Itanium has a strange architectural mapping... */
extern unsigned long io_base;
#define __ia64_mf_a()	__asm__ __volatile__ ("mf.a" ::: "memory")
#define __ia64_io_addr(port) ((void *)(PHYS_BASE | io_base | (((port) >> 2) << 12) | ((port) & 0xfff)))

static inline uint8_t inb(unsigned long port)
{
	uint8_t result;
	
	result = *((volatile uint8_t *)__ia64_io_addr(port));
	__ia64_mf_a();
	return result;
}

static inline uint16_t inw(unsigned long port)
{
	uint8_t result;
	result = *((volatile uint16_t *)__ia64_io_addr(port));
	__ia64_mf_a();
	return result;
}

static inline uint32_t inl(unsigned long port)
{
	uint32_t result;
	result = *((volatile uint32_t *)__ia64_io_addr(port));
	__ia64_mf_a();
	return result;
}

static inline void outb(uint8_t val, unsigned long port)
{
	*((volatile uint8_t *)__ia64_io_addr(port)) = val;
	__ia64_mf_a();
}

static inline void outw(uint16_t val, unsigned long port)
{
	*((volatile uint16_t *)__ia64_io_addr(port)) = val;
	__ia64_mf_a();
}

static inline void outl(uint32_t val, unsigned long port)
{
	*((volatile uint32_t *)__ia64_io_addr(port)) = val;
	__ia64_mf_a();
}



static inline void insb(unsigned long port, void *dst, unsigned long count)
{
	volatile uint8_t  *addr = __ia64_io_addr(port);
	uint8_t *dp = dst;
	__ia64_mf_a();
	while(count--) 
		*dp++ = *addr;
	__ia64_mf_a();
}

static inline void insw(unsigned long port, void *dst, unsigned long count)
{
	volatile uint16_t  *addr = __ia64_io_addr(port);
	uint16_t *dp = dst;
	__ia64_mf_a();
	while(count--) 
		*dp++ = *addr;
	__ia64_mf_a();
}

static inline void insl(unsigned long port, void *dst, unsigned long count)
{
	volatile uint32_t  *addr = __ia64_io_addr(port);
	uint32_t *dp = dst;
	__ia64_mf_a();
	while(count--) 
		*dp++ = *addr;
	__ia64_mf_a();
}

static inline void outsb(unsigned long port, void *src, unsigned long count)
{
	const uint8_t *sp = src;
	volatile uint8_t *addr = __ia64_io_addr(port);

	while (count--)
		*addr = *sp++;
	__ia64_mf_a();
}

static inline void outsw(unsigned long port, void *src, unsigned long count)
{
	const uint16_t *sp = src;
	volatile uint16_t *addr = __ia64_io_addr(port);

	while (count--)
		*addr = *sp++;
	__ia64_mf_a();
}

static inline void outsl(unsigned long port, void *src, unsigned long count)
{
	const uint32_t *sp = src;
	volatile uint32_t *addr = __ia64_io_addr(port);

	while (count--)
		*addr = *sp++;
	__ia64_mf_a();
}

static inline unsigned long ia64_get_kr0(void)
{
	unsigned long r;
	asm volatile ("mov %0=ar.k0" : "=r"(r));
	return r;
}

#endif /* ETHERBOOT_IO_H */
