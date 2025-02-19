/* ************************************************************************** */
/*                                                                            */
/*   thread-worker.hpp                                                        */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/19 00:31:19 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __THREAD_WORKER_HPP__
# define __THREAD_WORKER_HPP__

# include <xkrt/driver/naive-queue.hpp>
# include <xkrt/task/task.hpp>
# include <xkrt/driver/thread.hpp>
# include <xkrt/memory/cache-line-size.hpp>

/* maximum number of workers */
# define XKRT_WORKERS_MAX 8

/**
 *  This class represents an ptr user thread, that is producing tasks
 */
class alignas(CACHE_LINE_SIZE) ThreadWorker : public Thread
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
        static ThreadWorker * self(void);

        ////////////////////////
        // NON-STATIC MEMBERS //
        ////////////////////////
        ThreadWorker();
        virtual ~ThreadWorker();

        /* Sleep the thread until signaled */
        void pause(void);

        /* Wake up the thread */
        void wakeup(void);

        /* push a task */
        void push(Task * const & task);

        /* pop a task */
        Task * pop(void);

        /**
         * Complete the given task, that is:
         *  - callback with any successors that are now ready
         *  - then move the wait counter
         */
        template <void (*callback)(void * vargs, Task * task)>
        void
        complete(void * vargs, Task * task)
        {
            task->complete<callback>(vargs);    /* this may 'push' tasks, incrementing 'wc' */
            writemem_barrier();                 /* membarrier to avoid 'wc' being decremented before the previous line increment */
            ThreadWorker::move_wc(-1);
        }

        /* return true if the wait counter is '0' */
        bool completed(void) const;

        static inline void
        move_wc(int32_t offset)
        {
            ThreadWorker::self()->wc += offset;
        }

    public:

        /* number of uncompleted tasks */
        int32_t wc;

        /* the cpuset of that worker */
        cpu_set_t cpuset;

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
