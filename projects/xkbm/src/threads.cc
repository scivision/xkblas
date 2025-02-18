# include <atomic>
# include <hwloc.h>
# include <pthread.h>

# include <xkbm/topology.h>
# include <xkbm/team.h>

# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>

// bind the current thread to the hwloc cpu mask
static void
cpu_bind_to(hwloc_cpuset_t cpuset)
{
    HWLOC_SAFE_CALL(hwloc_set_cpubind(TOPOLOGY, cpuset, HWLOC_CPUBIND_THREAD));
    for (int ii = 0; ii < 100; ++ii)
        sched_yield();
}

typedef struct  trampoline_args_t
{
    xkbm_team_t * team;
    int tid;
    void * args;
}               trampoline_args_t;

static void *
trampoline(void * vargs)
{
    trampoline_args_t * args = (trampoline_args_t *) vargs;
    args->team->routine(args->team, args->tid, args->args);
    free(vargs);
    return NULL;
}

/**
 *  'n' is the weight of the cpuset
 *  Fork n thread binding it on each cpu
 *  The calling thread sleep until all threads are done
 */
void
xkbm_team_work(
    hwloc_cpuset_t cpuset,
    void (*routine)(xkbm_team_t *, int, void *),
    void * args
) {
    xkbm_team_t team;
    team.routine = routine;
    team.args = args;
    team.cpuset = cpuset;
    team.nthreads = hwloc_bitmap_weight(cpuset);
    if (team.nthreads == 0)
        return ;
    team.threads = (xkbm_thread_t *) malloc(sizeof(xkbm_thread_t) * team.nthreads);
    assert(team.threads);

    // init barrier
    team.barrier.n.store(team.nthreads, std::memory_order_seq_cst);
    pthread_mutex_init(&team.barrier.mtx, NULL);
    pthread_cond_init(&team.barrier.cond, NULL);

    // save current thread cpuset
    hwloc_cpuset_t saveset = hwloc_bitmap_alloc();
    assert(saveset);
    HWLOC_SAFE_CALL(hwloc_get_cpubind(TOPOLOGY, saveset, HWLOC_CPUBIND_THREAD));

    hwloc_cpuset_t forkset = hwloc_bitmap_alloc();
    int tid = 0;
    unsigned int id;
    hwloc_bitmap_foreach_begin(id, cpuset)
    {
        hwloc_bitmap_set(forkset, id);
        cpu_bind_to(forkset);

        xkbm_thread_t * thread = team.threads + tid;
        assert(tid < team.nthreads);

        trampoline_args_t * vargs = (trampoline_args_t *) malloc(sizeof(trampoline_args_t));
        assert(vargs);
        vargs->team = &team;
        vargs->tid = tid;
        vargs->args = args;
        pthread_create(&thread->pthread, NULL, trampoline, vargs);

        hwloc_bitmap_clr(forkset, id);
        ++tid;
    }
    hwloc_bitmap_foreach_end();
    assert(tid == team.nthreads);
    hwloc_bitmap_free(forkset);

    // bind back to original cpu set
    cpu_bind_to(saveset);
    hwloc_bitmap_free(saveset);

    // wait for work completion
    for (int i = 0 ; i < team.nthreads ; ++i)
        pthread_join(team.threads[i].pthread, NULL);
}

# pragma message(TODO "Optimize this barrier so it has as little restart latency as possible")
/* Barrier, wait until all threads called */
void
xkbm_team_barrier(xkbm_team_t * team)
{
    int old_version = team->barrier.version;
    if (team->barrier.n.fetch_sub(1, std::memory_order_relaxed) == 1)
    {
        team->barrier.n.store(team->nthreads, std::memory_order_seq_cst);
        ++team->barrier.version;
        pthread_mutex_lock(&team->barrier.mtx);
        {
            pthread_cond_broadcast(&team->barrier.cond);
        }
        pthread_mutex_unlock(&team->barrier.mtx);
    }
    else
    {
        while (old_version == team->barrier.version)
        {
            pthread_mutex_lock(&team->barrier.mtx);
            {
                if (old_version == team->barrier.version)
                {
                    pthread_cond_wait(&team->barrier.cond, &team->barrier.mtx);
                }
            }
            pthread_mutex_unlock(&team->barrier.mtx);
        }
    }
}
