# ifndef __ALLOCATOR_H__
#  define __ALLOCATOR_H__

void   xkbm_mem_touch(void * ptr, size_t size);
void * xkbm_alloc_and_touch(const size_t size);
void * xkbm_mem_alloc(size_t size);
void   xkbm_free(void * ptr, size_t size);

# endif /* __ALLOCATOR_H__ */
