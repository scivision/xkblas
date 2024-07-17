#ifndef __THREAD_HPP__
# define __THREAD_HPP__

# include "logger/todo.h"
// # include "scheduler/deque.hpp"
# include "scheduler/naive-queue.hpp"
# include "scheduler/memory-tree.hpp"
# include "scheduler/task.hpp"

# include <new>

// Maximum number of bytes allocated for tasks descriptor
# ifndef THREAD_MAX_MEMORY
#  define THREAD_MAX_MEMORY (64*1024*1024)
# endif /* THREAD_MAX_MEMORY */

// Maximum tasks queued in this thread queue
# ifndef THREAD_DEQUE_CAPACITY
#  define THREAD_DEQUE_CAPACITY 16384
# endif /* THREAD_DEQUE_CAPACITY */

class alignas(std::hardware_constructive_interference_size) Thread
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
        static Thread * get(void);

        ////////////////////////
        // NON-STATIC MEMBERS //
        ////////////////////////
        Thread();
        virtual ~Thread();

        /* allocates memory */
        uint8_t * allocate(uint64_t size);

        /* free all allocated memory */
        void deallocate_all(void);

        /**
         *  Process task dependence.
         *  A task cannot be scheduled before a 'commit' call, but may be
         *  scheduled before its return by any thread
         */
        template<int N>
        void commit(Task * task)
        {
            // set edges with previously inserted tasks
            for (int i = 0 ; i < N ; ++i)
            {
                assert(task->accesses[i].mode);
                this->memtree.intersect(task->accesses[i].mode, task->accesses[i].region, task);
            }

            // register accesses for linking with future tasks
            for (int i = 0 ; i < N ; ++i)
                this->memtree.insert(task->accesses[i].mode, task->accesses[i].region, task);

            // commit the task
            if (task->commit() == TASK_STATE_READY)
                this->queue.push(task);
        }

    private:

        /* tasks stack */
        alignas(std::hardware_constructive_interference_size) uint8_t memory_stack_bottom[THREAD_MAX_MEMORY];

        /* next free task pointer in the stack */
        uint8_t * memory_stack_ptr;

        /* per-thread queue */
        // Deque<Task *, THREAD_DEQUE_CAPACITY> queue;
        NaiveQueue<Task *> queue;

        /* Memory mapping */
        MemoryTree memtree;
};

#endif /* __THREAD_HPP__ */
