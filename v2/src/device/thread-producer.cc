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
