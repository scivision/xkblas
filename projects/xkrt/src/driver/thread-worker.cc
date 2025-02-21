/* ************************************************************************** */
/*                                                                            */
/*   thread-worker.cc                                                         */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/21 17:55:01 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/logger/logger.h>
# include <xkrt/driver/thread-worker.hpp>

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
    // LOGGER_DEBUG("New worker thread");
    pthread_mutex_init(&this->sleep.lock, 0);
    pthread_cond_init (&this->sleep.cond, 0);
    this->sleep.sleeping = false;
}

ThreadWorker::~ThreadWorker()
{
    // LOGGER_DEBUG("Delete worker thread");
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
ThreadWorker::pause(void)
{
    assert(ThreadWorker::self() == this);
    pthread_mutex_lock(&this->sleep.lock);
    {
        this->sleep.sleeping = true;
        while (this->sleep.sleeping)
        {
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
    }
    pthread_mutex_unlock(&this->sleep.lock);
}
