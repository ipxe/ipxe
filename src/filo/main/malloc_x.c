#include <etherboot.h>

#include <lib.h>

void *calloc(size_t nmemb, size_t size)
{
    size_t alloc_size = nmemb * size;
    void *mem;

    if (alloc_size < nmemb || alloc_size < size) {
        printf("calloc overflow: %u, %u\n", nmemb, size);
        return 0;
    }

    mem = allot(alloc_size);
    memset(mem, 0, alloc_size);

    return mem;
}

void *realloc(void *mem, size_t size)
{
    size_t copy_size;
    void *new_mem;
        size_t *mark, addr;

    if (mem == 0)
        return allot(size);
    if (size == 0) {
        forget(mem);
        return 0;
    }

        addr = virt_to_phys(mem);
        mark = phys_to_virt(addr - sizeof(size_t));
        copy_size = *mark;

    if (size < copy_size)
        copy_size = size;
    /* XXX should optimze this */
    new_mem = allot(size);
    memcpy(new_mem, mem, copy_size);
    forget(mem);
    return new_mem;
}
