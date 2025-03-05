/* ************************************************************************** */
/*                                                                            */
/*   thread.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/05 05:18:37 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

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

/* trampoline arguments */
typedef struct  xkrt_thread_trampoline_args_t
{
    xkrt_team_t * team;
    int tid;
}               xkrt_thread_trampoline_args_t;

static void *
trampoline(void * vargs)
{
    xkrt_thread_trampoline_args_t * args = (xkrt_thread_trampoline_args_t *) vargs;
    assert(args);

    args->team->priv.threads[args->tid].state = XKRT_THREAD_INITIALIZED;
    args->team->desc.routine(args->team, args->tid);
    free(args);

    return NULL;
}

static inline int
team_barrier_fetch(xkrt_team_t * team, int delta)
{
    int n = team->priv.barrier.n.fetch_sub(delta, std::memory_order_relaxed) - delta;
    assert(n >= 0);
    if (n == 0)
    {
        team->priv.barrier.n.store(team->desc.nthreads, std::memory_order_seq_cst);
        ++team->priv.barrier.version;
        pthread_mutex_lock(&team->priv.barrier.mtx);
        {
            pthread_cond_broadcast(&team->priv.barrier.cond);
        }
        pthread_mutex_unlock(&team->priv.barrier.mtx);
    }
    return n;
}

/**
 *  Let's say (to, from) = (0, 3) - then the calling thread
 *      - fork a new thread with (to, from) = (0, 1)
 *      - fork a new thread with (to, from) = (2, 3)
 *      - and return
 *
 *      with (to, from) = (0, 1)
 *          - fork a new thread with (to, from) = (0, 0)
 *          - fork a new thread with (to, from) = (1, 1)
 *          - and return
 *
 *          with (to, from) = (0, 0)
 *              - the thread binds to the place '0' and call the routine
 *              - and return
 *
 *          with (to, from) = (1, 1)
 *              - the thread binds to the place '1' and call the routine
 *              - and return
 *
 *      with (to, from) = (2, 3)
 *          - fork a new thread with (to, from) = (2, 2)
 *          - fork a new thread with (to, from) = (3, 3)
 *          - and return
 *
 *          with (to, from) = (2, 2)
 *              - the thread binds to the place '2' and call the routine
 *              - and return
 *
 *          with (to, from) = (3, 3)
 *              - the thread binds to the place '3' and call the routine
 *              - and return
 */

typedef struct  team_recursive_args_t
{
    xkrt_runtime_t * runtime;
    xkrt_team_t * team;
    cpu_set_t cpuset;
    int from;
    int to;
    int malloced;
}               team_recursive_args_t;

