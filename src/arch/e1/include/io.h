#ifndef ETHERBOOT_IO_H
#define ETHERBOOT_IO_H

/* Don't require identity mapped physical memory,
 * osloader.c is the only valid user at the moment.
 */
#if 0
static inline unsigned long virt_to_phys(volatile const void *virt_addr)
{
	return ((unsigned long)virt_addr);
}
#else
#define virt_to_phys(vaddr)	((unsigned long) (vaddr))
#endif

#if 0
static inline void *phys_to_virt(unsigned long phys_addr)
{
	return (void *)(phys_addr);
}
#else
#define phys_to_virt(vaddr)	((void *) (vaddr))
#endif

/* virt_to_bus converts an addresss inside of etherboot [_start, _end]
 * into a memory address cards can use.
 */
#define virt_to_bus virt_to_phys

/* bus_to_virt reverses virt_to_bus, the address must be output
 * from virt_to_bus to be valid.  This function does not work on
 * all bus addresses.
 */
#define bus_to_virt phys_to_virt

#define iounmap(addr)				((void)0)
#define ioremap(physaddr, size)			(physaddr)

#define IORegAddress	13
#define IOWait			11
#define IOSetupTime		8
#define IOAccessTime	5
#define IOHoldTime		3

#define SLOW_IO_ACCESS ( 0x3 << IOSetupTime | 0x0 << IOWait | 7 << IOAccessTime | 3 << IOHoldTime )

/* The development board can generate up to 15 Chip selects */
#define NR_CS	16

extern unsigned int io_periph[NR_CS];
#define ETHERNET_CS 4

static inline unsigned short _swapw(volatile unsigned short v)
{
    return ((v << 8) | (v >> 8));
}

static inline unsigned int _swapl(volatile unsigned long v)
{
    return ((v << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | (v >> 24));
}

#define  hy_inpw(addr)  						\
	({   register unsigned long dummy, dummy1; 			\
		 dummy  = addr; 						\
		 asm volatile  ("LDW.IOD   %1, %0, 0" 	\
				 		: "=l" (dummy1) 		\
					   	: "l" (dummy)); dummy1; })


#define  hy_outpw(x, addr)						  \
	({   register unsigned long dummy0,dummy1; \
	 	 dummy0 = addr; 					  \
		 dummy1 = x;						  \
		 asm volatile  ("STW.IOD   %1, %0, 0" \
						 : "=l" (dummy1) 	  \
						 : "l"(dummy0), "l" (dummy1)); dummy1; })

#define readb(addr)	({ unsigned char  __v = inregb(addr); __v; })
#define readw(addr)	({ unsigned short __v = inregw(addr); __v; })
#define readl(addr)	({ unsigned long  __v = inregl(addr); __v; })

#define writeb(b,addr) (void)(outreg(b, addr))
#define writew(b,addr) (void)(outreg(b, addr))
#define writel(b,addr) (void)(outreg(b, addr))

static inline unsigned long common_io_access(unsigned long addr)
{
	return io_periph[(addr & 0x03C00000) >> 22];
}

static inline volatile unsigned char inregb(volatile unsigned long reg)
{
	unsigned char val;

	val = hy_inpw(common_io_access(reg) | ((0xf & reg) << IORegAddress)); 
	return val;
}

static inline volatile unsigned short inregw(volatile unsigned long reg)
{
	unsigned short val;

	val = hy_inpw(common_io_access(reg) | ((0xf & reg) << IORegAddress)); 
	return val;
}

static inline volatile unsigned long inregl(volatile unsigned long reg)
{
	unsigned long val;

	val = hy_inpw(common_io_access(reg) | ((0xf & reg) << IORegAddress)); 
	return val;
}

static inline void outreg(volatile unsigned long val, volatile unsigned long reg)
{
		
	hy_outpw(val, (common_io_access(reg) | ((0xf & reg) << IORegAddress)));
}

static inline void io_outsb(unsigned int addr, void *buf, int len)
{
	unsigned long tmp;
	unsigned char *bp = (unsigned char *) buf;

	tmp = (common_io_access(addr)) | ((0xf & addr) << IORegAddress);

	while (len--){
		hy_outpw(_swapw(*bp++), tmp);
	}
}

static inline void io_outsw(volatile unsigned int addr, void *buf, int len)
{
	unsigned long tmp;
	unsigned short *bp = (unsigned short *) buf;
	
	tmp = (common_io_access(addr)) | ((0xf & addr) << IORegAddress);

	while (len--){
		hy_outpw(_swapw(*bp++), tmp);
	}
}

static inline void io_outsl(volatile unsigned int addr, void *buf, int len)
{
	unsigned long tmp;
	unsigned int *bp = (unsigned int *) buf;
		
	tmp = (common_io_access(addr)) | ((0xf & addr) << IORegAddress);

	while (len--){
		hy_outpw(_swapl(*bp++), tmp);
	}
}

static inline void io_insb(volatile unsigned int addr, void *buf, int len)
{
	unsigned long tmp;
	unsigned char *bp = (unsigned char *) buf;

	tmp = (common_io_access(addr)) | ((0xf & addr) << IORegAddress);

	while (len--)
		*bp++ = hy_inpw((unsigned char) tmp);
	
}

static inline void io_insw(unsigned int addr, void *buf, int len)
{
	unsigned long tmp;
	unsigned short *bp = (unsigned short *) buf;

	tmp = (common_io_access(addr)) | ((0xf & addr) << IORegAddress);

	while (len--)
		*bp++ = _swapw((unsigned short)hy_inpw(tmp));

}

static inline void io_insl(unsigned int addr, void *buf, int len)
{
	unsigned long tmp;
	unsigned int *bp = (unsigned int *) buf;

	tmp = (common_io_access(addr)) | ((0xf & addr) << IORegAddress);

	while (len--)
		*bp++ = _swapl((unsigned int)hy_inpw(tmp));
}

#define inb(addr)    readb(addr)
#define inw(addr)    readw(addr)
#define inl(addr)    readl(addr)
#define outb(x,addr) ((void) writeb(x,addr))
#define outw(x,addr) ((void) writew(x,addr))
#define outl(x,addr) ((void) writel(x,addr))

#define insb(a,b,l) io_insb(a,b,l)
#define insw(a,b,l) io_insw(a,b,l)
#define insl(a,b,l) io_insl(a,b,l)
#define outsb(a,b,l) io_outsb(a,b,l)
#define outsw(a,b,l) io_outsw(a,b,l)
#define outsl(a,b,l) io_outsl(a,b,l)
	
#define memcpy_fromio(a,b,c)	memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)	memcpy((void *)(a),(b),(c))

#endif /* ETHERBOOT_IO_H */
