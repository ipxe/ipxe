#include "etherboot.h"

size_t heap_ptr, heap_top, heap_bot;

void init_heap(void)
{
	size_t size;
	size_t start, end;
	unsigned i;
	/* Find the largest contiguous area of memory that
	 * I can use for the heap, which is organized as 
	 * a stack that grows backwards through memory.
	 */

	/* If I have virtual address that do not equal physical addresses
	 * there is a change I will try to use memory from both sides of
	 * the virtual address space simultaneously, which can cause all kinds
	 * of interesting problems.
	 * Avoid it by logically extending etherboot.  Once I know that relocation
	 * works I can just start the virtual address space at 0, and this problem goes
	 * away so that is probably a better solution.
	 */
#if 0
	start = virt_to_phys(_text);
#else
	/* segment wrap around is nasty don't chance it. */
	start = virt_to_phys(_virt_start);
#endif
	end  = virt_to_phys(_end);
	size = 0;
	for(i = 0; i < meminfo.map_count; i++) {
		unsigned long r_start, r_end;
		if (meminfo.map[i].type != E820_RAM)
			continue;
		if (meminfo.map[i].addr > ULONG_MAX)
			continue;
		if (meminfo.map[i].size > ULONG_MAX)
			continue;
		
		r_start = meminfo.map[i].addr;
		r_end = r_start + meminfo.map[i].size;
		if (r_end < r_start) {
			r_end = ULONG_MAX;
		}
		/* Handle areas that overlap etherboot */
		if ((end > r_start) && (start < r_end)) {
			/* Etherboot completely covers the region */
			if ((start <= r_start) && (end >= r_end))
				continue;
			/* Etherboot is completely contained in the region */
			if ((start > r_start) && (end < r_end)) {
				/* keep the larger piece */
				if ((r_end - end) >= (r_start - start)) {
					r_start = end;
				}
				else {
					r_end = start;
				}
			}
			/* Etherboot covers one end of the region.
			 * Shrink the region.
			 */
			else if (end >= r_end) {
				r_end = start;
			}
			else if (start <= r_start) {
				r_start = end;
			}
		}
		/* If two areas are the size prefer the greater address */
		if (((r_end - r_start) > size) ||
			(((r_end - r_start) == size) && (r_start > heap_top))) {
			size = r_end - r_start;
			heap_top = r_start;
			heap_bot = r_end;
		}
	}
	if (size == 0) {
		printf("init_heap: No heap found.\n");
		exit(1);
	}
	heap_ptr = heap_bot;
}

void *allot(size_t size)
{
	void *ptr;
	size_t *mark, addr;
	/* Get an 16 byte aligned chunk of memory off of the heap 
	 * An extra sizeof(size_t) bytes is allocated to track
	 * the size of the object allocated on the heap.
	 */
	addr = (heap_ptr - (size + sizeof(size_t))) &  ~15;
	if (addr < heap_top) {
		ptr = 0;
	} else {
		mark = phys_to_virt(addr);
		*mark = size;
		heap_ptr = addr;
		ptr = phys_to_virt(addr + sizeof(size_t));
	}
	return ptr;
}

//if mask = 0xf, it will be 16 byte aligned
//if mask = 0xff, it will be 256 byte aligned
//For DMA memory allocation, because it has more reqiurement on alignment
void *allot2(size_t size, uint32_t mask)
{
        void *ptr;
        size_t *mark, addr;
	uint32_t *mark1;
        
	addr = ((heap_ptr - size ) &  ~mask) - sizeof(size_t) - sizeof(uint32_t);
        if (addr < heap_top) {
                ptr = 0;        
        } else {        
                mark = phys_to_virt(addr);
                *mark = size;  
		mark1 = phys_to_virt(addr+sizeof(size_t));
		*mark1 = mask; 
                heap_ptr = addr;
                ptr = phys_to_virt(addr + sizeof(size_t) + sizeof(uint32_t));
        }                       
        return ptr;             
}  

void forget(void *ptr)
{
	size_t *mark, addr;
	size_t size;

	if (!ptr) {
		return;
	}
	addr = virt_to_phys(ptr);
	mark = phys_to_virt(addr - sizeof(size_t));
	size = *mark;
	addr += (size + 15) & ~15;
	
	if (addr > heap_bot) {
		addr = heap_bot;
	}
	heap_ptr = addr;
}

void forget2(void *ptr)
{
        size_t *mark, addr;
        size_t size;
	uint32_t mask;
	uint32_t *mark1;

        if (!ptr) {
                return;
        }
        addr = virt_to_phys(ptr);
        mark = phys_to_virt(addr - sizeof(size_t) - sizeof(uint32_t));
        size = *mark;
	mark1 = phys_to_virt(addr - sizeof(uint32_t));
	mask = *mark1;
        addr += (size + mask) & ~mask;

        if (addr > heap_bot) {
                addr = heap_bot;
        }
        heap_ptr = addr;
}
