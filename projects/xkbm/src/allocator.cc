# include <assert.h>
# include <stdlib.h>
# include <unistd.h>

# include <xkbm/allocator.h>

typedef char byte;

void
xkbm_mem_touch(void * ptr, size_t size)
{
    byte * bytes = (byte *) ptr;
    size_t pagesize = (size_t) getpagesize();
    for (size_t i = 0 ; i < size ; i += pagesize)
        bytes[i] = 42;
}

void *
xkbm_alloc_and_touch(const size_t size)
{
    void * ptr = malloc(size);
    assert(ptr);
    xkbm_mem_touch(ptr, size);
    return ptr;
}

void
xkbm_free(void * ptr)
{
    free(ptr);
}

