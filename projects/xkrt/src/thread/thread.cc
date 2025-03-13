/* ************************************************************************** */
/*                                                                            */
/*   thread.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/12 20:30:49 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/thread/thread.h>
# include <xkrt/runtime.h>

# include <cassert>
# include <cstring>
# include <cerrno>

# include <hwloc.h>
# include <sched.h>
# include <hwloc/glibc-sched.h>

# pragma message(TODO "Threading layer had been implemented in half a day with naive algorithm. If perf is an issue, reimplement with well-known hierarchical algorithm")

void
xkrt_runtime_t::thread_setaffinity(cpu_set_t & cpuset)
{
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    for (int ii = 0; ii < 10; ++ii) sched_yield();
}

void
xkrt_runtime_t::thread_getaffinity(cpu_set_t & cpuset)
{
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

static inline int
team_barrier_fetch(xkrt_team_t * team, int delta)
{
    int n = team->priv.barrier.n.fetch_sub(delta, std::memory_order_relaxed) - delta;
    assert(n >= 0);
    if (n == 0)
    {
        team->priv.barrier.n.store(team->priv.nthreads, std::memory_order_seq_cst);
        ++team->priv.barrier.version;
        pthread_mutex_lock(&team->priv.barrier.mtx);
        {
            pthread_cond_broadcast(&team->priv.barrier.cond);
        }
        pthread_mutex_unlock(&team->priv.barrier.mtx);
    }
    return n;
}

typedef struct  team_recursive_args_t
{
    xkrt_runtime_t * runtime;
    xkrt_team_t * team;
    cpu_set_t cpuset;
    int from;
    int to;
}               team_recursive_args_t;

/** Set the cpuset for the given thread */
static void inline
team_create_get_cpuset(xkrt_runtime_t * runtime, xkrt_team_t * team, int tid, cpu_set_t * cpuset)
{
    switch (team->desc.binding.mode)
    {
        case (XKRT_TEAM_BINDING_MODE_COMPACT):
        {
            switch (team->desc.binding.places)
            {
                case (XKRT_TEAM_BINDING_PLACES_DEVICE):
                {
                    const xkrt_device_t * device = runtime->device_get((xkrt_device_global_id_t) tid);
                    assert(device);
                    *cpuset = device->thread->cpuset;
                    return ;
                }

                case (XKRT_TEAM_BINDING_PLACES_CORE):
                {
                    hwloc_obj_t pu = hwloc_get_pu_obj_by_os_index(runtime->topology, tid);
                    HWLOC_SAFE_CALL(
                        hwloc_cpuset_to_glibc_sched_affinity(
                            runtime->topology,
                            pu->cpuset,
                            cpuset,
                            sizeof(cpu_set_t)
                        )
                    );
                    return ;
                }

                default:
                    LOGGER_FATAL("Team config. not supported");
            }
        }

        default:
            LOGGER_FATAL("Team config. not supported");
    }
}

void team_create_recursive_fork(xkrt_runtime_t * runtime, xkrt_team_t * team, int from, int to);

static void *
team_create_recursive(void * vargs)
{
    team_recursive_args_t * args = (team_recursive_args_t *) vargs;

    // recursion end
    if (args->from == args->to)
    {
        // init tls
        Thread * tls = Thread::self();
        xkrt_team_t * team = args->team;
        int tid = args->from;
        xkrt_thread_t * thread = team->priv.threads + tid;
        thread->tid = tid;

        tls->team = team;
        tls->thread = thread;

        // register affinity
        xkrt_runtime_t::thread_getaffinity(tls->cpuset);

        // launch routine
        thread->state = XKRT_THREAD_INITIALIZED;
        void * r = args->team->desc.routine(team, thread);

        free(args);

        return r;
    }

    const int from1 = args->from;
    const int to1   = args->from + (args->to - args->from) / 2;
    const int from2 = to1 + 1;
    const int to2   = args->to;

    if (from2 <= to2)
        team_create_recursive_fork(args->runtime, args->team, from2, to2);

    args->from = from1;
    args->to   = to1;
    team_create_recursive(args);

    return NULL;
}

