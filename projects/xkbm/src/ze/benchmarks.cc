/* ************************************************************************** */
/*                                                                            */
/*   benchmarks.cc                                                            */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/26 00:40:42 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/04 03:36:52 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <stddef.h>

# include <ze_api.h>

# include <xkbm/allocator.h>
# include <xkbm/benchmark.h>
# include <xkbm/topology.h>
# include <xkbm/time.h>

# include <xkrt/logger/logger.h>
# include <xkrt/logger/logger-ze.h>
# include <xkrt/logger/metric.h>

static ze_driver_handle_t driver;
static ze_device_handle_t device;
static ze_context_handle_t context;

/////////////////////////////////////
// LATENCY OF SUBMISSION IN QUEUES //
/////////////////////////////////////

static void
cmdlist_immediate_latency_run(benchmark_node_t * node)
{
    const int ordinal = 0;
    const int index   = 0;

    // command list
    const queue_desc_t queue_desc = {
        .stype      = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .pNext      = NULL,
        .ordinal    = ordinal,
        .index      = index,
        .flags      = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY,
        .mode       = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        .priority   = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW
    };
    ze_command_list_t list;
    ZE_SAFE_CALL(
        zeCommandListCreateImmediate(
            context,
            device,
           &queue_desc,
           &list
        )
    );

    // event pool
    const ze_event_pool_desc_t ze_event_pool_desc = {
        .stype  = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext  = NULL,
        .flags  = ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        .count  = 1
    };
    ze_event_pool_handle_t pool;
    ZE_SAFE_CALL(zeEventPoolCreate(context, &pool_desc, 1, &device, &pool));

    // event
    ze_event_desc_t event_desc = {
        .stype  = ZE_STRUCTURE_TYPE_EVENT_DESC,
        .signal = ZE_EVENT_SCOPE_FLAG_HOST,
        .wait   = ZE_EVENT_SCOPE_FLAG_HOST
    };
    ze_event_handle_t event;
    ZE_SAFE_CALL(zeEventCreate(pool, &desc, &event));

    // allocate memory for latency test
    const size_t size = 1;
    const void * src = calloc(1, size);

    const ze_device_mem_alloc_desc_t device_desc = {
        .stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES,
        .pNext = NULL,
        .flags = 0,
        .ordinal = 0
    };
    const size_t alignment = 4 * sizeof(double);
    void * dst = NULL;
    ZE_SAFE_CALL(zeMemAllocDevice(context, &device_desc, size, alignment, device, &dst));
    assert(dst);
    ZE_SAFE_CALL(zeContextMakeMemoryResident(context, device, dst, size));

    // run the test
    for (...)
    {
        ZE_SAFE_CALL(zeCommandListAppendMemoryCopy(list, dst, src, size, event, n_wait_events, wait_events));
        ZE_SAFE_CALL(zeCommandListHostSynchronize(list, UINT64_MAX));

    }

    // release stuff
    ZE_SAFE_CALL(zeEventPoolDestroy(pool));
    ZE_SAFE_CALL(zeCommandListDestroy(list));

    free(src);
}

static benchmark_node_t cmdlist_immediate_latency = {
    .name = "CMDLIST-IMMEDIATE",
    .desc = "Latency of submitting a H2D copy to an immediate command list",
    .run = cmdlist_immediate_latency_run
};

///////////////////
// ZE BENCHMARKS //
///////////////////

static benchmark_node_t ze = {
    .name = "ze",
    .desc = "Metrics on ZE-supported devices",
    .run = check_devices,
    .enabled = 1
};

void
ze_benchmark_push(benchmark_node_t * parent)
{
    benchmark_push_children(parent, &ze);

    # define LINK(X, Y) benchmark_push_children(&X, &Y)
    LINK(ze, cmdlist);
}

void
ze_benchmark_init(void)
{
    ze_init_flag_t initFlags = ZE_INIT_FLAG_GPU_ONLY;
    ZE_SAFE_CALL(zeInit(initFlags));

    uint32_t driverCount = 1;
    ZE_SAFE_CALL(zeDriverGet(&driverCount, &driver));

    uint32_t deviceCount = 1;
    ZE_SAFE_CALL(zeDeviceGet(driver, &deviceCount, &device));

    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ZE_SAFE_CALL(zeContextCreate(driver, &contextDesc, &context));
}

void
ze_benchmark_deinit(void)
{
    ZE_SAFE_CALL(zeContextDestroy(context));
}
