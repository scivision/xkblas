#ifndef __THREAD_WORKER_HPP__
# define __THREAD_WORKER_HPP__

# include "scheduler/naive-queue.hpp"
# include "scheduler/task.hpp"

# include <stack>

/* maximum number of workers */
# define XKBLAS_WORKERS_MAX 8

/**
 *  This class represents an xkblas user thread, that is producing tasks
 */
class alignas(std::hardware_constructive_interference_size) ThreadWorker
{
    public:

        ////////////////////
        // STATIC MEMBERS //
        ////////////////////

        /* initialize the TLS */
        static void init(void);

        /* deinitialize the TLS */
        static void deinit(void);

        /* retrieve the tls */
        static ThreadWorker * get(void);

        ////////////////////////
        // NON-STATIC MEMBERS //
        ////////////////////////
        ThreadWorker();
        virtual ~ThreadWorker();

        /**
         *  Register a task for later execution
         */
        void push(Task * task);

        /**
         *  Pop the next task to execute by this worker
         */
        Task * pop(void);

    private:

        /* per-thread queue */
        // Deque<Task *, THREAD_WORKER_DEQUE_CAPACITY> queue;
        NaiveQueue<Task *> queue;
};

#endif /* __THREAD_WORKER_HPP__ */
