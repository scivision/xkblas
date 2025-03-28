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
# include <stdlib.h>

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
static ze_module_handle_t module;
static ze_kernel_handle_t kernel;
static uint32_t n_queue_prop;
static ze_command_queue_group_properties_t * queue_prop;
static int ordinal_copy;
static int ordinal_compute;

const uint8_t SPIRV_EMPTY_KERNEL[] = {
#  include <xkbm/kernels/empty.spvbin>
};

/////////////////////////////////////
// LATENCY OF SUBMISSION IN QUEUES //
/////////////////////////////////////

typedef enum    immediate_t
{
    IMMEDIATE,
    NON_IMMEDIATE
}               immediate_t;

typedef enum    test_mode_t
{
    KERNEL,
    H2D,
    D2H
}               test_mode_t;

template<immediate_t immediate, test_mode_t mode>
static void
cmdlist_run(benchmark_node_t * node)
{
    // command list
    const uint32_t ordinal = (mode == KERNEL) ? ordinal_compute : ordinal_copy;
    const uint32_t index   = 0;
    const ze_command_queue_desc_t queue_desc = {
        .stype      = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
        .pNext      = NULL,
        .ordinal    = ordinal,
        .index      = index,
        // .flags      = ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
        .flags      = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY,
        // .mode       = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS,
        .mode       = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        .priority   = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW
    };
    ze_command_queue_handle_t queue;
    ze_command_list_handle_t list;

    if (immediate == IMMEDIATE)
        ZE_SAFE_CALL(zeCommandListCreateImmediate(context, device, &queue_desc, &list));
    else if (immediate == NON_IMMEDIATE)
    {
        ZE_SAFE_CALL(zeCommandQueueCreate(context, device, &queue_desc, &queue));

        // on ZE_COMMAND_LIST_FLAG_MAXIMIZE_THROUGHPUT
        // driver may perform additional optimizations that increase execution
        // throughput. using this flag may increase Host overhead of
        // zeCommandListClose and zeCommandQueueExecuteCommandLists. therefore,
        // this flag should not be set for low-latency usage-models.

        const uint32_t queue_ordinal = ordinal;
        ze_command_list_desc_t list_desc {
            .stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC,
            .pNext = NULL,
            .commandQueueGroupOrdinal = queue_ordinal,
            .flags = ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING,
            //  .flags = ZE_COMMAND_LIST_FLAG_MAXIMIZE_THROUGHPUT,
            // .flags = ZE_COMMAND_LIST_FLAG_IN_ORDER
        };
        ZE_SAFE_CALL(zeCommandListCreate(context, device, &list_desc, &list));
    }
    else
        LOGGER_FATAL("error");

    // timing
    constexpr int n = immediate == IMMEDIATE ? 1 : 8;
    time_array_t time(n, 100);

    // event pool
   const ze_event_pool_desc_t pool_desc = {
        .stype  = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext  = NULL,
        .flags  = ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        .count  = 1 << n
    };
    const uint32_t ndevices = 1;
    ze_event_pool_handle_t pool;
    ZE_SAFE_CALL(zeEventPoolCreate(context, &pool_desc, ndevices, &device, &pool));

    // event
    ze_event_desc_t event_desc = {
        .stype  = ZE_STRUCTURE_TYPE_EVENT_DESC,
        .signal = ZE_EVENT_SCOPE_FLAG_HOST,
        .wait   = ZE_EVENT_SCOPE_FLAG_HOST
    };
    ze_event_handle_t * events = (ze_event_handle_t *) malloc(sizeof(ze_event_handle_t) * pool_desc.count);
    for (int j = 0 ; j < pool_desc.count ; ++j)
        ZE_SAFE_CALL(zeEventCreate(pool, &event_desc, events + j));

    const uint32_t n_wait_events = 0;
    ze_event_handle_t * wait_events = nullptr;
    ze_group_count_t launch = { 1, 0, 0 };

    // allocate memory for latency test
    const size_t size = 1 << n;
    unsigned char * hostptr = (unsigned char *) calloc(1, size);
    const size_t alignment = 4 * sizeof(double);
    unsigned char * deviceptr = NULL;
    const ze_device_mem_alloc_desc_t device_desc = {
        .stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES,
        .pNext = NULL,
        .flags = 0,
        .ordinal = 0
    };
    ZE_SAFE_CALL(zeMemAllocDevice(context, &device_desc, size, alignment, device, (void **) &deviceptr));
    assert(deviceptr);
    ZE_SAFE_CALL(zeContextMakeMemoryResident(context, device, deviceptr, size));

    unsigned char * src = (mode == H2D) ? hostptr   : (mode == D2H) ? deviceptr : NULL;
    unsigned char * dst = (mode == H2D) ? deviceptr : (mode == D2H) ? hostptr   : NULL;

    const uint32_t n_list = 1;
    ze_fence_handle_t fence = NULL;

    if (immediate == IMMEDIATE)
    {
        ze_event_handle_t event = events[0];

        if (mode == KERNEL)
        {
            for (int iter = -10 ; iter < time.niters ; ++iter)
            {
                uint64_t t0 = xkrt_get_nanotime();
                ZE_SAFE_CALL(zeCommandListAppendLaunchKernel(list, kernel, &launch, event, n_wait_events, wait_events));
                ZE_SAFE_CALL(zeCommandListHostSynchronize(list, UINT64_MAX));
                uint64_t tf = xkrt_get_nanotime();
                if (iter >= 0)
                    time.set(0, iter, tf-t0);
                ZE_SAFE_CALL(zeEventHostReset(event));
            }
        }
        else if (mode == H2D || mode == D2H)
        {
            // run the test
            for (int iter = -10 ; iter < time.niters ; ++iter)
            {
                uint64_t t0 = xkrt_get_nanotime();
                ZE_SAFE_CALL(zeCommandListAppendMemoryCopy(list, dst, src, size, event, n_wait_events, wait_events));
                ZE_SAFE_CALL(zeCommandListHostSynchronize(list, UINT64_MAX));
                uint64_t tf = xkrt_get_nanotime();
                if (iter >= 0)
                    time.set(0, iter, tf-t0);
                ZE_SAFE_CALL(zeEventHostReset(event));
            }
        }
        else
            LOGGER_FATAL("error");
    }
    else if (immediate == NON_IMMEDIATE)
    {
        if (mode == KERNEL)
        {
            for (int i = 0 ; i < n ; ++i)
            {
                const int n_op = 1 << i;
                for (int iter = -10 ; iter < time.niters ; ++iter)
                {
                    uint64_t t0 = xkrt_get_nanotime();
                    for (int j = 0 ; j < n_op ; ++j)
                        ZE_SAFE_CALL(zeCommandListAppendLaunchKernel(list, kernel, &launch, events[j], n_wait_events, wait_events));
                    ZE_SAFE_CALL(zeCommandListClose(list));
                    ZE_SAFE_CALL(zeCommandQueueExecuteCommandLists(queue, n_list, &list, fence));
                    ZE_SAFE_CALL(zeCommandQueueSynchronize(queue, UINT64_MAX));
                    uint64_t tf = xkrt_get_nanotime();
                    if (iter >= 0)
                        time.set(i, iter, tf-t0);
                    for (int j = 0 ; j < n_op ; ++j)
                        ZE_SAFE_CALL(zeEventHostReset(events[j]));
                    ZE_SAFE_CALL(zeCommandListReset(list));
                }
            }
        }
        else if (mode == H2D || mode == D2H)
        {
            for (int i = 0 ; i < n ; ++i)
            {
                const int n_op = 1 << i;
                for (int iter = -10 ; iter < time.niters ; ++iter)
                {
                    uint64_t t0 = xkrt_get_nanotime();
                    for (int j = 0 ; j < n_op ; ++j)
                        ZE_SAFE_CALL(zeCommandListAppendMemoryCopy(list, dst + j, src + j, 1, events[j], n_wait_events, wait_events));
                    ZE_SAFE_CALL(zeCommandListClose(list));
                    ZE_SAFE_CALL(zeCommandQueueExecuteCommandLists(queue, n_list, &list, fence));
                    ZE_SAFE_CALL(zeCommandQueueSynchronize(queue, UINT64_MAX));
                    uint64_t tf = xkrt_get_nanotime();
                    if (iter >= 0)
                        time.set(i, iter, tf-t0);
                    for (int j = 0 ; j < n_op ; ++j)
                        ZE_SAFE_CALL(zeEventHostReset(events[j]));
                    ZE_SAFE_CALL(zeCommandListReset(list));
                }
            }
        }
    }
    else
        LOGGER_FATAL("error");

    // release memory
    free(hostptr);
    ZE_SAFE_CALL(zeMemFree(context, deviceptr));

    // report
    auto convert = [] (char * buffer, size_t buffer_size, int i) { snprintf(buffer, buffer_size, "%d", 1<<i); };
    time.report<METRIC_TIME>(mode == KERNEL ? "Launch Kernel" : mode == H2D ? "Launch H2D" : "Unknown", convert);

    // release stuff
    ZE_SAFE_CALL(zeEventPoolDestroy(pool));
    ZE_SAFE_CALL(zeCommandListDestroy(list));
    if (immediate == NON_IMMEDIATE)
        ZE_SAFE_CALL(zeCommandQueueDestroy(queue));
}

