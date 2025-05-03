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
static ze_context_handle_t context;
static ze_device_handle_t device;
static ze_device_handle_t device2;
static uint32_t n_queue_prop;
static ze_command_queue_group_properties_t * queue_prop;
static int ordinal_copy;
static int ordinal_compute;

const uint8_t SPIRV_EMPTY_KERNEL[] = {
#  include <xkbm/kernels/empty.ar.bytes>
// #  include <xkbm/kernels/empty.spv.bytes>
};

const uint8_t SPIRV_SLEEP_KERNEL[] = {
#  include <xkbm/kernels/sleep.ar.bytes>
// #  include <xkbm/kernels/sleep.spv.bytes>
};


typedef struct  module_t
{
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    const uint8_t * bytes;
    size_t nbytes;
    const char * funcname;
}               module_t;

# define N_KERNELS 2
# define KERNEL_EMPTY 0
# define KERNEL_SLEEP 1

static module_t modules[N_KERNELS] = {
    {
        .module = nullptr,
        .kernel = nullptr,
        .bytes = (const uint8_t *) SPIRV_EMPTY_KERNEL,
        .nbytes = sizeof(SPIRV_EMPTY_KERNEL),
        .funcname = "empty_kernel"
    },
    {
        .module = nullptr,
        .kernel = nullptr,
        .bytes = (const uint8_t *) SPIRV_SLEEP_KERNEL,
        .nbytes = sizeof(SPIRV_SLEEP_KERNEL),
        .funcname = "sleep_kernel"
    },

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
    D2H,
    D2D
}               test_mode_t;

typedef enum concurrency_mode_t : uint8_t
{
    SERIAL=0,
    CONCURRENT=1
}           concurrency_mode_t;

