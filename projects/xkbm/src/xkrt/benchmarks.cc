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

typedef enum    direction_t
{
    H2D,
    D2H
}               direction_t;

template <direction_t direction>
static void
xkrt_benchmarks_mem_run(void)
{
    time_array_t<XKRT_DEVICES_MAX, 5> time;

    # define NCHUNKS 64
    xkrt_area_chunk_t * chunks[NCHUNKS][XKRT_DEVICES_MAX];
    size_t chunk_size[XKRT_DEVICES_MAX];
    uintptr_t host_mem[XKRT_DEVICES_MAX];

    cpu_set_t save_set;
    xkrt_runtime_t::thread_getaffinity(save_set);

    // allocate memory
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
    {
        // bind current thread to the device closest NUMA domain
        const xkrt_device_t * device = runtime.device_get(device_global_id);
        xkrt_runtime_t::thread_setaffinity(device->thread->cpuset);

        // allocate memory on the device and the host
        xkrt_device_memory_info_t meminfo;
        runtime.memory_info(device_global_id, &meminfo);

        const float f = runtime.conf.device.gpu_mem_percent / 100.0 * 0.99;
        chunk_size[device_global_id] = (meminfo.capacity * f) / NCHUNKS;
        assert(chunk_size[device_global_id] > 0);

        for (int i = 0 ; i < NCHUNKS ; ++i)
        {
            chunks[device_global_id][i] = runtime.memory_allocate(device_global_id, chunk_size[device_global_id]);
            if (chunks[device_global_id][i] == NULL)
                LOGGER_FATAL("Out of device memory");
        }

        host_mem[device_global_id] = (uintptr_t) xkbm_alloc_and_touch(NCHUNKS * chunk_size[device_global_id]);
        if (host_mem[device_global_id] == 0)
            LOGGER_FATAL("Out of host memory");
    }
    xkrt_runtime_t::thread_setaffinity(save_set);

    // do the transfers
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
    {
        const xkrt_device_t * device = runtime.device_get(device_global_id);
        xkrt_runtime_t::thread_setaffinity(device->thread->cpuset);

        const xkrt_device_global_id_t dst_device_global_id  = (direction == H2D) ? device_global_id : HOST_DEVICE_GLOBAL_ID;
        const xkrt_device_global_id_t src_device_global_id  = (direction == D2H) ? device_global_id : HOST_DEVICE_GLOBAL_ID;
        const xkrt_callback_t         callback              = { .func = NULL };

        for (int iter = 0 ; iter < time.niters ; ++iter)
        {
            uint64_t t0 = xkrt_get_nanotime();
            {
                for (int i = 0 ; i < NCHUNKS ; ++i)
                {
                    const uintptr_t src_device_addr = (direction == H2D) ? host_mem[device_global_id] + i * chunk_size[device_global_id] : chunks[device_global_id][i]->device_ptr;
                    const uintptr_t dst_device_addr = (direction == D2H) ? host_mem[device_global_id] + i * chunk_size[device_global_id] : chunks[device_global_id][i]->device_ptr;
                    runtime.copy(
                        device_global_id,
                        chunk_size[device_global_id],
                        dst_device_global_id,
                        dst_device_addr,
                        src_device_global_id,
                        src_device_addr,
                        callback
                    );
                }
                runtime.wait_device(device_global_id);
            }
            uint64_t tf = xkrt_get_nanotime();
            const size_t bw = NCHUNKS * chunk_size[device_global_id] / ((tf - t0) / 1e9);
            time.set(device_global_id, iter, bw);
        }
    }

    // release TODO
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
    {
        // runtime.memory_reset(device_global_id);
        xkbm_free((void *) host_mem[device_global_id]);
    }

    xkrt_runtime_t::thread_setaffinity(save_set);

    // report
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
