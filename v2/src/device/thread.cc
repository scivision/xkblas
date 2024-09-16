# include "device/thread.hpp"
# include "logger/logger.h"

# include <cassert>
# include <cstring>

Thread::Thread()
{
    this->memory_stack_bottom   = (uint8_t *) malloc(THREAD_MAX_MEMORY);
    this->memory_stack_ptr      = this->memory_stack_bottom;

    assert(this->memory_stack_bottom);
    memset(this->memory_stack_bottom, 0, THREAD_MAX_MEMORY);
}

Thread::~Thread()
{
}

uint8_t *
Thread::allocate(uint64_t size)
{
    # if 0
    if (this->memory_stack_ptr >= this->memory_stack_bottom + THREAD_MAX_MEMORY)
        XKBLAS_FATAL("Stack overflow ! Increase `THREAD_MAX_MEMORY` and recompile");
    uint8_t * memory = this->memory_stack_ptr;
    this->memory_stack_ptr += size;
    return memory;
    # else
    return (uint8_t *) malloc(size);
    # endif
}

void
Thread::deallocate_all(void)
{
    this->memory_stack_ptr = this->memory_stack_bottom;
}
