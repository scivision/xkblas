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
ThreadWorker::self(void)
{
    if (__TLS_WORKER == NULL)
        ThreadWorker::init();
    assert(__TLS_WORKER);
    return __TLS_WORKER;
}

// non-static members

ThreadWorker::ThreadWorker() : queue(), wc(0)
{
    // XKBLAS_DEBUG("New worker thread");
    pthread_mutex_init(&this->sleep.lock, 0);
    pthread_cond_init (&this->sleep.cond, 0);
    this->sleep.sleeping = false;
}

ThreadWorker::~ThreadWorker()
{
    // XKBLAS_DEBUG("Delete worker thread");
}

void
ThreadWorker::push(Task * const & task)
{
    ThreadWorker::move_wc(1);
    this->queue.push(task);
    this->wakeup();
}

Task *
ThreadWorker::pop(void)
{
    /* this is true as we only have 1 worker per device currently */
    assert(ThreadWorker::self() == this);
    return this->queue.pop();
}

void
ThreadWorker::complete(Task * task)
{
    task->complete();           /* this may 'push' tasks, incrementing 'wc' */
    writemem_barrier();         /* membarrier to avoid 'wc' being decremented before the previous line increment */
    ThreadWorker::move_wc(-1);
}

void
ThreadWorker::pause(void)
{
    assert(ThreadWorker::self() == this);
    pthread_mutex_lock(&this->sleep.lock);
    {
        this->sleep.sleeping = true;
        while (this->sleep.sleeping)
        {
            XKBLAS_DEBUG("Thread paused");
            pthread_cond_wait(&this->sleep.cond, &this->sleep.lock);
        }
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
        XKBLAS_DEBUG("Thread woke up");
    }
    pthread_mutex_unlock(&this->sleep.lock);
}
