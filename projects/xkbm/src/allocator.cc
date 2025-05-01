# include <assert.h>
# include <stdlib.h>
# include <unistd.h>
# include <sys/mman.h>

# include <xkbm/allocator.h>

# define USE_MMAP 0

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
xkbm_mem_alloc(size_t size)
{
    # if USE_MMAP
    // TODO : do we need SHARED ?
    void * ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,           -1, 0);
           ptr = mmap( ptr, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
    return ptr;
    # else
    return malloc(size);
    # endif
}

void *
xkbm_alloc_and_touch(const size_t size)
{
    void * ptr = xkbm_mem_alloc(size);
    xkbm_mem_touch(ptr, size);
    return ptr;
}

void
xkbm_free(void * ptr, size_t size)
{
    (void) size;
    # if USE_MMAP
    munmap(ptr, size);
    # else
    free(ptr);
    # endif
}