void
team_create_recursive_fork(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    int from,
    int to
) {
    assert(to >= from);

    // save calling thread cpu set
    cpu_set_t save_set;
    xkrt_runtime_t::thread_getaffinity(save_set);

    // retrieve cpuset
    const int tid = from;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    team_create_get_cpuset(runtime, team, tid, &cpuset);

    // move thread before allocating future thread-private memory
    xkrt_runtime_t::thread_setaffinity(cpuset);

    team_recursive_args_t * args = (team_recursive_args_t *) malloc(sizeof(team_recursive_args_t));
    args->runtime = runtime;
    args->team = team;
    args->from = from;
    args->to = to;
    args->cpuset = cpuset;

    // fork
    xkrt_thread_t * thread = team->priv.threads + tid;
    pthread_create(&thread->pthread, NULL, team_create_recursive, args);

    // restore calling thread cpu set
    xkrt_runtime_t::thread_setaffinity(save_set);
}

void
xkrt_runtime_t::team_create(xkrt_team_t * team)
{
    // only supported modes currently
    assert(
        (team->desc.binding.mode == XKRT_TEAM_BINDING_MODE_COMPACT && team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_DEVICE) ||
        (team->desc.binding.mode == XKRT_TEAM_BINDING_MODE_COMPACT && team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_CORE)
    );

    // set all to zero
    memset(&team->priv, 0, sizeof(team->priv));

    // allocate thread array
    const int nthreads = team->desc.nthreads;   // TODO : this should be a func of the team desc
    team->priv.nthreads = nthreads;
    team->priv.threads = (xkrt_thread_t *) malloc(sizeof(xkrt_thread_t) * team->priv.nthreads);
    assert(team->priv.threads);
    memset(team->priv.threads, 0, sizeof(xkrt_thread_t) * team->priv.nthreads);

    // init barrier with nthreads + 1 to avoid early barrier release
    team->priv.barrier.n.store(team->priv.nthreads + 1, std::memory_order_seq_cst);
    pthread_mutex_init(&team->priv.barrier.mtx, NULL);
    pthread_cond_init(&team->priv.barrier.cond, NULL);

    // init critical
    pthread_mutex_init(&team->priv.critical.mtx, NULL);

    // fork thread 0
    team_create_recursive_fork(this, team, 0, team->priv.nthreads - 1);

    // decrement barrier
    team_barrier_fetch(team, 1);
}

static inline void
run(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    xkrt_thread_t * thread,
    task_t * task
) {
    Thread * tls = Thread::self();
    // tls->execute(task, xkrt_team_thread_task_enqueue, runtime, team, thread);
    __Thread_task_execute(tls, task, xkrt_team_thread_task_enqueue, runtime, team, thread);
}

/* Return the 'i-th' victim to steal for the thread 'tid' when there is 'n' threads in the tree */
static inline int
get_ith_victim(int tid, int i, int n)
{
    // assume threads are bound 1:1 compactly onto physical cores
    // for instance, if we have n = 4 threads, we have
    //      F : (tid, i, n) -> victim
    // defined as
    //      F(0, 0, 4) = 0  - but whatever, threads dont steal themselves
    //      F(0, 1, 4) = 1
    //      F(0, 2, 4) = 2
    //      F(0, 3, 4) = 3
    // and
    //      F(1, 0, 4) = 1  - but whatever, threads dont steal themselves
    //      F(1, 1, 4) = 0
    //      F(1, 2, 4) = 3
    //      F(1, 3, 4) = 2
    // and
    //      F(2, 0, 4) = 2
    //      F(2, 1, 4) = 3
    //      F(2, 2, 4) = 0
    //      F(2, 3, 4) = 1

    return (i + tid) % n;
}

static inline int
worksteal(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    xkrt_thread_t * thread
) {
    const int n = team->priv.nthreads;
    const int tid = thread->tid;

    for (int i = 0 ; i < n ; ++i)
    {
        const int victim_tid = get_ith_victim(tid, i, n);
        xkrt_thread_t * victim = team->priv.threads + victim_tid;
        task_t * task = (victim_tid == tid) ? victim->deque.pop() : victim->deque.steal();
        if (task)
        {
            run(runtime, team, thread, task);
            return 1;
        }
    }
    return 0;
}

