/* ************************************************************************** */
/*                                                                            */
/*   benchmarks.cc                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/26 00:40:42 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/26 04:47:23 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <stddef.h>

# include <ze_api.h>

# include <xkbm/allocator.h>
# include <xkbm/benchmark.h>
# include <xkbm/thread.h>
# include <xkbm/topology.h>
# include <xkbm/time.h>

# include <xkrt/logger/logger.h>
# include <xkrt/logger/logger-ze.h>
# include <xkrt/logger/metric.h>

////////////////
// H2D or D2H //
////////////////

# if 0
static benchmark_node_t ze_benchmarks_h2d = {
    .name = "H2D",
    .desc = "Host memory to device (global) memory bandwidth",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = ze_benchmarks_mem_run,
    .enabled = 1
};
# endif

///////////////////
// ZE BENCHMARKS //
///////////////////

static benchmark_node_t ze_benchmarks = {
    .name = "ze",
    .desc = "Metrics on ZE-supported devices",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = NULL,
    .enabled = 1
};

void
ze_benchmark_push(benchmark_node_t * parent)
{
    benchmark_push_children(parent, &ze_benchmarks);
    // benchmark_push_children(&ze_benchmarks, &ze_benchmarks_h2d);
}

void
ze_benchmark_init(void)
{
}

void
ze_benchmark_deinit(void)
{
}