static void *
team_create_recursive(void * vargs)
{
    team_recursive_args_t * args = (team_recursive_args_t *) vargs;

    // recursion end
    if (args->from == args->to)
    {
        const int tid = args->from;
        args->team->priv.threads[tid].state = XKRT_THREAD_INITIALIZED;
        void * r = args->team->desc.routine(args->team, tid);
        free(args);
        return r;
    }

    // create threads
    const int pairs[2][2] = {
        {args->from,                                   args->from + (args->to - args->from) / 2},
        {args->from + (args->to - args->from) / 2 + 1, args->to}
    };

    // save calling thread cpu set
    cpu_set_t save_set;
    xkrt_runtime_t::thread_getaffinity(save_set);

    for (int i = 0 ; i < 2 ; ++i)
    {
        if (pairs[i][0] > pairs[i][1])
            continue ;

        team_recursive_args_t new_args = {
            .runtime    = args->runtime,
            .team       = args->team,
            .from       = pairs[i][0],
            .to         = pairs[i][1],
            .malloced   = 1
        };
        CPU_ZERO(&new_args.cpuset);

        // retrieve cpuset
        hwloc_obj_t pu = hwloc_get_pu_obj_by_os_index(args->runtime->topology, new_args.from);
        HWLOC_SAFE_CALL(
            hwloc_cpuset_to_glibc_sched_affinity(
                args->runtime->topology,
                pu->cpuset,
               &new_args.cpuset,
                sizeof(cpu_set_t)
            )
        );

        // move thread before allocating future thread-private memory
        xkrt_runtime_t::thread_setaffinity(new_args.cpuset);

        xkrt_thread_t * thread = new_args.team->priv.threads + new_args.from;

        team_recursive_args_t * dup = (team_recursive_args_t *) malloc(sizeof(team_recursive_args_t));
        memcpy(dup, &new_args, sizeof(team_recursive_args_t));
        pthread_create(&thread->pthread, NULL, team_create_recursive, dup);
    }

    // restore calling thread cpu set
    xkrt_runtime_t::thread_setaffinity(save_set);

    if (args->malloced)
        free(args);

    return NULL;
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
    team->priv.threads = (xkrt_thread_t *) malloc(sizeof(xkrt_thread_t) * team->desc.nthreads);
    assert(team->priv.threads);
    memset(team->priv.threads, 0, sizeof(xkrt_thread_t) * team->desc.nthreads);

    // init barrier with nthreads + 1 to avoid early barrier release
    team->priv.barrier.n.store(team->desc.nthreads + 1, std::memory_order_seq_cst);
    pthread_mutex_init(&team->priv.barrier.mtx, NULL);
    pthread_cond_init(&team->priv.barrier.cond, NULL);

    // init critical
    pthread_mutex_init(&team->priv.critical.mtx, NULL);

    switch (team->desc.binding.mode)
    {
        case (XKRT_TEAM_BINDING_MODE_COMPACT):
        {
            switch (team->desc.binding.places)
            {
                case (XKRT_TEAM_BINDING_PLACES_DEVICE):
                {
                    // save calling thread cpu set
                    cpu_set_t save_set;
                    xkrt_runtime_t::thread_getaffinity(save_set);

                    assert(team->desc.nthreads == this->drivers.devices.n);
                    for (xkrt_device_global_id_t device_global_id ; device_global_id < this->drivers.devices.n ; ++device_global_id)
                    {
                        const xkrt_device_t * device = this->device_get(device_global_id);
                        assert(device);

                        // move thread before allocating future thread-private memory
                        xkrt_runtime_t::thread_setaffinity(device->thread->cpuset);

                        xkrt_thread_t * thread = team->priv.threads + device_global_id;

                        xkrt_thread_trampoline_args_t * args = (xkrt_thread_trampoline_args_t *) malloc(sizeof(xkrt_thread_trampoline_args_t));
                        args->team = team;
                        args->tid = (int) device_global_id;
                        pthread_create(&thread->pthread, NULL, trampoline, args);
                    }

                    // restore calling thread cpu set
                    xkrt_runtime_t::thread_setaffinity(save_set);

                    break ;
                }

                case (XKRT_TEAM_BINDING_PLACES_CORE):
                {
                    team_recursive_args_t args = {
                        .runtime = this,
                        .team = team,
                        .from = 0,
                        .to = team->desc.nthreads - 1,
                        .malloced = 0
                    };
                    team_create_recursive(&args);
                    break ;
                }

                default:
                    LOGGER_FATAL("Team config. not supported");
            }
            break ;
        }

        case (XKRT_TEAM_BINDING_MODE_SPREAD):
        {
            LOGGER_FATAL("Team config. not supported");
            break ;
        }
    }

    // decrement barrier
    team_barrier_fetch(team, 1);
}

void
xkrt_runtime_t::team_barrier(xkrt_team_t * team)
{
    // TODO : reimplement this using team's topology

    int old_version = team->priv.barrier.version;
    if (team_barrier_fetch(team, 1))
    {
        while (old_version == team->priv.barrier.version)
        {
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

void
xkrt_runtime_t::team_join(xkrt_team_t * team)
{
    // TODO : reimpl this using team's topology
    for (int i = 0 ; i < team->desc.nthreads ; ++i)
    {
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