static inline int
schedule(
    xkrt_runtime_t * runtime,
    xkrt_team_t * team,
    xkrt_thread_t * thread
) {
    return worksteal(runtime, team, thread);
}

void
xkrt_runtime_t::task_wait(void)
{
    Thread * tls = Thread::self();
    assert(tls);

    # define WAIT do { if (tls->current_task->cc.load(std::memory_order_relaxed) == 0) return ; } while (0)

    /* active polling */
    # if 0

    while (1)
    {
        if (tls->team && schedule(this, tls->team, tls->thread))
            continue ;
        WAIT ;
    }

    # else
    /* exponential backoff sleep */

    # define WAIT2   do { WAIT   ; WAIT   ; } while (0)
    # define WAIT4   do { WAIT2  ; WAIT2  ; } while (0)
    # define WAIT8   do { WAIT4  ; WAIT4  ; } while (0)
    # define WAIT16  do { WAIT8  ; WAIT8  ; } while (0)
    # define WAIT32  do { WAIT16 ; WAIT16 ; } while (0)
    # define WAIT64  do { WAIT32 ; WAIT32 ; } while (0)

    // Poll first for fast way out
    mem_barrier();
    WAIT32 ;

    // Else, work steal and sleep with backoff
    constexpr int initial_backoff = 1024;;  // Initial backoff time in nanoseconds
    constexpr int max_backoff = 64 * 1024;  // Maximum backoff time nanoseconds
    int backoff = initial_backoff;          // Initial backoff time in nanoseconds
    assert(max_backoff < 1000000);          // nanosleep condition

    struct timespec ts = { .tv_sec = 0 };
    while (1)
    {
        // work steal
        if (tls->team && schedule(this, tls->team, tls->thread))
        {
            backoff = initial_backoff;
            continue ;
        }

        // sleep with backoff
        ts.tv_nsec = backoff;
        nanosleep(&ts, NULL);
        if (backoff < max_backoff)
            backoff = (backoff << 1);
        WAIT64 ;
    }
    # endif
}

template<bool worksteal>
void
xkrt_runtime_t::team_barrier(xkrt_team_t * team, xkrt_thread_t * thread)
{
    // TODO : reimplement this using team's topology

    this->task_wait();

    if (team->priv.nthreads == 1)
        return ;

    assert((worksteal && thread) || (!worksteal && !thread));

    int old_version = team->priv.barrier.version;
    if (team_barrier_fetch(team, 1))
    {
        while (old_version == team->priv.barrier.version)
        {
            if (worksteal && schedule(this, team, thread))
                continue ;

            pthread_mutex_lock(&team->priv.barrier.mtx);
            {
                if (old_version == team->priv.barrier.version)
                {
                    pthread_cond_wait(&team->priv.barrier.cond, &team->priv.barrier.mtx);
                }
            }
            pthread_mutex_unlock(&team->priv.barrier.mtx);
        }
    }
}

template void xkrt_runtime_t::team_barrier<true>(xkrt_team_t * team, xkrt_thread_t * thread);
template void xkrt_runtime_t::team_barrier<false>(xkrt_team_t * team, xkrt_thread_t * thread);

void
xkrt_runtime_t::team_join(xkrt_team_t * team)
{
    // TODO : reimpl this using team's topology
    for (int i = 0 ; i < team->priv.nthreads ; ++i)
    {
        // waiting for the thread to spawn before joining
        while ((volatile xkrt_thread_state_t) team->priv.threads[i].state != XKRT_THREAD_INITIALIZED)
            ;
        assert(team->priv.threads[i].state == XKRT_THREAD_INITIALIZED);
        pthread_join(team->priv.threads[i].pthread, NULL);
        team->priv.threads[i].state = XKRT_THREAD_UNINITIALIZED;
    }
    free(team->priv.threads);
}

void
xkrt_runtime_t::team_critical_begin(xkrt_team_t * team)
{
    pthread_mutex_lock(&team->priv.critical.mtx);
}

void
xkrt_runtime_t::team_critical_end(xkrt_team_t * team)
{
    pthread_mutex_unlock(&team->priv.critical.mtx);
}
