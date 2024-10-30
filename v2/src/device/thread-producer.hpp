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
            {
                # pragma message(TODO "If we semantically force a task accesses to be disjointed, then these 2 loops can be merged with no risks of dependency cycle")

                DependencyTree::Search search;
                DependencyTree * deptrees[N];

                // intersect with past tasks
                for (int access_id = 0 ; access_id < N ; ++access_id)
                {
                    search.prepare_resolve(task, access_id);

                    const Access * access = task->accesses + access_id;
                    assert(access);

                    deptrees[access_id] = this->get_dependency_tree_for_ld(access->host_view.ld);
                    deptrees[access_id]->intersect(search, access->cube, access->mode);
                }

                // insert for future tasks
                for (int access_id = 0 ; access_id < N ; ++access_id)
                {
                    search.prepare_resolve(task, access_id);

                    const Access * access = task->accesses + access_id;
                    assert(access);

                    assert(deptrees[access_id]);
                    deptrees[access_id]->insert(search, access->cube, access->mode);
                }
            }
        }

        inline void
        commit(Task * task)
        {
            task->commit();

            # ifndef NDEBUG
            tasks.push_back(task);
            # endif
        }

        inline DependencyTree *
        get_dependency_tree_for_ld(const size_t ld)
        {
            /* find previous deptree for that ld */
            for (DependencyTree * deptree : this->deptrees)
                if (deptree->ld == ld)
                    return deptree;

            /* if not found, create a new deptree */
            DependencyTree * deptree = new DependencyTree(ld);
            assert(deptree);
            assert(deptree->ld == ld);
            this->deptrees.push_back(deptree);
            return deptree;
        }

        # ifndef NDEBUG
        void
        dump_tasks(FILE * f)
        {
            ThreadProducer::dump_tasks(f, this->tasks);
        }

        static void
        dump_tasks(FILE * f, std::vector<Task *> & tasks)
        {
            fprintf(f, "digraph G {\n");
            for (Task * & task : tasks)
                fprintf(f, "    \"%p\" [label=\"%s\"] ;\n", task, task->label);

            for (Task * & pred : tasks)
                for (Task::Edge & edge : pred->edges)
                    fprintf(f, "    \"%p\" -> \"%p\" ;\n", pred, edge.successor);
            fprintf(f, "}\n");
        }
        # endif /* NDEBUG */

    public:

        /* Dependency tree */
        std::vector<DependencyTree *> deptrees;

        #ifndef NDEBUG
        std::vector<Task *> tasks;
        #endif /* NDEBUG */

};

#endif /* __THREAD_PRODUCER_HPP__ */
