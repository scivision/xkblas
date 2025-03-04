/* ************************************************************************** */
/*                                                                            */
/*   thread.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/04 04:09:30 by Romain PEREIRA            \_)     (_/    */
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
    int tid;
}               xkrt_thread_trampoline_args_t;

static void *
trampoline(void * vargs)
{
    xkrt_thread_trampoline_args_t * args = (xkrt_thread_trampoline_args_t *) vargs;
    assert(args);

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

void
xkrt_runtime_t::team_create(xkrt_team_t * team)
{
    // only supported modes currently
    assert(
        (team->desc.binding.mode == XKRT_TEAM_BINDING_MODE_COMPACT && team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_DEVICE) ||
        (team->desc.binding.mode == XKRT_TEAM_BINDING_MODE_SPREAD  && team->desc.binding.places == XKRT_TEAM_BINDING_PLACES_MACHINE)
    );

    // save calling thread cpu set
    cpu_set_t save_set;
    xkrt_runtime_t::thread_getaffinity(save_set);

    // set all to zero
    memset(&team->priv, 0, sizeof(team->priv));

    // allocate thread array
    team->priv.threads = (xkrt_thread_t *) malloc(sizeof(xkrt_thread_t) * team->desc.nthreads);
    assert(team->priv.threads);

    // init barrier with nthreads + 1 to avoid early barrier release
    team->priv.barrier.n.store(team->desc.nthreads + 1, std::memory_order_seq_cst);
    pthread_mutex_init(&team->priv.barrier.mtx, NULL);
    pthread_cond_init(&team->priv.barrier.cond, NULL);

    // init critical
    pthread_mutex_init(&team->priv.critical.mtx, NULL);

    // load hwloc topo
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);

    switch (team->desc.binding.mode)
    {
        case (XKRT_TEAM_BINDING_MODE_COMPACT):
        {
            switch (team->desc.binding.places)
            {
                case (XKRT_TEAM_BINDING_PLACES_DEVICE):
                {
                    assert(team->desc.nthreads == this->drivers.devices.n);
                    for (xkrt_device_global_id_t device_global_id ; device_global_id < this->drivers.devices.n ; ++device_global_id)
                    {
                        const xkrt_device_t * device = this->device_get(device_global_id);
                        assert(device);

                        xkrt_runtime_t::thread_setaffinity(device->thread->cpuset);

                        xkrt_thread_t * thread = team->priv.threads + device_global_id;
                        thread->state = XKRT_THREAD_INITIALIZED;

                        xkrt_thread_trampoline_args_t * args = (xkrt_thread_trampoline_args_t *) malloc(sizeof(xkrt_thread_trampoline_args_t));
                        args->team = team;
                        args->tid = (int) device_global_id;
                        pthread_create(&thread->pthread, NULL, trampoline, args);
                    }

                    break ;
                }

                default:
                    LOGGER_FATAL("Team config. not supported");
            }
            break ;
        }

        case (XKRT_TEAM_BINDING_MODE_SPREAD):
        {
            switch (team->desc.binding.places)
            {
                case (XKRT_TEAM_BINDING_PLACES_MACHINE):
                {
                    // TODO
                    break ;
                }

                default:
                    LOGGER_FATAL("Team config. not supported");
            }
            break ;
        }
    }

    // restore calling thread cpu set
    xkrt_runtime_t::thread_setaffinity(save_set);

    // decrement barrier
    team_barrier_fetch(team, 1);

    // release topo
    hwloc_topology_destroy(topology);
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
    for (int i = 0 ; i < team->desc.nthreads ; ++i)
    {
        assert(team->priv.threads[i].state == XKRT_THREAD_INITIALIZED);
        pthread_join(team->priv.threads[i].pthread, NULL);
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
