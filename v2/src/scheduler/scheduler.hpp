#ifndef __SCHEDULER_HPP__
# define __SCHEDULER_HPP__

# include <atomic>
# include "scheduler/thread-worker.hpp"

typedef struct  xkblas_scheduler_t
{
    /* list of worker threads */
    ThreadWorker * workers[XKBLAS_WORKERS_MAX];

    /* next worker to offload round robin mode */
    std::atomic<uint8_t> round_robin_device_id;

}               xkblas_scheduler_t;

/* enqueue a ready task */
void xkblas_scheduler_enqueue(xkblas_scheduler_t * scheduler, Task * task);

/* register a worker thread */
void xkblas_scheduler_register(xkblas_scheduler_t * scheduler, ThreadWorker * worker, int device_id);

#endif /* __SCHEDULER_HPP__ */
