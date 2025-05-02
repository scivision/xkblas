/* ************************************************************************** */
/*                                                                            */
/*   team-cpus-parallel-for.cc                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/02 21:33:00 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
# include <sched.h>

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static xkrt_runtime_t runtime;

static int
get_ncpus(void)
{
    return 20;

    // Try to get the number of CPU cores from topology
    int depth = hwloc_get_type_depth(runtime.topology, HWLOC_OBJ_CORE);
    int r = hwloc_get_nbobjs_by_depth(runtime.topology, depth);
    return r;
}

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    // team on all cpus
    int ncpus = get_ncpus();
    xkrt_team_t team = {
        .desc = {
            .routine = XKRT_TEAM_ROUTINE_PARALLEL_FOR,
            .args = NULL,
            .nthreads = ncpus,
            .binding = {
                .mode = XKRT_TEAM_BINDING_MODE_COMPACT,
                .places = XKRT_TEAM_BINDING_PLACES_CORE,
                .flags = XKRT_TEAM_BINDING_FLAG_NONE
            }
        }
    };

    // TEST 1
    // create and destroy the team without working
    {
        std::atomic<int> counter(0);
        runtime.team_create(&team);
        runtime.team_join(&team);
        assert(counter == 0);
    }

    // TEST 2
    // create, dispatch 1 function, destroy
    {
        std::atomic<int> counter(0);
        runtime.team_create(&team);
        runtime.team_parallel_for(&team, [&counter] (xkrt_team_t * team, xkrt_thread_t * thread) {
                LOGGER_INFO("Thread `%3d` running on `sched_getcpu() -> %3d`", thread->tid, sched_getcpu());
                ++counter;
            }
        );
        runtime.team_join(&team);
        assert(counter == ncpus);
    }

    // TEST 3
    // create, dispatch 'n' functions, destroy
    {
        constexpr int n = 10000;
        std::atomic<int> counter(0);
        runtime.team_create(&team);

        uint64_t t0 = xkrt_get_nanotime();
        for (int i = 0 ; i < n ; ++i)
        {
            runtime.team_parallel_for(&team, [&counter] (xkrt_team_t * team, xkrt_thread_t * thread) {
                }
            );
        }
        uint64_t tf = xkrt_get_nanotime();
        LOGGER_INFO("`%d` empty parallel on `%d` threads for took %lf s - that is %luns/task\n", n, ncpus, (tf-t0)/1e9, (tf-t0)/(n*ncpus));

        runtime.team_join(&team);
    }
    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
