# include "logger/logger.h"
# include "device/thread-producer.hpp"

# include <cassert>
# include <cstring>

// The thread local storage
thread_local ThreadProducer * __TLS_PRODUCER;

// static members

void
ThreadProducer::init(void)
{
    assert(!__TLS_PRODUCER);
    __TLS_PRODUCER = new ThreadProducer();
}

void
ThreadProducer::deinit(void)
{
    assert(__TLS_PRODUCER);
    delete __TLS_PRODUCER;
}

ThreadProducer *
ThreadProducer::get(void)
{
    assert(__TLS_PRODUCER);
    return __TLS_PRODUCER;
}

// non-static members

ThreadProducer::ThreadProducer() :
    memory_stack_bottom(),
    memory_stack_ptr(memory_stack_bottom)
{
    // XKBLAS_DEBUG("New producer thread");
    memset(this->memory_stack_bottom, 0, THREAD_PRODUCER_MAX_MEMORY);
}

ThreadProducer::~ThreadProducer()
{
    XKBLAS_DEBUG("Delete producer thread");
}

uint8_t *
ThreadProducer::allocate(uint64_t size)
{
    assert(this->memory_stack_ptr < this->memory_stack_bottom + THREAD_PRODUCER_MAX_MEMORY);
    uint8_t * memory = this->memory_stack_ptr;
    this->memory_stack_ptr += size;
    return memory;
}

void
ThreadProducer::deallocate_all(void)
{
    this->memory_stack_ptr = this->memory_stack_bottom;
}
