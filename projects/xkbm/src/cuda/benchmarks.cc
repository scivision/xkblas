# include <cuda_runtime.h>

# include <hwloc.h>
# include <hwloc/cuda.h>
# include <hwloc/cudart.h>
# include <hwloc/glibc-sched.h>

# include <xkbm/allocator.h>
# include <xkbm/benchmark.h>
# include <xkbm/thread.h>
# include <xkbm/topology.h>
# include <xkbm/team.h>
# include <xkbm/time.h>
# include <xkbm/combinator.h>
# include <xkbm/pp.h>

# include <xkrt/logger/logger.h>
# include <xkrt/logger/logger-cu.h>
# include <xkrt/logger/metric.h>

# include <stddef.h>

/**
 *  From
 *      https://docs.nvidia.com/cuda/cuda-runtime-api/
 *      https://docs.nvidia.com/cuda/pdf/CUDA_C_Programming_Guide.pdf
 *
 *  DEVICE PHYSICAL MEMORIES
 *      - Global Memory - all threads of all blocks
 *      - Shared Memory - all threads within a block
 *      - Local Memory  - private to a thread (register)
 *      - Constant Memory - read-only, for fast broadcasting to all threads
 *      - Texture Memory
 */

/////////////////////
// Memory register //
/////////////////////

static void
cuda_benchmarks_register_memory(void)
{
    // warmup
    {
        const size_t size = 1*1024*1024;
        void * hostmem = xkbm_alloc_and_touch(size);
        CUDA_SAFE_CALL(cudaHostRegister(hostmem, size, cudaHostRegisterPortable));
        CUDA_SAFE_CALL(cudaHostUnregister(hostmem));
        free(hostmem);
    }

    // bench
    combinator_t<unsigned int> combinator(
        {cudaHostRegisterPortable, cudaHostRegisterMapped, cudaHostRegisterIoMemory, cudaHostRegisterReadOnly},
        {              "Portable",               "Mapped",               "IoMemory",               "ReadOnly"}
    );

    time_array_t<32, 5>   register_time;
    time_array_t<32, 5> unregister_time;

    for (auto flags : combinator)
    {
        for (size_t i = 0 ; i < register_time.nelements ; ++i)
        {
            for (int iter = 0 ; iter < register_time.niters ; ++iter)
            {
                const size_t size = ((size_t) 1) << i;
                void * hostmem = xkbm_alloc_and_touch(size);

                {
                    const uint64_t t0 = xkrt_get_nanotime();
                    if (cudaHostRegister(hostmem, size, flags) == cudaErrorInvalidValue)
                        continue ;
                    const uint64_t tf = xkrt_get_nanotime();
                    register_time.set(i, iter, tf - t0);
                }

                {
                    const uint64_t t0 = xkrt_get_nanotime();
                    CUDA_SAFE_CALL(cudaHostUnregister(hostmem));
                    const uint64_t tf = xkrt_get_nanotime();
                    unregister_time.set(i, iter, tf - t0);
                }

                xkbm_free(hostmem, size);
            }
        }

        char buffer[128];
        combinator.names_from_flags(buffer, sizeof(buffer), flags);

        LOGGER_INFO("----------------------------------------------------------");
        LOGGER_INFO("Register with %s", buffer);
        LOGGER_INFO("----------------------------------------------------------");
        LOGGER_INFO("%12s | %27s", "size", "avg +/- stdev");
        register_time.report<pp_1byte_1time>();

        LOGGER_INFO("----------------------------------------------------------");
        LOGGER_INFO("Unregister with %s", buffer);
        LOGGER_INFO("----------------------------------------------------------");
        LOGGER_INFO("%12s | %25s", "size", "avg +/- stdev");
        unregister_time.report<pp_1byte_1time>();
    }
}

static benchmark_node_t cuda_benchmark_register_memory = {
    .name = "register-memory",
    .desc = "Host memory registration cost",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cuda_benchmarks_register_memory,
    .enabled = 1
};

////////////////
// H2D or D2H //
////////////////

typedef struct  h2d_run_args_t
{
    int device_id;
    size_t chunk_size;
}               h2d_run_args_t;

