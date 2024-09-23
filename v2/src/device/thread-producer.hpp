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
        void commit(xkblas_context_t * context, Task * task)
        {
            task->naccesses = N;

            // set edges with previously inserted tasks, and then register
            // accesses for linking with future tasks
            for (int i = 0 ; i < N ; ++i)
                this->deptree.intersect(task, task->accesses[i].region, task->accesses[i].mode);
            for (int i = 0 ; i < N ; ++i)
            {
                # if 0
                XKBLAS_WARN("Interval(%4d,%4d), Interval(%4d,%4d),",
                        task->accesses[i].region[0].a,
                        task->accesses[i].region[0].b,
                        task->accesses[i].region[1].a,
                        task->accesses[i].region[1].b);
                # endif
                this->deptree.insert(task, task->accesses[i].region, task->accesses[i].mode);
            }

            // commit the task - and enqueue it if now ready
            if (task->commit())
                xkblas_context_submit_task(context, task);

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
