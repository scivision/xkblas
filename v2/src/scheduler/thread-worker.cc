# include "logger/logger.h"
# include "scheduler/thread-worker.hpp"

# include <cassert>
# include <cstring>

// The thread local storage
thread_local ThreadWorker * __TLS_WORKER;

// static members

void
ThreadWorker::init(void)
{
    assert(!__TLS_WORKER);
    __TLS_WORKER = new ThreadWorker();
}

void
ThreadWorker::deinit(void)
{
    assert(__TLS_WORKER);
    delete __TLS_WORKER;
}

ThreadWorker *
ThreadWorker::get(void)
{
    return __TLS_WORKER;
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
    this->queue.push(task);
}

Task *
ThreadWorker::pop(void)
{
    return this->queue.pop();
}
