/* ************************************************************************** */
/*                                                                            */
/*   team-cpus.cc                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/19 23:49:10 by Romain PEREIRA            \_)     (_/    */
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
static std::atomic<int> counter;

static void *
main_team(xkrt_team_t * team, xkrt_thread_t * thread)
{
    int cpu = sched_getcpu();
    LOGGER_INFO("Thread `%3d` running on `sched_getcpu() -> %3d`", thread->tid, cpu);
    ++counter;
    return NULL;
}

static int
get_ncpus(void)
{
    // Try to get the number of CPU cores from topology
    int depth = hwloc_get_type_depth(runtime.topology, HWLOC_OBJ_CORE);
    int r = hwloc_get_nbobjs_by_depth(runtime.topology, depth);
    return r;
}

int
main(void)
{
    assert(xkrt_init(&runtime) == 0);

    // team of 1 thread
    xkrt_team_t team = {
        .desc = {
            .routine = main_team,
            .args = NULL,
            .nthreads = 1,
            .binding = {
                .mode = XKRT_TEAM_BINDING_MODE_COMPACT,
                .places = XKRT_TEAM_BINDING_PLACES_CORE,
                .flags = XKRT_TEAM_BINDING_FLAG_NONE
            }
        }
    };
    runtime.team_create(&team);
    runtime.team_join(&team);
    assert(counter == 1);

    // team on all cpus
    team.desc.nthreads = get_ncpus();
    runtime.team_create(&team);
    runtime.team_join(&team);
    assert(counter == 1 + get_ncpus());

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
