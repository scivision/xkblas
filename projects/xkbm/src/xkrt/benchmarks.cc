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
    LOGGER_INFO("%12lu | %8s | %8s | %8s", i, buffer[0], buffer[1], buffer[2]);
}

static void
xkrt_benchmarks_mem_run(void)
{
    const xkrt_device_global_id_t device_global_id = 0;

    // TODO : retrieve memory capacity from xkrt
    size_t size = (size_t) 50 * 1024*1024*1024;  // 50 Go

    xkrt_area_chunk_t * device_chunk = NULL;
    while (1)
    {
        device_chunk = runtime.memory_allocate(device_global_id, size);
        if (device_chunk)
            break ;
        size = (size_t) (size * 0.8);
        if (size < 1024)
            break ;
    }

    if (device_chunk == NULL)
        LOGGER_FATAL("Couldnt allocate %lu bytes to the device", size);

    uintptr_t device_mem = device_chunk->device_ptr;
    uintptr_t host_mem   = (uintptr_t) xkbm_alloc_and_touch(size);
    mlock((void *)host_mem, size);

    const xkrt_device_global_id_t dst_device_global_id  = device_global_id;
    const uintptr_t               dst_device_addr       = device_mem;
    const xkrt_device_global_id_t src_device_global_id  = HOST_DEVICE_GLOBAL_ID;
    const uintptr_t               src_device_addr       = host_mem;
    const xkrt_callback_t         callback              = { .func = NULL };

    time_array_t<1, 3> time;
    for (int iter = 0 ; iter < time.niters ; ++iter)
    {
        uint64_t t0 = xkrt_get_nanotime();
        {
            runtime.copy(
                device_global_id,
                size,
                dst_device_global_id,
                dst_device_addr,
                src_device_global_id,
                src_device_addr,
                callback
            );
            runtime.wait_device(device_global_id);
        }
        uint64_t tf = xkrt_get_nanotime();
        const size_t bw = size / ((tf - t0) / 1e9);
        time.set(0, iter, bw);
    }
    runtime.memory_deallocate(device_global_id, device_chunk);
    time.report<pp_int_3bw>();
}

static benchmark_node_t xkrt_benchmarks_h2d = {
    .name = "H2D",
    .desc = "Host memory to device (global) memory bandwidth",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = xkrt_benchmarks_mem_run,
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
