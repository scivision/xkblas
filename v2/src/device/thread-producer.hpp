#ifndef __THREAD_PRODUCER_HPP__
# define __THREAD_PRODUCER_HPP__

# include "logger/todo.h"
# include "device/dependency-tree.hpp"
# include "device/driver.h"
# include "device/naive-queue.hpp"
# include "device/task.hpp"
# include "device/thread-worker.hpp"

# include <new>

// Maximum number of bytes allocated for tasks descriptor
# ifndef THREAD_PRODUCER_MAX_MEMORY
#  define THREAD_PRODUCER_MAX_MEMORY (64*1024*1024)
# endif /* THREAD_PRODUCER_MAX_MEMORY */

/**
 *  This class represents an xkblas user thread, that is producing tasks
 */
class alignas(std::hardware_constructive_interference_size) ThreadProducer
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
        static ThreadProducer * get(void);

        ////////////////////////
        // NON-STATIC MEMBERS //
        ////////////////////////
        ThreadProducer();
        virtual ~ThreadProducer();

        /* allocates memory */
        uint8_t * allocate(uint64_t size);

        /* free all allocated memory */
        void deallocate_all(void);

        /**
         *  Commit the passed task
         *      - compute its dependences
         *      - submit if ready
         *  A task cannot be scheduled before a 'commit' call.
         *  The task may be scheduled before this function returns
         */
        template<int N>
        void commit(xkblas_drivers_t * drivers, Task * task)
        {
            task->naccesses = N;

            // set edges with previously inserted tasks
            for (int i = 0 ; i < N ; ++i)
            {
                task_access_t<2> * access = task->accesses + i;
                this->deptree.intersect(task, access->region, access->mode);
            }

            // register accesses for linking with future tasks
            for (int i = 0 ; i < N ; ++i)
            {
                task_access_t<2> * access = task->accesses + i;
                this->deptree.insert(task, access->region, access->mode);
            }

            // commit the task
            if (task->commit())
            {
                // defer the task to consumer threads
                xkblas_drivers_enqueue(drivers, task);
            }

            # ifndef NDEBUG
            tasks.push_back(task);
            # endif
        }

        # ifndef NDEBUG
        void
        dump_tasks(FILE * f)
        {
            fprintf(f, "digraph G {\n");
            for (Task * & task : this->tasks)
                fprintf(f, "    \"%p\" [label=\"%s\"] ;\n", task, task->label);

            for (Task * & pred : this->tasks)
                for (task_edge_t & edge : pred->edges)
                    fprintf(f, "    \"%p\" -> \"%p\" ;\n", pred, edge.successor);
            fprintf(f, "}\n");
        }
        # endif /* NDEBUG */

    private:

        /* tasks stack */
        alignas(std::hardware_constructive_interference_size)
            uint8_t memory_stack_bottom[THREAD_PRODUCER_MAX_MEMORY];

        /* next free task pointer in the stack */
        uint8_t * memory_stack_ptr;

        /* Dependency tree */
        DependencyTree<2> deptree;

        #ifndef NDEBUG
        std::vector<Task *> tasks;
        #endif /* NDEBUG */
};

#endif /* __THREAD_PRODUCER_HPP__ */