static benchmark_node_t non_immediate_kernel = {
    .name = "kernel",
    .desc = "Latency of a kernel launch to an non-immediate command list",
    .run = cmdlist_run<NON_IMMEDIATE, KERNEL>
};

static benchmark_node_t non_immediate_h2d = {
    .name = "h2d",
    .desc = "Latency of a H2D copy to an non-immediate command list",
    .run = cmdlist_run<NON_IMMEDIATE, H2D>
};

static benchmark_node_t non_immediate_d2h = {
    .name = "d2h",
    .desc = "Latency of a D2H copy to an non-immediate command list",
    .run = cmdlist_run<NON_IMMEDIATE, D2H>
};

static benchmark_node_t non_immediate = {
    .name = "non-immediate",
    .desc = "Cost of non-immediate command list",
};

static benchmark_node_t immediate_kernel = {
    .name = "kernel",
    .desc = "Latency of a kernel launch to an immediate command list",
    .run = cmdlist_run<IMMEDIATE, KERNEL>
};

static benchmark_node_t immediate_h2d = {
    .name = "h2d",
    .desc = "Latency of a H2D copy to an immediate command list",
    .run = cmdlist_run<IMMEDIATE, H2D>
};

static benchmark_node_t immediate_d2h = {
    .name = "d2h",
    .desc = "Latency of a D2H copy to an immediate command list",
    .run = cmdlist_run<IMMEDIATE, D2H>
};

