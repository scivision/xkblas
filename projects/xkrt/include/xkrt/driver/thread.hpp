/* ************************************************************************** */
/*                                                                            */
/*   thread.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/28 01:28:33 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __THREAD_HPP__
# define __THREAD_HPP__

# include <xkrt/driver/dependency-tree.hpp>
# include <xkrt/driver/naive-queue.hpp>
# include <xkrt/driver/thread.hpp>
# include <xkrt/logger/todo.h>
# include <xkrt/memory/cache-line-size.hpp>
# include <xkrt/task/task.hpp>

# include <stddef.h>
# include <stdint.h>

# ifndef THREAD_MAX_MEMORY
#  define THREAD_MAX_MEMORY ((size_t)2*1024*1024*1024)
# endif /* THREAD_MAX_MEMORY */

class alignas(CACHE_LINE_SIZE) Thread
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
        static Thread * self(void);

        ////////////////////////
        // NON-STATIC MEMBERS //
        ////////////////////////

        Thread();
        ~Thread();

        /* allocates memory */
        uint8_t * allocate(uint64_t size);

        /* free all allocated memory */
        void deallocate_all(void);

        /* Sleep the thread until signaled */
        void pause(void);

        /* Wake up the thread */
        void wakeup(void);

        /* push a task */
        void push(Task * const & task);

        /* pop a task */
        Task * pop(void);

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
                    deptrees[access_id]->intersect(search, access->cubes[0]);
                    deptrees[access_id]->intersect(search, access->cubes[1]);
                }

                // insert for future tasks
                for (int access_id = 0 ; access_id < N ; ++access_id)
                {
                    search.prepare_resolve(task, access_id);

                    const Access * access = task->accesses + access_id;
                    assert(access);

                    assert(deptrees[access_id]);
                    deptrees[access_id]->insert(search, access->cubes[0]);
                    deptrees[access_id]->insert(search, access->cubes[1]);
                }
            }
        }


        template <void (*callback)(void * vargs, Task * task)>
        inline task_state_t
        commit(void * vargs, Task * task)
        {
            # ifndef NDEBUG
            tasks.push_back(task);
            # endif

            assert(this->current_task);
            this->current_task->cc.fetch_add(1, std::memory_order_relaxed);
            task->parent = this->current_task;
            return task->commit<callback>(vargs);
        }

        /**
         * Complete the given task, that is:
         *  - callback with any successors that are now ready
         *  - then move the wait counter
         */
        template <void (*callback)(void * vargs, Task * task)>
        void
        complete(void * vargs, Task * task)
        {
            task->complete<callback>(vargs);
        }

        # ifndef NDEBUG
        void
        dump_tasks(FILE * f)
        {
            Thread::dump_tasks(f, this->tasks);
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

        void report_tasks(void);
        # endif /* NDEBUG */

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

    public:

        /* the cpuset of that worker */
        cpu_set_t cpuset;

        /* the thread implicit task */
        Task implicit_task;

        /* the current task */
        Task * current_task;

    private:

        /* tasks stack */
        uint8_t * memory_stack_bottom;

        /* next free task pointer in the stack */
        uint8_t * memory_stack_ptr;

        /* memory capacity */
        size_t capacity;

        /* per-thread queue */
        // Deque<Task *, THREAD_WORKER_DEQUE_CAPACITY> queue;
        NaiveQueue<Task *> queue;

        /* lock and condition to sleep the mutex */
        struct {
            pthread_mutex_t lock;
            pthread_cond_t  cond;
            volatile bool   sleeping;
        } sleep;

        /* Dependency tree */
        std::vector<DependencyTree *> deptrees;

        #ifndef NDEBUG
        std::vector<Task *> tasks;
        #endif /* NDEBUG */


};

#endif /* __THREAD_HPP__ */
