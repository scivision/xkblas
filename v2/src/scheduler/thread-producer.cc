# include "logger/logger.h"
# include "scheduler/producer-thread.hpp"

# include <cassert>
# include <cstring>

// The thread local storage
thread_local ThreadProducer * __TLS;

// static members

void
ThreadProducer::init(void)
{
    assert(!__TLS);
    __TLS = new ThreadProducer();
}

void
ThreadProducer::deinit(void)
{
    assert(__TLS);
    delete __TLS;
}

ThreadProducer *
ThreadProducer::get(void)
{
    assert(__TLS);
    return __TLS;
}

// non-static members

ThreadProducer::ThreadProducer() :
    memory_stack_bottom(),
    memory_stack_ptr(memory_stack_bottom),
    queue()
{
    XKBLAS_INFO("New producer thread");
    memset(this->memory_stack_bottom, 0, THREAD_MAX_MEMORY);
}

ThreadProducer::~ThreadProducer()
{
    XKBLAS_INFO("Delete producer thread");
}

uint8_t *
ThreadProducer::allocate(uint64_t size)
{
    assert(this->memory_stack_ptr < this->memory_stack_bottom + THREAD_MAX_MEMORY);
    uint8_t * memory = this->memory_stack_ptr;
    this->memory_stack_ptr += size;
    return memory;
}

void
ThreadProducer::deallocate_all(void)
{
    this->memory_stack_ptr = this->memory_stack_bottom;
}
