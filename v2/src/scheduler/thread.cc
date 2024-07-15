# include "logger/logger.h"
# include "scheduler/thread.hpp"

# include <cassert>
# include <cstring>

// The thread local storage
thread_local Thread * __TLS;

// static members

void
Thread::init(void)
{
    assert(!__TLS);
    __TLS = new Thread();
}

void
Thread::deinit(void)
{
    assert(__TLS);
    delete __TLS;
}

Thread *
Thread::get(void)
{
    return __TLS;
}

// non-static members

Thread::Thread() :
    memory_stack_bottom(),
    memory_stack_ptr(memory_stack_bottom),
    deque()
{
    XKBLAS_INFO("New Xkblas thread");
    memset(this->memory_stack_bottom, 0, THREAD_MAX_MEMORY);
}

Thread::~Thread()
{
    XKBLAS_INFO("Delete Xkblas thread");
}

uint8_t *
Thread::allocate(uint64_t size)
{
    assert(this->memory_stack_ptr < this->memory_stack_bottom + THREAD_MAX_MEMORY);
    uint8_t * memory = this->memory_stack_ptr;
    this->memory_stack_ptr += size;
    return memory;
}

void
Thread::deallocate_all(void)
{
    this->memory_stack_ptr = this->memory_stack_bottom;
}

void
Thread::submit(Task * task)
{
    // TODO
}
