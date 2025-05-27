# include <cuda.h>

# include <hwloc.h>
# include <hwloc/cuda.h>
# include <hwloc/glibc-sched.h>

# include <xkbm/allocator.h>
# include <xkbm/benchmark.h>
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

static CUdevice device;
static CUcontext context;

/////////////////////
// Get attributes  //
/////////////////////

static void
bench_pointer_get_attribute(benchmark_node_t * node)
{
    static CUpointer_attribute attributes[] = {
        CU_POINTER_ATTRIBUTE_CONTEXT,
        CU_POINTER_ATTRIBUTE_MEMORY_TYPE,
        CU_POINTER_ATTRIBUTE_DEVICE_POINTER,
        CU_POINTER_ATTRIBUTE_HOST_POINTER,
        CU_POINTER_ATTRIBUTE_P2P_TOKENS,
        CU_POINTER_ATTRIBUTE_SYNC_MEMOPS,
        CU_POINTER_ATTRIBUTE_BUFFER_ID,
        CU_POINTER_ATTRIBUTE_IS_MANAGED,
        CU_POINTER_ATTRIBUTE_DEVICE_ORDINAL,
        CU_POINTER_ATTRIBUTE_IS_LEGACY_CUDA_IPC_CAPABLE,
        CU_POINTER_ATTRIBUTE_RANGE_START_ADDR,
        CU_POINTER_ATTRIBUTE_RANGE_SIZE,
        CU_POINTER_ATTRIBUTE_MAPPED,
        CU_POINTER_ATTRIBUTE_ALLOWED_HANDLE_TYPES,
        CU_POINTER_ATTRIBUTE_IS_GPU_DIRECT_RDMA_CAPABLE,
        CU_POINTER_ATTRIBUTE_ACCESS_FLAGS,
        CU_POINTER_ATTRIBUTE_MEMPOOL_HANDLE,
        CU_POINTER_ATTRIBUTE_MAPPING_SIZE,
        CU_POINTER_ATTRIBUTE_MAPPING_BASE_ADDR,
        CU_POINTER_ATTRIBUTE_MEMORY_BLOCK_ID,
        // CU_POINTER_ATTRIBUTE_IS_HW_DECOMPRESS_CAPABLE
    };

    constexpr size_t N = sizeof(attributes) / sizeof(CUpointer_attribute);
    time_array_t time(N, 1023);

    // allocate some memory to populate cuda allocator
    CUdeviceptr ptrs[24];
    constexpr int nptrs = sizeof(ptrs) / sizeof(void *);
    static_assert(nptrs % 3 == 0);
    for (int i = 0 ; i < nptrs ; i += 3)
    {
        const size_t size = 1024 * (i + 1);
        CU_SAFE_CALL(cuMemHostAlloc((void **) (ptrs + i + 0), size, CU_MEMHOSTREGISTER_PORTABLE));
        CU_SAFE_CALL(cuMemAlloc(               ptrs + i + 1,  size                             ));
        CU_SAFE_CALL(cuMemAllocManaged(        ptrs + i + 2,  size, CU_MEM_ATTACH_GLOBAL       ));
    }

    for (size_t i = 0 ; i < time.nelements ; ++i)
    {
        CUpointer_attribute attr = attributes[i];
        for (int iter = 0 ; iter < time.niters ; ++iter)
        {
            // get a pseudo random ptr
            CUdeviceptr ptr = ptrs[iter % nptrs];

            // get attr
            int attr_value[16];

            const uint64_t t0 = xkrt_get_nanotime();
            if (cuPointerGetAttribute(attr_value, attr, ptr))
                continue ;
            const uint64_t tf = xkrt_get_nanotime();

            time.set(i, iter, tf - t0);
        }
    }

    auto convert = [] (char * buffer, size_t buffer_size, int i) {
        const char * attributes_name[] = {
            "CONTEXT",
            "MEMORY_TYPE",
            "DEVICE_POINTER",
            "HOST_POINTER",
            "P2P_TOKENS",
            "SYNC_MEMOPS",
            "BUFFER_ID",
            "IS_MANAGED",
            "DEVICE_ORDINAL",
            "IS_LEGACY_CUDA_IPC_CAPABLE",
            "RANGE_START_ADDR",
            "RANGE_SIZE",
            "MAPPED",
            "ALLOWED_HANDLE_TYPES",
            "IS_GPU_DIRECT_RDMA_CAPABLE",
            "ACCESS_FLAGS",
            "MEMPOOL_HANDLE",
            "MAPPING_SIZE",
            "MAPPING_BASE_ADDR",
            "MEMORY_BLOCK_ID",
            // "IS_HW_DECOMPRESS_CAPABLE"
        };
        snprintf(buffer, buffer_size, "%s", attributes_name[i]);
    };
    time.report<METRIC_TIME>("Attribute", convert);

    for (int i = 0 ; i < nptrs ; i += 3)
    {
        CU_SAFE_CALL(cuMemFreeHost((void *) ptrs[i + 0]));
        CU_SAFE_CALL(cuMemFree(ptrs[i + 1]));
        CU_SAFE_CALL(cuMemFree(ptrs[i + 2]));
    }
}