static benchmark_node_t immediate = {
    .name = "immediate",
    .desc = "Cost of immediate command list",
};

static benchmark_node_t cmdlist = {
    .name = "cmdlist",
    .desc = "Latency of using command lists",
};

///////////////////
// ZE BENCHMARKS //
///////////////////

static benchmark_node_t ze = {
    .name = "ze",
    .desc = "Metrics on ZE-supported devices",
    .enabled = 1
};

void
ze_benchmark_push(benchmark_node_t * parent)
{
    benchmark_push_children(parent, &ze);

    # define LINK(X, Y) benchmark_push_children(&X, &Y)
    LINK(ze, cmdlist);

    LINK(cmdlist, immediate);
    LINK(immediate, immediate_h2d);
    LINK(immediate, immediate_d2h);
    LINK(immediate, immediate_kernel);

    LINK(cmdlist, non_immediate);
    LINK(non_immediate, non_immediate_h2d);
    LINK(non_immediate, non_immediate_d2h);
    LINK(non_immediate, non_immediate_kernel);
}

void
ze_benchmark_init(void)
{
    ze_init_flag_t initFlags = ZE_INIT_FLAG_GPU_ONLY;
    ZE_SAFE_CALL(zeInit(initFlags));

    // driver
    uint32_t driverCount = 1;
    ZE_SAFE_CALL(zeDriverGet(&driverCount, &driver));

    // device
    uint32_t deviceCount = 1;
    ZE_SAFE_CALL(zeDeviceGet(driver, &deviceCount, &device));

    // context
    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ZE_SAFE_CALL(zeContextCreate(driver, &contextDesc, &context));

    // module
    ze_module_desc_t module_desc = {
        .stype = ZE_STRUCTURE_TYPE_MODULE_DESC,
        .pNext = NULL,
        .format = ZE_MODULE_FORMAT_IL_SPIRV,
        .inputSize = sizeof(SPIRV_EMPTY_KERNEL),
        .pInputModule = SPIRV_EMPTY_KERNEL,
        .pBuildFlags = NULL,
        .pConstants = NULL
    };
    ZE_SAFE_CALL(zeModuleCreate(context, device, &module_desc, &module, NULL));

    // kernel
    ze_kernel_desc_t kernel_desc = {
        .stype = ZE_STRUCTURE_TYPE_KERNEL_DESC,
        .pNext = NULL,
        .flags = ZE_KERNEL_FLAG_FORCE_RESIDENCY,
        .pKernelName = "empty_kernel"
    };
    ZE_SAFE_CALL(zeKernelCreate(module, &kernel_desc, &kernel));

    // command list/queue
    ZE_SAFE_CALL(zeDeviceGetCommandQueueGroupProperties(device, &n_queue_prop, NULL));
    queue_prop = (ze_command_queue_group_properties_t *) malloc(sizeof(ze_command_queue_group_properties_t) * n_queue_prop);
    ZE_SAFE_CALL(zeDeviceGetCommandQueueGroupProperties(device, &n_queue_prop, queue_prop));

    for (uint32_t i = 0; i < n_queue_prop ; ++i)
        if (queue_prop[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)
            ordinal_compute = i;

    for (uint32_t i = 0; i < n_queue_prop ; ++i)
        if (queue_prop[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY)
            ordinal_copy = i;
}

void
ze_benchmark_deinit(void)
{
    ZE_SAFE_CALL(zeModuleDestroy(module));
    ZE_SAFE_CALL(zeContextDestroy(context));
}
