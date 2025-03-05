/* ************************************************************************** */
/*                                                                            */
/*   team-cpus.cc                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/03/03 01:28:08 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/05 05:19:28 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# define _GNU_SOURCE
# include <sched.h>

# include <xkrt/xkrt.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>

static xkrt_runtime_t runtime;

static void *
main_team(xkrt_team_t * team, int tid)
{
    int cpu = sched_getcpu();
    LOGGER_INFO("Thread `%3d` running on `sched_getcpu() -> %3d`", tid, cpu);
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

    xkrt_team_t team = {
        .desc = {
            .routine = main_team,
            .args = NULL,
            .nthreads = get_ncpus(),
            .binding = {
                .mode = XKRT_TEAM_BINDING_MODE_COMPACT,
                .places = XKRT_TEAM_BINDING_PLACES_CORE,
            }
        }
    };

    runtime.team_create(&team);
    runtime.team_join(&team);

    assert(xkrt_deinit(&runtime) == 0);

    return 0;
}