static benchmark_node_t pointer_get_attribute = {
    .name = "pointer_get_attribute",
    .desc = "cuPointerGetAttribute overheads",
    .run = bench_pointer_get_attribute,
};

/////////////////////
// Memory register //
/////////////////////

static void
bench_register_memory(benchmark_node_t * bench)
{
    // warmup
    {
        const size_t size = 1*1024*1024;
        void * hostmem = xkbm_alloc_and_touch(size);
        CU_SAFE_CALL(cuMemHostRegister(hostmem, size, CU_MEMHOSTREGISTER_PORTABLE));
        CU_SAFE_CALL(cuMemHostUnregister(hostmem));
        free(hostmem);
    }

    // bench
    combinator_t<unsigned int> combinator(
        {CU_MEMHOSTREGISTER_PORTABLE, CU_MEMHOSTREGISTER_DEVICEMAP, CU_MEMHOSTREGISTER_IOMEMORY, CU_MEMHOSTREGISTER_READ_ONLY},
        {              "Portable",               "Mapped",               "IoMemory",               "ReadOnly"}
    );

    time_array_t   register_time(32, 5);
    time_array_t unregister_time(32, 5);

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
                    if (cuMemHostRegister(hostmem, size, flags))
                        continue ;
                    const uint64_t tf = xkrt_get_nanotime();
                    register_time.set(i, iter, tf - t0);
                }

                {
                    const uint64_t t0 = xkrt_get_nanotime();
                    CU_SAFE_CALL(cuMemHostUnregister(hostmem));
                    const uint64_t tf = xkrt_get_nanotime();
                    unregister_time.set(i, iter, tf - t0);
                }

                xkbm_free(hostmem, size);
            }
        }

        char name[64];
        char buffer[64];
        combinator.names_from_flags(buffer, sizeof(buffer), flags);

        snprintf(name, sizeof(name), "%s(%s)", "Register", buffer);
        register_time.report<decltype(  register_time)::pp_1byte_1time>(name);

        snprintf(name, sizeof(name), "%s(%s)", "Unregister", buffer);
        unregister_time.report<decltype(unregister_time)::pp_1byte_1time>(name);
    }
}

static benchmark_node_t register_memory = {
    .name = "register-memory",
    .desc = "Host memory registration cost",
    .run = bench_register_memory
};

/////////////////////
// CUDA BENCHMARKS //
/////////////////////

static benchmark_node_t cuda = {
    .name = "cuda",
    .desc = "Metrics on Cuda-supported devices",
    .enabled = 1
};

void
cuda_benchmark_push(benchmark_node_t * parent)
{
    int ndevices = 0;
    if (cuDeviceGetCount(&ndevices))
    {
        LOGGER_WARN("Built with cuda support but couldnt detect any devices");
        return ;
    }

    benchmark_push_children(parent, &cuda);
    # define LINK(X, Y) benchmark_push_children(&X, &Y)

    LINK(cuda, register_memory);
    LINK(cuda, pointer_get_attribute);
}

void
cuda_benchmark_deinit(void)
{
    CU_SAFE_CALL(cuCtxDestroy(context));
}

void
cuda_benchmark_init(void)
{
    CU_SAFE_CALL(cuDeviceGet(&device, 0));
    CU_SAFE_CALL(cuCtxCreate(&context, 0, device));
}
