/* ************************************************************************** */
/*                                                                            */
/*   thread.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:45 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/06 06:56:08 by Romain PEREIRA            \_)     (_/    */
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
# include <xkrt/thread/thread.h>

# include <stddef.h>
# include <stdint.h>

# ifndef THREAD_MAX_MEMORY
#  define THREAD_MAX_MEMORY ((size_t)2*1024*1024*1024)
# endif /* THREAD_MAX_MEMORY */

/* thread local storage through Thread::self() */
class alignas(CACHE_LINE_SIZE) Thread
{
    public:

        /* the xkrt_team_t of that thread */
        xkrt_team_t * team;

        /* the xkrt_thread_t of that thread */
        xkrt_thread_t * thread;

        /* the cpuset of that thread */
        cpu_set_t cpuset;

        /* the thread implicit task */
        union {
            task_t implicit_task;
            char _implicit_task_buffer[task_compute_size(TASK_FLAG_DOMAIN, 0)];
        };

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

        /* touch every pages of thread memory */
        void warmup(void);

        /* allocates a task */
        task_t * allocate_task(const size_t size);

        /* free all allocated memory */
        void deallocate_all_tasks(void);

        /* Sleep the thread until signaled */
        void pause(void);

        /* Wake up the thread */
        void wakeup(void);

        /* push a task */
        void push(task_t * const & task);

        /* pop a task */
        task_t * pop(void);

        /**
         * Retrieve or (insert and return) the dependency domain of the passed access
         * on the currently executing task
         */
        inline DependencyDomain *
        get_dependency_domain(const access_t * access)
        {
            DependencyDomain * domain = task_get_dependency_domain(this->current_task, access);
            if (domain == NULL)
            {
                domain = new DependencyTree(access->host_view.ld, access->host_view.sizeof_type);
                task_put_dependency_domain(this->current_task, domain);
            }
            return domain;
        }

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
            DependencyTree * tree = (DependencyTree *) this->get_dependency_domain(access);
            tree->resolve<AC>(accesses);
        }

        # define __Thread_task_execute(T, t, F, ...)                                                \
            do {                                                                                    \
                assert(T && t);                                                                     \
                task_format_t * format = runtime->formats.list.list + t->fmtid;                     \
                assert(format->f[TASK_FORMAT_TARGET_HOST]);                                         \
                task_t * current = T->current_task;                                                 \
                T->current_task = t;                                                                \
                void (*f)(task_t *) = (void (*)(task_t *)) format->f[TASK_FORMAT_TARGET_HOST];      \
                f(t);                                                                               \
                __task_executed(t, F, __VA_ARGS__);                                                 \
                T->current_task = current;                                                          \
            } while (0)

        template <typename... Args>
        inline void
        commit(
            task_t * task,
            void (*F)(Args..., task_t *),
            Args... args
        ) {
            assert(this->current_task);
            ++this->current_task->cc;
            task->parent = this->current_task;
            __task_commit(task, F, args...);
        }

};

#endif /* __THREAD_HPP__ */
