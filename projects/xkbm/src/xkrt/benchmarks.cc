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

# include <xkrt/driver/driver-cuda.h>
# include <xkrt/logger/logger-cu.h>

# include <xkrt/driver/driver-ze.h>
# include <xkrt/logger/logger-ze.h>

template <direction_t direction>
static void
xkrt_benchmarks_mem_run(void)
{
    time_array_t<XKRT_DEVICES_MAX, 3> time;

    # define NCHUNKS 64
    xkrt_area_chunk_t * chunk[XKRT_DEVICES_MAX];
    size_t sizes[XKRT_DEVICES_MAX];
    size_t chunk_size[XKRT_DEVICES_MAX];
    uintptr_t host_mem[XKRT_DEVICES_MAX];

    cpu_set_t save_set;
    xkrt_runtime_t::thread_getaffinity(save_set);

    /////////////////////
    // allocate memory //
    /////////////////////
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
    {
        // bind current thread to the device closest NUMA domain
        const xkrt_device_t * device = runtime.device_get(device_global_id);
        xkrt_runtime_t::thread_setaffinity(device->thread->cpuset);

        // allocate memory on the device and the host
        xkrt_device_memory_info_t meminfo;
        runtime.memory_info(device_global_id, &meminfo);

        const float f = runtime.conf.device.gpu_mem_percent / 100.0 * 0.99;
        const size_t size = (meminfo.capacity * f);
        sizes[device_global_id] = size;
        chunk_size[device_global_id] = size / NCHUNKS;

        // device alocation
        chunk[device_global_id] = runtime.memory_allocate(device_global_id, size);
        if (chunk[device_global_id] == NULL)
            LOGGER_FATAL("Out of device memory");

        # define USE_ZE_ALLOC 1
        # define USE_CU_ALLOC 0

        // host allocation
        # if USE_ZE_ALLOC
        host_mem[device_global_id] = (uintptr_t) xkbm_mem_alloc(size);
        const xkrt_device_ze_t * dev = (xkrt_device_ze_t *) device;
        const ze_host_mem_alloc_desc_t host_desc = {
            .stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC,
            .pNext = NULL,
            .flags = 0
            // .flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_CACHED | ZE_HOST_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT | ZE_HOST_MEM_ALLOC_FLAG_BIAS_WRITE_COMBINED
        };
        void * ptr;
        ZE_SAFE_CALL(zeMemAllocHost(dev->ze_context, &host_desc, size, 64, (void **) &ptr));
        if (host_mem[device_global_id] == 0)
            LOGGER_FATAL("Out of host memory");
        host_mem[device_global_id] = (uintptr_t) ptr;
        # elif USE_CU_ALLOC
        void * ptr;
        CU_SAFE_CALL(cudaMallocHost(&ptr, size));
        host_mem[device_global_id] = (uintptr_t) ptr;
        # else
        host_mem[device_global_id] = (uintptr_t) xkbm_mem_alloc(size);
        # endif

        xkbm_mem_touch((void *) host_mem[device_global_id], size);
    }

    /////////////////////
    // do the transfer //
    /////////////////////
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
    {
        const xkrt_device_t * device = runtime.device_get(device_global_id);
        xkrt_runtime_t::thread_setaffinity(device->thread->cpuset);

        const xkrt_device_global_id_t src_device_global_id  = (direction == H2D) ? HOST_DEVICE_GLOBAL_ID               : device_global_id;
        const uintptr_t               src_device_mem        = (direction == H2D) ? host_mem[device_global_id]          : chunk[device_global_id]->device_ptr;
        const xkrt_device_global_id_t dst_device_global_id  = (direction == H2D) ? device_global_id                    : HOST_DEVICE_GLOBAL_ID;
        const uintptr_t               dst_device_mem        = (direction == H2D) ? chunk[device_global_id]->device_ptr : host_mem[device_global_id];

        const xkrt_callback_t callback = { .func = NULL };

        for (int iter = 0 ; iter < time.niters ; ++iter)
        {
            uint64_t t0 = xkrt_get_nanotime();
            {
                for (int i = 0 ; i < NCHUNKS ; ++i)
                {
                    runtime.copy(
                        device_global_id,
                        chunk_size[device_global_id],
                        dst_device_global_id,
                        dst_device_mem + i * chunk_size[device_global_id],
                        src_device_global_id,
                        src_device_mem + i * chunk_size[device_global_id],
                        callback
                    );
                }
                runtime.wait_device(device_global_id);
            }
            uint64_t tf = xkrt_get_nanotime();

            const size_t size = chunk_size[device_global_id] * NCHUNKS;
            const size_t bw = size / ((tf - t0) / 1e9);
            time.set(device_global_id, iter, bw);
            LOGGER_INFO("Took %lf s. for %lu bytes", (tf - t0) / 1e9, size);
        }
    }

    /////////////
    // release //
    /////////////
    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
    {
        runtime.memory_deallocate_all(device_global_id);
        # if USE_ZE_ALLOC
        const xkrt_device_ze_t * dev = (const xkrt_device_ze_t *) runtime.device_get(device_global_id);
        ZE_SAFE_CALL(zeMemFree(dev->ze_context, (void *) host_mem[device_global_id]));
        # elif USE_CU_ALLOC
        cudaFreeHost((void *) host_mem[device_global_id]);
        # else
        xkbm_free((void *) host_mem[device_global_id], sizes[device_global_id]);
        # endif
    }

    xkrt_runtime_t::thread_setaffinity(save_set);

    // report
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
