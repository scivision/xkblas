#ifndef __WORKER_THREAD_HPP__
# define __WORKER_THREAD_HPP__

# include "scheduler/naive-queue.hpp"
# include "scheduler/task.hpp"

# include <stack>

/**
 *  This class represents an xkblas user thread, that is producing tasks
 */
class alignas(std::hardware_constructive_interference_size) ThreadWorker
{
    public:

        ////////////////////
        // STATIC MEMBERS //
        ////////////////////

        /* retrieve the tls */
        static ThreadWorker * get(void);

        ////////////////////////
        // NON-STATIC MEMBERS //
        ////////////////////////
        ThreadWorker();
        virtual ~ThreadWorker();

        /**
         *  Push a task to be executed by this worker
         */
        void push(Task * task);

    private:

        /* per-thread queue */
        // Deque<Task *, WORKER_THREAD_DEQUE_CAPACITY> queue;
        NaiveQueue<Task *> queue;
};

#endif /* __WORKER_THREAD_HPP__ */
