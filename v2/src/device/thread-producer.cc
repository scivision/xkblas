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
ThreadProducer::self(void)
{
    if (__TLS_PRODUCER == NULL)
        ThreadProducer::init();
    assert(__TLS_PRODUCER);
    return __TLS_PRODUCER;
}

# ifndef NDEBUG
void
xkblas_thread_report_tasks(void)
{
    ThreadProducer * producer = ThreadProducer::self();
    assert(producer);

    int summary[TASK_STATE_MAX];
    memset(summary, 0, sizeof(summary));

    for (size_t i = 0 ; i < producer->tasks.size() ; ++i)
    {
        Task * task = producer->tasks[i];
        assert(task);

        XKBLAS_WARN(
            "%4lu - %12s - wc=%u - %s",
            i, task_state_to_str(task->state.value), task->wc.load(), task->label
        );

        ++summary[task->state.value];
    }

    XKBLAS_WARN("Summary");
    for (int i = 0 ; i < TASK_STATE_MAX ; ++i)
        XKBLAS_WARN("  %12s: %6d", task_state_to_str((task_state_t)i), summary[i]);
}

# endif /* NDEBUG */
