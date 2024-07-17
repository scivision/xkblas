#ifndef __THREAD_HPP__
# define __THREAD_HPP__

# include "logger/todo.h"
# include "scheduler/deque.hpp"
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
        void commit(Thread * thread);

    private:

        /* tasks stack */
        alignas(std::hardware_constructive_interference_size) uint8_t memory_stack_bottom[THREAD_MAX_MEMORY];

        /* next free task pointer in the stack */
        uint8_t * memory_stack_ptr;

        /* per-thread queue */
        Deque<Task *, THREAD_DEQUE_CAPACITY> queue;

        /* Memory mapping */
        MemoryTree memtree;
};

#endif /* __THREAD_HPP__ */
