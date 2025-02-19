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

////////////////
// H2D or D2H //
////////////////

# include <sys/mman.h>

static void
pp_int_3bw(size_t i, size_t min, size_t med, size_t max)
{
    char buffer[3][64];
    xkrt_metric_bandwidth(buffer[0], sizeof(buffer[0]), min);
    xkrt_metric_bandwidth(buffer[1], sizeof(buffer[1]), med);
    xkrt_metric_bandwidth(buffer[2], sizeof(buffer[2]), max);
    LOGGER_INFO("%10lu | %10s | %10s | %10s", i, buffer[0], buffer[1], buffer[2]);
}

typedef enum    direction_t
{
    H2D,
    D2H
}               direction_t;

typedef struct  bench_args_t
{
    xkrt_team_t * team;
    time_array_t<XKRT_DEVICES_MAX, 3> * time;
}               bench_args_t;

template <direction_t direction, int nchunks>
static void *
xkrt_benchmarks_mem_run_thread(xkrt_device_global_id_t device_global_id, void * vargs)
{
    bench_args_t * args = (bench_args_t *) vargs;
    xkrt_team_t * team = args->team;
    time_array_t<XKRT_DEVICES_MAX, 3> * time = args->time;

    /////////////////////
    // allocate memory //
    /////////////////////

    // device alocation
    xkrt_device_memory_info_t meminfo;
    runtime.memory_info(device_global_id, &meminfo);

    const float                      f = runtime.conf.device.gpu_mem_percent / 100.0 * 0.99;
    const size_t                  size = (meminfo.capacity * f);
    const size_t            chunk_size = size / nchunks;
    const xkrt_area_chunk_t    * chunk = runtime.memory_allocate(device_global_id, size);
    if (chunk == NULL)
        LOGGER_FATAL("Out of device memory");

    // host allocation
    void * host_mem = runtime.memory_allocate_host(device_global_id, size);
    xkbm_mem_touch(host_mem, size);

    // wait for all devices to allocate
    runtime.team_barrier(team);

    /////////////////////
    // do the transfer //
    /////////////////////

    // only 1 device at a time
    runtime.team_critical_begin(team);
    {
        const xkrt_device_global_id_t src_device_global_id  = (direction == H2D) ? HOST_DEVICE_GLOBAL_ID : device_global_id;
        const uintptr_t               src_device_mem        = (direction == H2D) ? (uintptr_t) host_mem  : chunk->device_ptr;
        const xkrt_device_global_id_t dst_device_global_id  = (direction == H2D) ? device_global_id      : HOST_DEVICE_GLOBAL_ID;
        const uintptr_t               dst_device_mem        = (direction == H2D) ? chunk->device_ptr     : (uintptr_t) host_mem;

        const xkrt_callback_t callback = { .func = NULL };

        for (int iter = 0 ; iter < time->niters ; ++iter)
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
            time->set(device_global_id, iter, bw);
        }
    }
    runtime.team_critical_end(team);

    /////////////
    // release //
    /////////////

    // device
    runtime.memory_deallocate_all(device_global_id);
    runtime.memory_deallocate_host(device_global_id, host_mem, size);

    return NULL;
}

template <direction_t direction>
static void
xkrt_benchmarks_mem_run(void)
{
    bench_args_t args;
    time_array_t<XKRT_DEVICES_MAX, 3> time;
    xkrt_team_t team = {
        .desc = {
            .routine = xkrt_benchmarks_mem_run_thread<direction, 64>,
            .args    = &args,
            .devices = XKRT_DEVICES_MASK_ALL,
        }
    };
    args.team = &team;
    args.time = &time;

    runtime.team_create(args.team);
    runtime.team_join(args.team);

    LOGGER_INFO("%10s | %10s | %10s | %10s", "Device", "min", "med", "max");
    time.report<pp_int_3bw>(runtime.drivers.devices.n);
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

///////////////////
// ZE BENCHMARKS //
///////////////////

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
    benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_h2d);
    benchmark_push_children(&xkrt_benchmarks, &xkrt_benchmarks_d2h);
}

void
xkrt_benchmark_init(void)
{
    xkrt_init(&runtime);
}

void
xkrt_benchmark_deinit(void)
{
    xkrt_deinit(&runtime);
}
