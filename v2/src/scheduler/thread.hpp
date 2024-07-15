#ifndef __THREAD_HPP__
# define __THREAD_HPP__

# include "scheduler/deque.hpp"
# include "scheduler/task.hpp"

# include <new>

// Maximum number of bytes allocated for tasks descriptor
# ifndef THREAD_MAX_MEMORY
#  define THREAD_MAX_MEMORY (64*1024*1024)
# endif /* THREAD_MAX_MEMORY */

// Maximum tasks queued in this thread deque
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

        /* submit a task */
        void submit(Task * task);

    private:

        /* tasks stack */
        alignas(std::hardware_constructive_interference_size) uint8_t memory_stack_bottom[THREAD_MAX_MEMORY];

        /* next free task pointer in the stack */
        uint8_t * memory_stack_ptr;

        /* per-thread queue */
        Deque<THREAD_DEQUE_CAPACITY> deque;
};

#endif /* __THREAD_HPP__ */
