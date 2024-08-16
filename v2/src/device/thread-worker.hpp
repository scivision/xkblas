#ifndef __THREAD_WORKER_HPP__
# define __THREAD_WORKER_HPP__

# include "device/naive-queue.hpp"
# include "device/task.hpp"
# include "sync/cache-line-size.hpp"

# include <stack>

/* maximum number of workers */
# define XKBLAS_WORKERS_MAX 8

/**
 *  This class represents an xkblas user thread, that is producing tasks
 */
class alignas(CACHE_LINE_SIZE) ThreadWorker
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

        /**
         *  Sleep the thread until signaled
         */
        void pause(void);

        /**
         *  Wake up the thread
         */
        void wakeup(void);

    private:

        /* per-thread queue */
        // Deque<Task *, THREAD_WORKER_DEQUE_CAPACITY> queue;
        NaiveQueue<Task *> queue;

        /* lock and condition to sleep the mutex */
        struct {
            pthread_mutex_t lock;
            pthread_cond_t  cond;
            volatile bool   sleeping;
        } sleep;
};

#endif /* __THREAD_WORKER_HPP__ */