template <cudaMemcpyKind kind>
static void
cuda_benchmarks_memcpy_run_thread(
    xkbm_team_t * team,
    int tid,
    void * vargs
) {
    h2d_run_args_t * args = (h2d_run_args_t *) vargs;
    const size_t total_size = args->chunk_size * team->nthreads;

    void * hostptr;
    CUDA_SAFE_CALL(cudaMallocHost(&hostptr, args->chunk_size));

    void * devptr;
    CUDA_SAFE_CALL(cudaMalloc(&devptr, args->chunk_size));

    const void * src = (kind == cudaMemcpyDeviceToHost) ? devptr  : hostptr;
          void * dst = (kind == cudaMemcpyDeviceToHost) ? hostptr :  devptr;

    cudaStream_t stream;
    const int flags = cudaStreamNonBlocking;
    CUDA_SAFE_CALL(cudaStreamCreateWithFlags(&stream, flags));

    time_array_t<1, 3> time;
    for (int iter = 0 ; iter < time.niters ; ++iter)
    {
        uint64_t t0;
        xkbm_team_barrier(team);
        {
            if (tid == 0)
                t0 = xkrt_get_nanotime();

            CUDA_SAFE_CALL(cudaMemcpyAsync(devptr, hostptr, args->chunk_size, kind, stream));
            CUDA_SAFE_CALL(cudaStreamSynchronize(stream));
        }
        xkbm_team_barrier(team);

        if (tid == 0)
        {
            uint64_t tf = xkrt_get_nanotime();
            const size_t bw = total_size / ((tf - t0) / 1e9);
            time.set(0, iter, bw);
        }
    }

    CUDA_SAFE_CALL(cudaStreamDestroy(stream));
    if (tid == 0)
        time.report<pp_1zu_1bw>();
}

template <cudaMemcpyKind kind>
static void
cuda_benchmarks_mem_run(void)
{
    int ndevices;
    CUDA_SAFE_CALL(cudaGetDeviceCount(&ndevices));

    for (int devid = 0 ; devid < ndevices ; ++devid)
    {
        CUDA_SAFE_CALL(cudaSetDevice(devid));

        hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
        HWLOC_SAFE_CALL(hwloc_cudart_get_device_cpuset(TOPOLOGY, devid, cpuset));
        int nthreads = hwloc_bitmap_weight(cpuset);

        size_t free, total;
        CUDA_SAFE_CALL(cudaMemGetInfo(&free, &total));

        h2d_run_args_t args = {
            .device_id  = devid,
            .chunk_size = (free / 5 * 4) / nthreads
        };
        xkbm_team_work(cpuset, cuda_benchmarks_memcpy_run_thread<kind>, &args);
    }
}

static benchmark_node_t cuda_benchmarks_h2d = {
    .name = "H2D",
    .desc = "Host memory to device (global) memory bandwidth",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cuda_benchmarks_mem_run<cudaMemcpyHostToDevice>,
    .enabled = 1
};

static benchmark_node_t cuda_benchmarks_d2h = {
    .name = "D2H",
    .desc = "Device (global) memory to host memory bandwidth",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = cuda_benchmarks_mem_run<cudaMemcpyDeviceToHost>,
    .enabled = 1
};

/////////////////////
// CUDA BENCHMARKS //
/////////////////////

static benchmark_node_t cuda_benchmarks = {
    .name = "cuda",
    .desc = "Metrics on Cuda-supported devices",
    .parent = NULL,
    .children = { NULL },
    .nchildren = 0,
    .run = NULL,
    .enabled = 1
};

void
cuda_benchmark_push(benchmark_node_t * parent)
{
    int ndevices = 0;
    if (cudaGetDeviceCount(&ndevices) == cudaErrorInsufficientDriver)
    {
        LOGGER_WARN("Built with cuda support but couldnt detect any devices");
        return ;
    }

    benchmark_push_children(parent, &cuda_benchmarks);
    // benchmark_push_children(&cuda_benchmarks, &cuda_benchmark_register_memory);
    // benchmark_push_children(&cuda_benchmarks, &cuda_benchmarks_h2d);
    // benchmark_push_children(&cuda_benchmarks, &cuda_benchmarks_d2h);
}
