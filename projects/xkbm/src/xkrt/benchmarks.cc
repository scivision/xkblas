# include <stddef.h>

# include <xkbm/allocator.h>
# include <xkbm/benchmark.h>
# include <xkbm/thread.h>
# include <xkbm/topology.h>
# include <xkbm/time.h>

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>
# include <xkrt/logger/metric.h>
# include <xkrt/logger/logger.h>

static xkrt_runtime_t runtime;

static void
pp_int_3bw(size_t i, size_t min, size_t med, size_t max)
{
    char buffer[3][64];
    xkrt_metric_bandwidth(buffer[0], sizeof(buffer[0]), min);
    xkrt_metric_bandwidth(buffer[1], sizeof(buffer[1]), med);
    xkrt_metric_bandwidth(buffer[2], sizeof(buffer[2]), max);
    LOGGER_INFO("%10lu | %10s | %10s | %10s", i, buffer[0], buffer[1], buffer[2]);
}

static void
pp_1byte_3time(size_t i, size_t min, size_t med, size_t max)
{
    char buffer[4][64];
    xkrt_metric_byte(buffer[0], sizeof(buffer[0]), ((size_t) 1) << i);
    xkrt_metric_time(buffer[1], sizeof(buffer[1]), min);
    xkrt_metric_time(buffer[2], sizeof(buffer[2]), med);
    xkrt_metric_time(buffer[3], sizeof(buffer[3]), max);
    LOGGER_INFO("%12s | %12s | %12s | %12s", buffer[0], buffer[1], buffer[2], buffer[3]);
}

///////////////////////////
// kernel launch latency //
///////////////////////////

# if XKRT_SUPPORT_ZE
#  include <xkrt/logger/logger-ze.h>
#  include <xkrt/driver/driver-ze.h>

ze_kernel_handle_t XKBM_ZE_KERNEL_EMPTY;
extern const uint8_t * SPIRV_EMPTY_KERNEL;
# endif

static void
launch(
    void * istream,
    void * vinstr,
    xkrt_stream_instruction_counter_t idx
) {
    assert(istream);

    xkrt_stream_instruction_t * instr = (xkrt_stream_instruction_t *) vinstr;
    assert(instr);

    xkrt_device_t * device = (xkrt_device_t *) instr->kern.vargs;
    assert(device);

    switch (device->driver_type)
    {
        # if XKRT_SUPPORT_CUDA
        case (XKRT_DRIVER_TYPE_CUDA):
        {
            break ;
        }
        # endif

        # if XKRT_SUPPORT_ZE
        case (XKRT_DRIVER_TYPE_ZE):
        {
            xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;
            ze_event_handle_t * ze_event = stream->ze.events.list + idx;
            ze_group_count_t launch_args = { 1, 1, 1 };
            ZE_SAFE_CALL(
                zeCommandListAppendLaunchKernel(
                    stream->ze.command.list,
                    XKBM_ZE_KERNEL_EMPTY,
                   &launch_args,
                    nullptr,
                    0,
                    ze_event
                )
            );
            break ;
        }
        # endif

        default:
            LOGGER_FATAL("Something went wrong");
    }
}

static void
completed(const void * vargs[XKRT_CALLBACK_ARGS_MAX])
{
}

