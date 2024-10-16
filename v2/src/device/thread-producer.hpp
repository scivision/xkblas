#ifndef __THREAD_PRODUCER_HPP__
# define __THREAD_PRODUCER_HPP__

# include "xkblas-context.h"
# include "logger/todo.h"
# include "device/dependency-tree.hpp"
# include "device/driver.h"
# include "device/naive-queue.hpp"
# include "device/task.hpp"
# include "device/thread.hpp"
# include "sync/cache-line-size.hpp"

/**
 *  This class represents an xkblas user thread, that is producing tasks
 */
class alignas(CACHE_LINE_SIZE) ThreadProducer : public Thread
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
        static ThreadProducer * self(void);

        ////////////////////////
        // NON-STATIC MEMBERS //
        ////////////////////////
        ThreadProducer() : Thread() {}

        ~ThreadProducer() {}

        /**
         *  Commit the passed task
         *      - compute its dependences
         *      - submit if ready
         *  A task cannot be scheduled before a 'commit' call.
         *  The task may be scheduled before this function returns
         */
        template<int N>
        inline void
        resolve(Task * task)
        {
            task->naccesses = N;

            if (N)
                this->deptree.resolve<N>(task);
        }

        inline void
        commit(
            xkblas_context_t * context,
            Task * task
        ) {
            task->commit();

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
                for (Task::Edge & edge : pred->edges)
                    fprintf(f, "    \"%p\" -> \"%p\" ;\n", pred, edge.successor);
            fprintf(f, "}\n");
        }
        # endif /* NDEBUG */

        #ifndef NDEBUG
    public:
        std::vector<Task *> tasks;
        #endif /* NDEBUG */

        /* Dependency tree */
        DependencyTree deptree;
};

#endif /* __THREAD_PRODUCER_HPP__ */
