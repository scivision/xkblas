# include "logger/logger.h"
# include "scheduler/producer-thread.hpp"

# include <cassert>
# include <cstring>

// The thread local storage
thread_local ThreadWorker * __TLS;

// static members

void
ThreadWorker::init(void)
{
    assert(!__TLS);
    __TLS = new ThreadWorker();
}

void
ThreadWorker::deinit(void)
{
    assert(__TLS);
    delete __TLS;
}

void
ThreadWorker::get(void)
{
    return __TLS;
}

// non-static members

ThreadWorker::ThreadWorker() : queue()
{
    XKBLAS_INFO("New worker thread");
}

ThreadWorker::~ThreadWorker()
{
    XKBLAS_INFO("Delete worker thread");
}

void
ThreadWorker::push(Task * task)
{
    this->queue->push(task);
}
