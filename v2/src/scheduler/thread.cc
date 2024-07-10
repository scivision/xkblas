# include "logger/logger.h"
# include "scheduler/thread.hpp"

// The thread local storage
thread_local Thread __TLS;

Thread * get_thread(void)
{
    return &__TLS;
}

// class impl
void Thread::init(void)
{
    XKBLAS_INFO("New thread spawned with tid=%p\n", &__TLS);
}

