/* ************************************************************************** */
/*                                                                            */
/*   benchmarks.cc                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/26 00:40:42 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/02 16:25:13 by Romain PEREIRA            \_)     (_/    */
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

static ze_driver_handle_t driverHandle;
static ze_device_handle_t deviceHandle;
static ze_context_handle_t contextHandle;

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
    ze_init_flag_t initFlags = ZE_INIT_FLAG_GPU_ONLY;
    ZE_SAFE_CALL(zeInit(initFlags));

    uint32_t driverCount = 1;
    ZE_SAFE_CALL(zeDriverGet(&driverCount, &driverHandle));

    uint32_t deviceCount = 1;
    ZE_SAFE_CALL(zeDeviceGet(driverHandle, &deviceCount, &deviceHandle));

    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ZE_SAFE_CALL(zeContextCreate(driverHandle, &contextDesc, &contextHandle));
}

void
ze_benchmark_deinit(void)
{
    ZE_SAFE_CALL(zeContextDestroy(contextHandle));
}
