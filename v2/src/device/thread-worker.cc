# include "logger/logger.h"
# include "device/thread-worker.hpp"

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
    pthread_mutex_init(&this->sleep.lock, 0);
    pthread_cond_init (&this->sleep.cond, 0);
    this->sleep.sleeping = false;
}

ThreadWorker::~ThreadWorker()
{
    XKBLAS_INFO("Delete worker thread");
}

void
ThreadWorker::push(Task * task)
{
    this->queue.push(task);
    this->wakeup();
}

Task *
ThreadWorker::pop(void)
{
    return this->queue.pop();
}

void
ThreadWorker::pause(void)
{
    pthread_mutex_lock(&this->sleep.lock);
    {
        this->sleep.sleeping = true;
        while (this->sleep.sleeping)
            pthread_cond_wait(&this->sleep.cond, &this->sleep.lock);
    }
    pthread_mutex_unlock(&this->sleep.lock);
}

void
ThreadWorker::wakeup(void)
{
    pthread_mutex_lock(&this->sleep.lock);
    if (this->sleep.sleeping)
    {
        this->sleep.sleeping = false;
        pthread_cond_signal(&this->sleep.cond);
    }
    pthread_mutex_unlock(&this->sleep.lock);
}
