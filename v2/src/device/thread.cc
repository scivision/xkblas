# include "device/thread.hpp"

uint8_t *
ThreadProducer::allocate(uint64_t size)
{
    #ifndef NDEBUG
    if (this->memory_stack_ptr >= this->memory_stack_bottom + THREAD_MAX_MEMORY)
        XKBLAS_FATAL("Stack overflow ! Increase `THREAD_MAX_MEMORY` and recompile");
    # endif /* NDEBUG */
    uint8_t * memory = this->memory_stack_ptr;
    this->memory_stack_ptr += size;
    return memory;
}

void
ThreadProducer::deallocate_all(void)
{
    this->memory_stack_ptr = this->memory_stack_bottom;
}
