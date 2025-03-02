/* ************************************************************************** */
/*                                                                            */
/*   thread.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/02 01:07:08 by Romain PEREIRA            \_)     (_/    */
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
        void push(task_t * const & task);

        /* pop a task */
        task_t * pop(void);

        /**
         * Resolve dependencies of the passed task through the domain of the
         * task currently executing
         */
        template <int AC>
        inline void
        resolve(task_t * task, access_t * accesses)
        {
            assert(task->flags & TASK_FLAG_DEPENDENT);
            assert(AC > 0);

            task_dep_info_t * dep = TASK_DEP_INFO(task);
            new (dep) task_dep_info_t(AC);

            // TODO
            // 1) we assume that all accesses use that same dependency domain
            // 2) C++ pure virtual function cannot be templated. To still
            //    benefits from compile-time optimization, we force the casting to
            //    a DependencyTree, as it is the only DependencyDomain currently.
            access_t * access = accesses + 0;
            DependencyDomain * domain = task_get_dependency_domain(task, accesses + 0);
            if (domain == NULL)
            {
                domain = new DependencyTree(access->host_view.ld, access->host_view.sizeof_type);
                task_put_dependency_domain(task, domain);
            }
            DependencyTree * tree = (DependencyTree *) domain;
            tree->resolve<AC>(accesses);
        }

        /**
         *  Commit the passed task
         *      - compute its dependences
         *      - submit if ready
         *  A task cannot be scheduled before a 'commit' call.
         *  The task may be scheduled before this function returns
         */
        template <void (*callback)(void * vargs, task_t * task)>
        inline void
        commit(void * vargs, task_t * task)
        {
            # ifndef NDEBUG
            tasks.push_back(task);
            # endif

            assert(this->current_task);
            this->current_task->cc.fetch_add(1, std::memory_order_relaxed);
            task->parent = this->current_task;
            __task_commit(callback, vargs, task);
        }

        # ifndef NDEBUG
        void
        dump_tasks(FILE * f)
        {
            Thread::dump_tasks(f, this->tasks);
        }

        static void
        dump_tasks(FILE * f, std::vector<task_t *> & tasks)
        {
            fprintf(f, "digraph G {\n");
            for (task_t * & task : tasks)
                fprintf(f, "    \"%p\" [label=\"%s\"] ;\n", task, task->label);

            for (task_t * & pred : tasks)
            {
                if (pred->flags & TASK_FLAG_DEPENDENT)
                {
                    task_dep_info_t * dep = TASK_DEP_INFO(pred);
                    access_t * accesses = TASK_ACCESSES(pred);
                    for (int i = 0 ; i < dep->ac ; ++i)
                    {
                        access_t * pred_access = accesses + i;
                        for (access_t * succ_access : pred_access->successors)
                        {
                            task_t * succ = succ_access->task;
                            fprintf(f, "    \"%p\" -> \"%p\" ;\n", pred, succ);
                        }
                    }
                }
            }
            fprintf(f, "}\n");
        }

        void report_tasks(void);
        # endif /* NDEBUG */

    public:

        /* the cpuset of that worker */
        cpu_set_t cpuset;

        /* the thread implicit task */
        task_t implicit_task;

        /* the current task */
        task_t * current_task;

    private:

        /* tasks stack */
        uint8_t * memory_stack_bottom;

        /* next free task pointer in the stack */
        uint8_t * memory_stack_ptr;

        /* memory capacity */
        size_t capacity;

        /* per-thread queue */
        // Deque<task_t *, THREAD_WORKER_DEQUE_CAPACITY> queue;
        NaiveQueue<task_t *> queue;

        /* lock and condition to sleep the mutex */
        struct {
            pthread_mutex_t lock;
            pthread_cond_t  cond;
            volatile bool   sleeping;
        } sleep;

        /* Dependency tree */
        std::vector<DependencyTree *> deptrees;

        #ifndef NDEBUG
        std::vector<task_t *> tasks;
        #endif /* NDEBUG */


};

#endif /* __THREAD_HPP__ */