template<immediate_t immediate, test_mode_t mode>
static void
cmdlist_run(benchmark_node_t * node)
{
    (void) node;

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
    //  if immediate, only need to measure one
    //  if non-immediate, measure (1, 2, 4, ..., 128) batched in a cmdlist given to a cmdqueue
    //  do 100 iterations
    constexpr int n = immediate == IMMEDIATE ? 1 : 8;
    constexpr int warmup = 10;
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
    ze_event_handle_t * events = (ze_event_handle_t *) malloc(sizeof(ze_event_handle_t) * pool_desc.count);
    for (unsigned int j = 0 ; j < pool_desc.count ; ++j)
    {
        ze_event_desc_t event_desc = {
            .stype  = ZE_STRUCTURE_TYPE_EVENT_DESC,
            .pNext  = NULL,
            .index  = j,
            .signal = ZE_EVENT_SCOPE_FLAG_HOST,
            .wait   = ZE_EVENT_SCOPE_FLAG_HOST
        };
        ZE_SAFE_CALL(zeEventCreate(pool, &event_desc, events + j));
    }

    const uint32_t n_wait_events = 0;
    ze_event_handle_t * wait_events = nullptr;
    ze_group_count_t launch = { 1, 1, 1 };

    // allocate memory
    const size_t size = 1 << n;
    const size_t alignment = 4 * sizeof(double);
    const ze_device_mem_alloc_desc_t device_desc = {
        .stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES,
        .pNext = NULL,
        .flags = 0,
        .ordinal = 0
    };

    // device ptr
    unsigned char * deviceptr = NULL;
    ZE_SAFE_CALL(zeMemAllocDevice(context, &device_desc, size, alignment, device, (void **) &deviceptr));
    assert(deviceptr);
    ZE_SAFE_CALL(zeContextMakeMemoryResident(context, device, deviceptr, size));

    // other ptr (host or device)
    unsigned char * otherptr;
    if (mode == D2H || mode == H2D)
    {
        otherptr = (unsigned char *) calloc(1, size);
    }
    else if (mode == D2D)
    {
        ZE_SAFE_CALL(zeMemAllocDevice(context, &device_desc, size, alignment, device2, (void **) &otherptr));
        assert(otherptr);
        ZE_SAFE_CALL(zeContextMakeMemoryResident(context, device2, otherptr, size));
    }

    unsigned char * src = (mode == H2D) ? otherptr  : (mode == D2H) ? deviceptr : (mode == D2D) ? deviceptr : NULL;
    unsigned char * dst = (mode == H2D) ? deviceptr : (mode == D2H) ? otherptr  : (mode == D2D) ? otherptr  : NULL;

    const uint32_t n_list = 1;
    ze_fence_handle_t fence = NULL;

    if (immediate == IMMEDIATE)
    {
        ze_event_handle_t event = events[0];

        if (mode == KERNEL)
        {
            for (int iter = -warmup ; iter < time.niters ; ++iter)
            {
                uint64_t t0 = xkrt_get_nanotime();
                ZE_SAFE_CALL(zeCommandListAppendLaunchKernel(list, modules[KERNEL_EMPTY].kernel, &launch, event, n_wait_events, wait_events));
                ZE_SAFE_CALL(zeCommandListHostSynchronize(list, UINT64_MAX));
                uint64_t tf = xkrt_get_nanotime();
                if (iter >= 0)
                    time.set(0, iter, tf-t0);
                ZE_SAFE_CALL(zeEventHostReset(event));
            }
        }
        else if (mode == H2D || mode == D2H || mode == D2D)
        {
            // run the test
            for (int iter = -warmup ; iter < time.niters ; ++iter)
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
                for (int iter = -warmup ; iter < time.niters ; ++iter)
                {
                    uint64_t t0 = xkrt_get_nanotime();
                    for (int j = 0 ; j < n_op ; ++j)
                        ZE_SAFE_CALL(zeCommandListAppendLaunchKernel(list, modules[KERNEL_EMPTY].kernel, &launch, events[j], n_wait_events, wait_events));
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
                for (int iter = -warmup ; iter < time.niters ; ++iter)
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
        else
        {
            LOGGER_FATAL("error");
        }
    }
    else
        LOGGER_FATAL("error");

    // release memory
    if (mode == D2H || mode == H2D)
        free(otherptr);
    else if (mode == D2D)
        ZE_SAFE_CALL(zeMemFree(context, otherptr));
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

template<immediate_t immediate>
static void
relaxed_ordering_run(benchmark_node_t * node)
{
    (void) node;
    assert(immediate == NON_IMMEDIATE);

    // command list
    const uint32_t ordinal = ordinal_compute;
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

    // event pool
    constexpr int n_op = 3;
    const ze_event_pool_desc_t pool_desc = {
        .stype  = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        .pNext  = NULL,
        .flags  = ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
        .count  = n_op
    };
    const uint32_t ndevices = 1;
    ze_event_pool_handle_t pool;
    ZE_SAFE_CALL(zeEventPoolCreate(context, &pool_desc, ndevices, &device, &pool));

    // event
    ze_event_handle_t * events = (ze_event_handle_t *) malloc(sizeof(ze_event_handle_t) * pool_desc.count);
    for (uint32_t j = 0 ; j < pool_desc.count ; ++j)
    {
        ze_event_desc_t event_desc = {
            .stype  = ZE_STRUCTURE_TYPE_EVENT_DESC,
            .pNext  = NULL,
            .index  = j,
            .signal = ZE_EVENT_SCOPE_FLAG_HOST,
            .wait   = ZE_EVENT_SCOPE_FLAG_HOST
        };
        ZE_SAFE_CALL(zeEventCreate(pool, &event_desc, events + j));
    }

    ze_group_count_t launch = { 1, 1, 1 };

    const uint32_t n_list = 1;
    ze_fence_handle_t fence = NULL;

    ze_kernel_handle_t kernel = modules[KERNEL_SLEEP].kernel;

    // CONCURRENT DAG IS
    //
    //  T1 -> T2
    //  T3
    //
    // SERIAL DAG IS
    //  T1 -> T2 -> T3
    //
    //  all tasks have the same workload w
    //
    //  If serial, we expect total time to 3.w
    //  If concurrent, we expect total time to be 2.w
    //
    const unsigned long int w = 100000;

    // timing
    constexpr int n = 2;        // 0 == serial, 1 == concurrent
    constexpr int niter = 100;
    constexpr int warmup_iters = 0;
    time_array_t time(n, niter);

    if (immediate == IMMEDIATE)
    {
        LOGGER_FATAL("No relaxed ordering with immediate lists");
    }
    else if (immediate == NON_IMMEDIATE)
    {
        for (int iter = warmup_iters ; iter < time.niters ; ++iter)
        {
            for (unsigned int mode = 0 ; mode < time.nelements ; ++mode)
            {
                uint64_t t0 = xkrt_get_nanotime();

                // T1
                {
                    const int task_id = 0;
                    const uint32_t n_wait_events = 0;
                    ze_event_handle_t * wait_events = nullptr;
                    ZE_SAFE_CALL(zeKernelSetArgumentValue(kernel, 0, sizeof(unsigned long int), &w));
                    ZE_SAFE_CALL(zeKernelSetArgumentValue(kernel, 1, sizeof(unsigned long int), &task_id));
                    ZE_SAFE_CALL(zeCommandListAppendLaunchKernel(list, kernel, &launch, events[0], n_wait_events, wait_events));
                }

                // T2
                {
                    const int task_id = 1;
                    const uint32_t n_wait_events = 1;
                    ze_event_handle_t * wait_events = &events[0];   // depends on T1
                    ZE_SAFE_CALL(zeKernelSetArgumentValue(kernel, 0, sizeof(unsigned long int), &w));
                    ZE_SAFE_CALL(zeKernelSetArgumentValue(kernel, 1, sizeof(unsigned long int), &task_id));
                    ZE_SAFE_CALL(zeCommandListAppendLaunchKernel(list, kernel, &launch, events[1], n_wait_events, wait_events));
                }

                // T3
                {
                    const int task_id = 2;
                    const uint32_t n_wait_events    = mode == SERIAL ?          1 :    0; // depends on T2 in SERIAL node
                    ze_event_handle_t * wait_events = mode == SERIAL ? &events[1] : NULL;
                    ZE_SAFE_CALL(zeKernelSetArgumentValue(kernel, 0, sizeof(unsigned long int), &w));
                    ZE_SAFE_CALL(zeKernelSetArgumentValue(kernel, 1, sizeof(unsigned long int), &task_id));
                    ZE_SAFE_CALL(zeCommandListAppendLaunchKernel(list, kernel, &launch, events[2], n_wait_events, wait_events));
                }

                ZE_SAFE_CALL(zeCommandListClose(list));
                ZE_SAFE_CALL(zeCommandQueueExecuteCommandLists(queue, n_list, &list, fence));
                ZE_SAFE_CALL(zeCommandQueueSynchronize(queue, UINT64_MAX));
                uint64_t tf = xkrt_get_nanotime();
                if (iter >= 0)
                    time.set(mode, iter, tf-t0);
                for (int j = 0 ; j < n_op ; ++j)
                    ZE_SAFE_CALL(zeEventHostReset(events[j]));
                ZE_SAFE_CALL(zeCommandListReset(list));
            }
        }
    }
    else
        LOGGER_FATAL("error");

    // report
    auto convert = [] (char * buffer, size_t buffer_size, int i) { snprintf(buffer, buffer_size, "%s", i == SERIAL ? "Serial" : i == CONCURRENT ? "Concurrent" : "Unknown"); };
    time.report<METRIC_TIME>("Time", convert);

    // release stuff
    ZE_SAFE_CALL(zeEventPoolDestroy(pool));
    ZE_SAFE_CALL(zeCommandListDestroy(list));
    if (immediate == NON_IMMEDIATE)
        ZE_SAFE_CALL(zeCommandQueueDestroy(queue));
}

static benchmark_node_t non_immediate_kernel = {
    .name = "kernel",
    .desc = "Latency of a kernel launch to an non-immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cmdlist_run<NON_IMMEDIATE, KERNEL>,
    .enabled = 0

};

static benchmark_node_t non_immediate_h2d = {
    .name = "h2d",
    .desc = "Latency of a H2D copy to an non-immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cmdlist_run<NON_IMMEDIATE, H2D>,
    .enabled = 0
};

static benchmark_node_t non_immediate_d2h = {
    .name = "d2h",
    .desc = "Latency of a D2H copy to an non-immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cmdlist_run<NON_IMMEDIATE, D2H>,
    .enabled = 0
};

static benchmark_node_t non_immediate = {
    .name = "non-immediate",
    .desc = "Cost of non-immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = NULL,
    .enabled = 0
};

static benchmark_node_t non_immediate_relaxed_ordering = {
    .name = "relaxed_ordering",
    .desc = "Test if relaxed ordering avoid bubbles in command queues with non-immediate lists. Serial is 'T1 -> T2 -> T3' while Concurrent is 'T1 -> T2' and 'T3' - submitting in the same order (T1, T2, T3) in both",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = relaxed_ordering_run<NON_IMMEDIATE>,
    .enabled = 0
};

static benchmark_node_t immediate_kernel = {
    .name = "kernel",
    .desc = "Latency of a kernel launch to an immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cmdlist_run<IMMEDIATE, KERNEL>,
    .enabled = 0
};

static benchmark_node_t immediate_h2d = {
    .name = "h2d",
    .desc = "Latency of a H2D copy to an immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cmdlist_run<IMMEDIATE, H2D>,
    .enabled = 0
};

static benchmark_node_t immediate_d2h = {
    .name = "d2h",
    .desc = "Latency of a D2H copy to an immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cmdlist_run<IMMEDIATE, D2H>,
    .enabled = 0
};

static benchmark_node_t immediate_d2d = {
    .name = "d2d",
    .desc = "Latency of a D2D copy to an immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cmdlist_run<IMMEDIATE, D2D>,
    .enabled = 0
};

static benchmark_node_t immediate = {
    .name = "immediate",
    .desc = "Cost of immediate command list",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = NULL,
    .enabled = 0
};

static benchmark_node_t cmdlist = {
    .name = "cmdlist",
    .desc = "Latency of using command lists",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = NULL,
    .enabled = 0
};

///////////////////
// ZE BENCHMARKS //
///////////////////

static benchmark_node_t ze = {
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
    benchmark_push_children(parent, &ze);

    # define LINK(X, Y) benchmark_push_children(&X, &Y)
    LINK(ze, cmdlist);

    LINK(cmdlist,   immediate);
    LINK(immediate, immediate_h2d);
    LINK(immediate, immediate_d2h);
    LINK(immediate, immediate_d2d);
    LINK(immediate, immediate_kernel);

    LINK(cmdlist,       non_immediate);
    LINK(non_immediate, non_immediate_h2d);
    LINK(non_immediate, non_immediate_d2h);
    LINK(non_immediate, non_immediate_kernel);
    LINK(non_immediate, non_immediate_relaxed_ordering);
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
    ze_device_handle_t devices[2];
    uint32_t deviceCount = 2;
    ZE_SAFE_CALL(zeDeviceGet(driver, &deviceCount, devices));
    assert(deviceCount == 2);
    device  = devices[0];
    device2 = devices[1];

    ze_bool_t can_access = 0;
    ZE_SAFE_CALL(zeDeviceCanAccessPeer(device, device2, &can_access));
    assert(can_access);

    // context
    ze_context_desc_t contextDesc = {
        .stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC,
        .pNext = NULL,
        .flags = 0
    };
    ZE_SAFE_CALL(zeContextCreate(driver, &contextDesc, &context));

    for (int i = 0 ; i < N_KERNELS ; ++i)
    {
        module_t * m = modules + i;

        // module
        ze_module_desc_t module_desc = {
            .stype = ZE_STRUCTURE_TYPE_MODULE_DESC,
            .pNext = NULL,
            // .format = ZE_MODULE_FORMAT_IL_SPIRV,
            .format = ZE_MODULE_FORMAT_NATIVE,
            .inputSize = m->nbytes,
            .pInputModule = m->bytes,
            .pBuildFlags = NULL,
            .pConstants = NULL
        };
        ZE_SAFE_CALL(zeModuleCreate(context, device, &module_desc, &m->module, NULL));

        // kernel
        ze_kernel_desc_t kernel_desc = {
            .stype = ZE_STRUCTURE_TYPE_KERNEL_DESC,
            .pNext = NULL,
            .flags = ZE_KERNEL_FLAG_FORCE_RESIDENCY,
            .pKernelName = m->funcname
        };
        ZE_SAFE_CALL(zeKernelCreate(m->module, &kernel_desc, &m->kernel));
    }

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
    for (int i = 0 ; i < N_KERNELS ; ++i)
        ZE_SAFE_CALL(zeModuleDestroy(modules[i].module));
    ZE_SAFE_CALL(zeContextDestroy(context));
}