template<bool task>
static void
xkrt_benchmarks_kernel_launch_latency(void)
{
    for (xkrt_device_global_id_t device_global_id ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
    {
        xkrt_device_t * device = runtime.device_get(device_global_id);
        assert(device);

        xkrt_callback_t callback = { .func = completed, .args = { device, NULL } };
        device->offloader_stream_instruction_submit_kernel(launch, device, callback);
    }
}

static benchmark_node_t xkrt_benchmark_kernel_launch_latency = {
    .name = "kernel-launch-latency",
    .desc = "Time to launch and complete a kernel",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_kernel_launch_latency<false>,
    .enabled = 1
};

////////////////
// alloc host //
////////////////

typedef enum    alloc_mode_t
{
    SYSTEM,
    DRIVER
}               alloc_mode_t;

template<alloc_mode_t mode, bool touch>
static void *
xkrt_benchmarks_alloc_run_thread(xkrt_device_global_id_t device_global_id, void * vargs)
{
    xkrt_team_t * team = (xkrt_team_t *) vargs;

    time_array_t<34, 5> time_alloc;
    time_array_t<34, 5> time_dealloc;

    runtime.team_critical_begin(team);
    for (int iter = 0 ; iter < time_alloc.niters ; ++iter)
    {
        for (int i = 0 ; i < time_alloc.nelements ; ++i)
        {
            const size_t size = ((size_t) 1) << i;
            void * ptr = NULL;

            // alloc
            {
                uint64_t t0 = xkrt_get_nanotime();
                {
                    if constexpr(mode == SYSTEM)
                        ptr = malloc(size);
                    else
                        ptr = runtime.memory_host_allocate(device_global_id, size);

                    if constexpr(touch)
                        xkbm_mem_touch(ptr, size);

                    if (ptr == NULL)
                        break ;
                }
                uint64_t tf = xkrt_get_nanotime();
                time_alloc.set(i, iter, tf - t0);
            }

            assert(ptr);

            // dealloc
            {
                uint64_t t0 = xkrt_get_nanotime();
                {
                    if constexpr(mode == SYSTEM)
                        free(ptr);
                    else
                        runtime.memory_host_deallocate(device_global_id, ptr, size);
                }
                uint64_t tf = xkrt_get_nanotime();
                time_dealloc.set(i, iter, tf - t0);
            }
        }
    }
    runtime.team_critical_end(team);

    const char * smode  = (mode == DRIVER) ? "driver" : "system";
    const char * stouch =            touch ? "touch" : "notouch";
    LOGGER_INFO("---- Device %u (alloc with %s and %s) ----", device_global_id, smode, stouch);
    time_alloc.report<pp_1byte_3time>();

    LOGGER_INFO("---- Device %u (dealloc with %s and %s) ----", device_global_id, smode, stouch);
    time_dealloc.report<pp_1byte_3time>();

    return NULL;
}

template <alloc_mode_t mode, bool touch>
static void
xkrt_benchmarks_alloc_run(void)
{
    xkrt_team_t team;
    team.desc.routine = xkrt_benchmarks_alloc_run_thread<mode, touch>;
    team.desc.args    = &team;
    team.desc.devices = XKRT_DEVICES_MASK_ALL;

    runtime.team_create(&team);
    runtime.team_join(&team);
}

static benchmark_node_t xkrt_benchmarks_alloc_system_touch = {
    .name = "alloc-host-system-touch",
    .desc = "Time (allocation+touch) and (deallocation) of host memory using the system host-allocator",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_alloc_run<SYSTEM, true>,
    .enabled = 1
};

static benchmark_node_t xkrt_benchmarks_alloc_system_notouch = {
    .name = "alloc-host-system-no-touch",
    .desc = "Time (allocation) and (deallocation) of host memory using the system host-allocator",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_alloc_run<SYSTEM, false>,
    .enabled = 1
};

static benchmark_node_t xkrt_benchmarks_alloc_driver_touch = {
    .name = "alloc-host-driver-touch",
    .desc = "Time (allocation+touch) and (deallocation) of host memory using the system host-allocator",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_alloc_run<DRIVER, true>,
    .enabled = 1
};

static benchmark_node_t xkrt_benchmarks_alloc_driver_notouch = {
    .name = "alloc-host-driver-no-touch",
    .desc = "Time (allocation) and (deallocation) of host memory using the system host-allocator",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_alloc_run<DRIVER, false>,
    .enabled = 1
};

//////////////////////////////////////////////

static void *
xkrt_benchmarks_alloc_parallel_run_thread(xkrt_device_global_id_t device_global_id, void * vargs)
{
    xkrt_team_t * team = (xkrt_team_t *) vargs;

    time_array_t<34, 5> time_alloc;
    uint64_t t0, tf;

    for (int iter = 0 ; iter < time_alloc.niters ; ++iter)
    {
        for (int i = 0 ; i < time_alloc.nelements ; ++i)
        {
            const size_t size = ((size_t) 1) << i;
            void * ptr = NULL;

            runtime.team_barrier(team);
            if (device_global_id == 0)
                t0 = xkrt_get_nanotime();

            ptr = runtime.memory_host_allocate(device_global_id, size);
            if (ptr == NULL)
                break ;

            runtime.team_barrier(team);
            if (device_global_id == 0)
                tf = xkrt_get_nanotime();

            time_alloc.set(i, iter, tf - t0);
            runtime.memory_host_deallocate(device_global_id, ptr, size);
        }
    }

    if (device_global_id == 0)
        time_alloc.report<pp_1byte_3time>();

    return NULL;
}

static void
xkrt_benchmarks_alloc_parallel_run(void)
{
    xkrt_team_t team;
    team.desc.routine = xkrt_benchmarks_alloc_parallel_run_thread,
    team.desc.args    = &team;
    team.desc.devices = XKRT_DEVICES_MASK_ALL;

    runtime.team_create(&team);
    runtime.team_join(&team);
}

static benchmark_node_t xkrt_benchmarks_alloc_driver_notouch_parallel = {
    .name = "alloc-host-driver-no-touch-parallel",
    .desc = "Time (allocation) of host memory using the driver host-allocator in parallel on every devices",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_alloc_parallel_run,
    .enabled = 1
};

////////////////
// H2D or D2H //
////////////////

typedef enum    direction_t
{
    H2D,
    D2H
}               direction_t;

typedef struct  bench_tranfer_args_t
{
    xkrt_team_t * team;
}               bench_tranfer_args_t;

template <direction_t direction, int nchunks>
static void *
xkrt_benchmarks_mem_run_thread(xkrt_device_global_id_t device_global_id, void * vargs)
{
    bench_tranfer_args_t * args = (bench_tranfer_args_t *) vargs;
    xkrt_team_t * team = args->team;

    xkrt_device_t * device = runtime.device_get(device_global_id);
    assert(device);

    time_array_t<XKRT_DEVICE_MEMORIES_MAX, 3> time;
    for (int i = 0 ; i < XKRT_DEVICE_MEMORIES_MAX ; ++i)
    {
        xkrt_device_memory_info_t * meminfo = device->memories + i;

        /////////////////////
        // allocate memory //
        /////////////////////

        // device alocation
        const float                      f = runtime.conf.device.gpu_mem_percent / 100.0 * 0.99;
        const size_t                  size = (meminfo->capacity * f);
        const size_t            chunk_size = size / nchunks;
        const xkrt_area_chunk_t    * chunk = device->memory_allocate_on(size, i);
        if (chunk == NULL)
            LOGGER_FATAL("Out of device memory");

        // host allocation
        void * host_mem = runtime.memory_host_allocate(device_global_id, size);
        xkbm_mem_touch(host_mem, size);

        // wait for all devices to allocate
        runtime.team_barrier(team);

        /////////////////////
        // do the transfer //
        /////////////////////

        const xkrt_device_global_id_t src_device_global_id  = (direction == H2D) ? HOST_DEVICE_GLOBAL_ID : device_global_id;
        const uintptr_t               src_device_mem        = (direction == H2D) ? (uintptr_t) host_mem  : chunk->device_ptr;
        const xkrt_device_global_id_t dst_device_global_id  = (direction == H2D) ? device_global_id      : HOST_DEVICE_GLOBAL_ID;
        const uintptr_t               dst_device_mem        = (direction == H2D) ? chunk->device_ptr     : (uintptr_t) host_mem;

        const xkrt_callback_t callback = { .func = NULL };

        // only 1 device at a time
        runtime.team_critical_begin(team);
        {
            for (int iter = 0 ; iter < time.niters ; ++iter)
            {
                uint64_t t0 = xkrt_get_nanotime();
                {
                    for (int i = 0 ; i < nchunks ; ++i)
                    {
                        runtime.copy(
                            device_global_id,
                            chunk_size,
                            dst_device_global_id,
                            dst_device_mem + i * chunk_size,
                            src_device_global_id,
                            src_device_mem + i * chunk_size,
                            callback
                        );
                    }
                    runtime.wait_device(device_global_id);
                }
                uint64_t tf = xkrt_get_nanotime();

                const size_t size = chunk_size * nchunks;
                const size_t bw = size / ((tf - t0) / 1e9);
                time.set(device_global_id, iter, bw);
            }
        }
        runtime.team_critical_end(team);

        /////////////
        // release //
        /////////////

        // device
        runtime.memory_deallocate_all(device_global_id);
        runtime.memory_host_deallocate(device_global_id, host_mem, size);

        //////////////////
        // print result //
        //////////////////
        runtime.team_critical_begin(team);
        {
            LOGGER_INFO("--- Device %u ---", device_global_id);
            LOGGER_INFO("%10s | %10s | %10s | %10s", "Memory ID", "min", "med", "max");
            time.report<pp_int_3bw>(device->nmemories);
        }
        runtime.team_critical_end(team);
    }

    return NULL;
}

template <direction_t direction>
static void
xkrt_benchmarks_mem_run(void)
{
    bench_tranfer_args_t args;
    xkrt_team_t team = {
        .desc = {
            .routine = xkrt_benchmarks_mem_run_thread<direction, 64>,
            .args    = &args,
            .devices = XKRT_DEVICES_MASK_ALL,
        }
    };

    args.team = &team;

    runtime.team_create(args.team);
    runtime.team_join(args.team);
}

static benchmark_node_t xkrt_benchmarks_h2d = {
    .name = "H2D",
    .desc = "Host memory to device (global) memory bandwidth",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_mem_run<H2D>,
    .enabled = 1
};

static benchmark_node_t xkrt_benchmarks_d2h = {
    .name = "D2H",
    .desc = "Device (global) to host memory memory bandwidth",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_mem_run<D2H>,
    .enabled = 1
};

/////////////////////
// XKRT BENCHMARKS //
/////////////////////

static benchmark_node_t xkrt_benchmarks = {
    .name = "xkrt",
    .desc = "Metrics on XKRT-supported devices",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = NULL,
    .enabled = 1
};

void
xkrt_benchmark_push(benchmark_node_t * parent)
{
    benchmark_push_children(parent, &xkrt_benchmarks);
    // benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_alloc_system_touch);
    // benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_alloc_system_notouch);
    // benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_alloc_driver_touch);
    // benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_alloc_driver_notouch);
    // benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_alloc_driver_notouch_parallel);
    benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmark_kernel_launch_latency);
    // benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_h2d);
    // benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_d2h);
}

void
xkrt_benchmark_init(void)
{
    xkrt_init(&runtime);

    # if XKRT_SUPPORT_ZE
    // TODO : get ze driver and compile kernel
    XKBM_ZE_KERNEL_EMPTY = NULL;
    # endif /* XKRT_SUPPORT_ZE */
}

void
xkrt_benchmark_deinit(void)
{
    xkrt_deinit(&runtime);
}
