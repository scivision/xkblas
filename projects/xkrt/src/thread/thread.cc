/* ************************************************************************** */
/*                                                                            */
/*   thread.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/20 20:03:43 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/runtime.h>

# include <cassert>
# include <cstring>
# include <cerrno>

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
    xkrt_device_global_id_t device_global_id;
}               xkrt_thread_trampoline_args_t;

static void *
trampoline(void * vargs)
{
    xkrt_thread_trampoline_args_t * args = (xkrt_thread_trampoline_args_t *) vargs;
    assert(args);

    args->team->desc.routine(args->device_global_id, args->team->desc.args);
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

void
xkrt_runtime_t::team_create(xkrt_team_t * team)
{
    // save calling thread cpu set
    cpu_set_t save_set;
    xkrt_runtime_t::thread_getaffinity(save_set);

    // set all to zero
    memset(&team->priv, 0, sizeof(team->priv));

    // store the number of threads
    int max_nthreads = this->drivers.devices.n;
    int nthreads = this->drivers.devices.n;

    // init barrier - max_threads + 1 to avoid early barrier release
    team->priv.barrier.n.store(max_nthreads + 1, std::memory_order_seq_cst);
    pthread_mutex_init(&team->priv.barrier.mtx, NULL);
    pthread_cond_init(&team->priv.barrier.cond, NULL);

    // init critical
    pthread_mutex_init(&team->priv.critical.mtx, NULL);

    // fork 1 thread per device
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < this->drivers.devices.n ; ++device_global_id)
    {
        if (team->desc.devices & (1 << device_global_id))
        {
            const xkrt_device_t * device = this->device_get(device_global_id);
            if (device == NULL)
            {
                team->priv.threads[device_global_id].state = XKRT_THREAD_UNINITIALIZED;
                --nthreads;
                continue ;
            }
            team->priv.threads[device_global_id].state = XKRT_THREAD_INITIALIZED;

            xkrt_runtime_t::thread_setaffinity(device->thread->cpuset);

            xkrt_thread_trampoline_args_t * args = (xkrt_thread_trampoline_args_t *) malloc(sizeof(xkrt_thread_trampoline_args_t));
            assert(args);
            args->team = team;
            args->device_global_id = device_global_id;
            pthread_create(&team->priv.threads[device_global_id].pthread, NULL, trampoline, args);
        }
    }

    // restore calling thread cpu set
    xkrt_runtime_t::thread_setaffinity(save_set);

    // decrement barrier
    team->priv.nthreads = nthreads;
    mem_barrier();
    team_barrier_fetch(team, 1 + (max_nthreads - nthreads));
}

void
xkrt_runtime_t::team_barrier(xkrt_team_t * team)
{
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
    for (int i = 0 ; i < team->priv.nthreads ; ++i)
        if (team->priv.threads[i].state == XKRT_THREAD_INITIALIZED)
            pthread_join(team->priv.threads[i].pthread, NULL);
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
