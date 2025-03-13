# include <stddef.h>

# include <xkbm/allocator.h>
# include <xkbm/benchmark.h>
# include <xkbm/topology.h>
# include <xkbm/time.h>
# include <xkbm/pp.h>

# include <xkrt/xkrt.h>
# include <xkrt/runtime.h>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/metric.h>
# include <xkrt/driver/thread.hpp>

static xkrt_runtime_t runtime;

///////////////////////////////////////
// HELPER TO RUN 1 THREAD PER DEVICE //
///////////////////////////////////////

typedef struct  tls_t
{
    // used in D2D only
    xkrt_area_chunk_t * areas[XKRT_DEVICES_MAX][XKRT_DEVICES_MAX][XKRT_DEVICE_MEMORIES_MAX][2];

}               tls_t;

template <void * (*func)(xkrt_team_t * team, xkrt_thread_t * thread)>
static void
foreach_device(benchmark_node_t * bench)
{
    // TODO: Update this benchmark if GPU has several memories

    tls_t tls;
    xkrt_team_t team = {
        .desc = {
            .routine = func,
            .args = &tls,
            .nthreads = runtime.drivers.devices.n,
            .binding = {
                .mode = XKRT_TEAM_BINDING_MODE_COMPACT,
                .places = XKRT_TEAM_BINDING_PLACES_DEVICE,
            }
        }
    };

    runtime.team_create(&team);
    runtime.team_join(&team);
}

///////////////////////////
// kernel launch latency //
///////////////////////////

# if XKRT_SUPPORT_CUDA
#  include <cuda.h>
#  include <xkrt/logger/logger-cu.h>
#  include <xkrt/driver/driver-cu.h>

# if 1
const char PTX_EMPTY_KERNEL[] = "       \
.version 6.4                            \
.target sm_52                           \
.address_size 64                        \
                                        \
.visible .entry empty_kernel()          \
{                                       \
    ret;                                \
}                                       \
";
# else
const uint8_t PTX_EMPTY_KERNEL[] = {
#  include <xkbm/kernels/empty.ptxbin>
};
# endif

static xkrt_driver_module_fn_t XKBM_CU_KERNEL_EMPTY[XKRT_DEVICES_MAX];

# endif

# if XKRT_SUPPORT_ZE
#  include <xkrt/logger/logger-ze.h>
#  include <xkrt/driver/driver-ze.h>

// an empty spirv kernel
const uint8_t SPIRV_EMPTY_KERNEL[] = {
#  include <xkbm/kernels/empty.spvbin>
};

static xkrt_driver_module_fn_t XKBM_ZE_KERNEL_EMPTY[XKRT_DEVICES_MAX];

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
            xkrt_stream_cu_t * stream = (xkrt_stream_cu_t *) istream;

               CUevent e = stream->cu.events.buffer[idx];
              CUstream s = stream->cu.handle.high;
            CUfunction f = (CUfunction) XKBM_CU_KERNEL_EMPTY[device->driver_id];

            CU_SAFE_CALL(
                cuLaunchKernel(
                    f,
                    1, 1, 1,    // grid size
                    1, 1, 1,    // block size
                    0,          // shared memory size
                    s,          // stream
                    NULL,       // kernel params,
                    NULL        // extra options
                )
            );

            CU_SAFE_CALL(cuEventRecord(e, s));

            break ;
        }
        # endif

        # if XKRT_SUPPORT_ZE
        case (XKRT_DRIVER_TYPE_ZE):
        {
            xkrt_stream_ze_t * stream = (xkrt_stream_ze_t *) istream;
            ze_kernel_handle_t handle = (ze_kernel_handle_t) XKBM_ZE_KERNEL_EMPTY[device->driver_id];
            assert(handle);

            const uint32_t size = 1;
            uint32_t group_size[3] = { 0 };
            ZE_SAFE_CALL(
                zeKernelSuggestGroupSize(
                    handle,
                    size, size, 1,
                    &(group_size[0]), &(group_size[1]), &(group_size[2])
                )
            );
            ZE_SAFE_CALL(zeKernelSetGroupSize(handle, group_size[0], group_size[1], group_size[2]));
            ze_group_count_t launch_args = { size / group_size[0], size / group_size[1], 1 };
            ze_event_handle_t ze_event = stream->ze.events.list[idx];
            ZE_SAFE_CALL(
                zeCommandListAppendLaunchKernel(
                    stream->ze.command.list,
                    handle,
                   &launch_args,
                    ze_event,
                    0,
                    NULL
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
kernel_launch_latency_launch(benchmark_node_t * bench)
{
    time_array_t time(runtime.drivers.devices.n, 1001);

    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
    {
        xkrt_device_t * device = runtime.device_get(device_global_id);
        assert(device);

        xkrt_callback_t callback = { .func = completed, .args = { device, NULL } };

        for (int iter = -100 ; iter < (int) time.niters ; ++iter)
        {
            uint64_t t0 = xkrt_get_nanotime();
            device->offloader_stream_instruction_submit_kernel(launch, device, callback);
            device->offloader_stream_instructions_launch(XKRT_STREAM_TYPE_KERN);
            device->offloader_stream_instructions_progress<true>(XKRT_STREAM_TYPE_KERN);
            uint64_t tf = xkrt_get_nanotime();
            if (iter >= 0)
                time.set(device_global_id, iter, tf - t0);
        }
    }

    auto convert = [] (char * buffer, size_t buffer_size, int i) { snprintf(buffer, buffer_size, "%d", i); };
    time.report<METRIC_TIME>("Device ID", convert);
}

static void
kernel_launch_latency_init(void)
{
    for (uint8_t i = 0 ; i < XKRT_DRIVER_TYPE_MAX ; ++i)
    {
        xkrt_driver_type_t driver_type = (xkrt_driver_type_t) i;
        if (xkrt_support_driver(driver_type))
        {
            xkrt_driver_t * driver = runtime.driver_get(driver_type);
            if (!driver || !driver->f_module_load)
                continue ;

            assert(driver->f_module_load);
            assert(driver->f_module_unload);
            assert(driver->f_module_get_fn);

            LOGGER_INFO("Loading empty kernel for driver `%s`", driver->f_get_name());
            for (xkrt_device_global_id_t device_driver_id ; device_driver_id < driver->ndevices_commited ; ++device_driver_id)
            {
                xkrt_device_t * device = xkrt_driver_device_get(driver, device_driver_id);
                assert(device);

                xkrt_driver_module_fn_t * dst = NULL;
                uint8_t * bin;
                size_t size;
                switch (driver_type)
                {
                    # if XKRT_SUPPORT_ZE
                    case (XKRT_DRIVER_TYPE_ZE):
                    {
                        bin  = (uint8_t *) SPIRV_EMPTY_KERNEL;
                        size = sizeof(SPIRV_EMPTY_KERNEL);
                        dst  = XKBM_ZE_KERNEL_EMPTY + device_driver_id;
                        break ;
                    }
                    # endif /* XKRT_SUPPORT_ZE */

                    # if XKRT_SUPPORT_CUDA
                    case (XKRT_DRIVER_TYPE_CUDA):
                    {
                        bin  = (uint8_t *) PTX_EMPTY_KERNEL;
                        size = sizeof(PTX_EMPTY_KERNEL);
                        dst  = XKBM_CU_KERNEL_EMPTY + device_driver_id;
                        break ;
                    }
                    # endif /* XKRT_SUPPORT_CUDA */

                    default:
                    {
                        LOGGER_WARN("Driver `%s` not supported for kernel latency", driver->f_get_name());
                        continue ;
                    }
                }

                if (!dst)
                    continue ;

                xkrt_driver_module_t module = driver->f_module_load(device->driver_id, bin, size);
                *dst = driver->f_module_get_fn(module, "empty_kernel");
                // driver->f_module_unload(module);
                assert(*dst);
            }
        }
    }
}

static benchmark_node_t kernel_launch_latency = {
    .name = "launch-latency",
    .desc = "Time to launch and complete a kernel",
    .run = kernel_launch_latency_launch<false>,
};

//////////////////
// alloc device //
//////////////////

typedef enum    alloc_mode_t
{
    SYSTEM,
    RUNTIME,
    DRIVER
}               alloc_mode_t;

typedef struct  alloc_chunk_t
{
    void * ptr;
    size_t size;
    alloc_chunk_t(void * ptr, size_t size) : ptr(ptr), size(size) {}
}               alloc_chunk_t;

template<alloc_mode_t mode>
static void *
alloc_device_run_fragmented(xkrt_team_t * team, xkrt_thread_t * thread)
{
    xkrt_device_global_id_t device_global_id = (xkrt_device_global_id_t) thread->tid;

    srand(16112003);

    xkrt_device_t * device = runtime.device_get(device_global_id);
    xkrt_driver_t * driver = runtime.driver_get(device->driver_type);
    assert(driver->f_memory_device_allocate);
    assert(driver->f_memory_device_deallocate);

    // TODO : update if the device has more than 1x memory
    const int memory_index = 0;
    const xkrt_device_memory_info_t * meminfo = device->memories + memory_index;

    std::list<alloc_chunk_t> chunks;

    size_t total_allocated = 0;
    size_t resident = 0;
    size_t nallocs = 0;
    size_t nfree = 0;
    uint64_t time_alloc = 0;
    uint64_t time_free = 0;

    runtime.team_critical_begin(team);
    while (1)
    {
        const size_t size = 512 * 1024 + (rand() % (128 * 1024 * 1024));
        void * ptr;

        uint64_t t0 = xkrt_get_nanotime();
        if constexpr(mode == RUNTIME)
            ptr = device->memory_allocate_on(size, memory_index);
        else if constexpr(mode == DRIVER)
            ptr = driver->f_memory_device_allocate(device_global_id, size, memory_index);
        uint64_t tf = xkrt_get_nanotime();
        time_alloc += (tf - t0);
        if (ptr == NULL)
            break ;

        total_allocated += size;
        resident += size;
        ++nallocs;
        chunks.push_back(alloc_chunk_t(ptr, size));

        if (rand() % 4 == 0)
        {
            alloc_chunk_t chunk = chunks.front();
            chunks.pop_front();
            uint64_t t0 = xkrt_get_nanotime();
            if constexpr(mode == RUNTIME)
                device->memory_deallocate_on((xkrt_area_chunk_t *) ptr, memory_index);
            else if constexpr(mode == DRIVER)
                driver->f_memory_device_deallocate(device_global_id, chunk.ptr, chunk.size, memory_index);
            uint64_t tf = xkrt_get_nanotime();
            time_free += (tf - t0);
            ++nfree;
            resident -= chunk.size;
        }
    }

    if constexpr(mode == DRIVER)
        for (auto & chunk : chunks)
            driver->f_memory_device_deallocate(device_global_id, chunk.ptr, chunk.size, memory_index);
    device->memory_reset_on(memory_index);

    char b1[32], b2[32], b3[32], b4[32];
    xkrt_metric_byte(b1, sizeof(b1), total_allocated);
    xkrt_metric_byte(b2, sizeof(b2), resident);
    xkrt_metric_time(b3, sizeof(b3), nallocs == 0 ? 0 : time_alloc / nallocs);
    xkrt_metric_time(b4, sizeof(b4), nfree == 0 ? 0 : time_free / nfree);
    LOGGER_INFO(
        "Allocated `%s` bytes in `%zu` allocs and `%zu` free until first allocation failed "
        "with `%s` resident bytes (=%.1lf%% occupancy) - with `%s` per alloc and `%s` per free in average",
        b1, nallocs, nfree, b2, resident/(double)meminfo->capacity*100.0, b3, b4
    );
    runtime.team_critical_end(team);

    return NULL;
}

template<alloc_mode_t mode>
static void *
alloc_device_run(xkrt_team_t * team, xkrt_thread_t * thread)
{
    xkrt_device_global_id_t device_global_id = (xkrt_device_global_id_t) thread->tid;

    time_array_t time_alloc(30, 5);
    time_array_t time_dealloc(30, 5);

    xkrt_device_t * device = runtime.device_get(device_global_id);
    xkrt_driver_t * driver = runtime.driver_get(device->driver_type);
    assert(driver->f_memory_device_allocate);
    assert(driver->f_memory_device_deallocate);

    // TODO : update if the device get more memory
    const int memory_index = 0;

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
                    if constexpr(mode == RUNTIME)
                        ptr = runtime.memory_device_allocate(device_global_id, size);
                    else if constexpr(mode == DRIVER)
                        ptr = driver->f_memory_device_allocate(device_global_id, size, memory_index);

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
                    if constexpr(mode == RUNTIME)
                        runtime.memory_device_deallocate(device_global_id, (xkrt_area_chunk_t *) ptr);
                    else if constexpr(mode == DRIVER)
                        driver->f_memory_device_deallocate(device_global_id, ptr, size, memory_index);
                }
                uint64_t tf = xkrt_get_nanotime();
                time_dealloc.set(i, iter, tf - t0);
            }
        }
    }
    runtime.team_critical_end(team);

    const char * smode  = (mode == DRIVER) ? "driver" : "runtime";
    LOGGER_INFO("---- Device %u (alloc with %s) ----", device_global_id, smode);
    time_alloc.report<decltype(time_alloc)::pp_1byte_1time>("Memory Size");

    LOGGER_INFO("---- Device %u (dealloc with %s) ----", device_global_id, smode);
    time_dealloc.report<decltype(time_dealloc)::pp_1byte_1time>("Memory Size");

    return NULL;
}

template <void * (*func)(xkrt_team_t * team, xkrt_thread_t * thread)>
static void
foreach_device_checkmem(benchmark_node_t * bench)
{
    foreach_device<func>(bench);
}

static benchmark_node_t allocation_device_driver_fragmented = {
    .name = "driver-fragmented",
    .desc = "Time to allocate/deallocate device memory through the driver directly - with random accesses",
    .run = foreach_device_checkmem<alloc_device_run_fragmented<DRIVER>>
};

static benchmark_node_t allocation_device_runtime_fragmented = {
    .name = "runtime-fragmented",
    .desc = "Time to allocate/deallocate device memory through the runtime custom allocator that overrides driver's allocator - with random accesses",
    .run = foreach_device_checkmem<alloc_device_run_fragmented<RUNTIME>>
};

static benchmark_node_t allocation_device_driver = {
    .name = "driver",
    .desc = "Time to allocate/deallocate device memory through the driver directly",
    .run = foreach_device_checkmem<alloc_device_run<DRIVER>>
};

static benchmark_node_t allocation_device_runtime = {
    .name = "runtime",
    .desc = "Time to allocate/deallocate device memory through the runtime custom allocator that overrides driver's allocator",
    .run = foreach_device_checkmem<alloc_device_run<RUNTIME>>
};

////////////////
// alloc host //
////////////////

template<alloc_mode_t mode, bool touch>
static void *
alloc_host_run(xkrt_team_t * team, xkrt_thread_t * thread)
{
    xkrt_device_global_id_t device_global_id = (xkrt_device_global_id_t) thread->tid;

    time_array_t time_alloc(34, 5);
    time_array_t time_dealloc(34, 5);

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
    time_alloc.report<decltype(time_alloc)::pp_1byte_1time>("Memory Size");

    LOGGER_INFO("---- Device %u (dealloc with %s and %s) ----", device_global_id, smode, stouch);
    time_dealloc.report<decltype(time_dealloc)::pp_1byte_1time>("Memory Size");

    return 0;
}

static benchmark_node_t system_touch = {
    .name = "system-touch",
    .desc = "Time (allocation+touch) and (deallocation) of host memory using the system host-allocator",
    .run = foreach_device<alloc_host_run<SYSTEM, true>>
};

static benchmark_node_t system_notouch = {
    .name = "system-no-touch",
    .desc = "Time (allocation) and (deallocation) of host memory using the system host-allocator",
    .run = foreach_device<alloc_host_run<SYSTEM, false>>
};

static benchmark_node_t driver_touch = {
    .name = "driver-touch",
    .desc = "Time (allocation+touch) and (deallocation) of host memory using the system host-allocator",
    .run = foreach_device<alloc_host_run<DRIVER, true>>
};

static benchmark_node_t driver_notouch = {
    .name = "driver-no-touch",
    .desc = "Time (allocation) and (deallocation) of host memory using the system host-allocator",
    .run = foreach_device<alloc_host_run<DRIVER, false>>
};

//////////////////////////////////////////////

static void *
alloc_parallel_run(xkrt_team_t * team, xkrt_thread_t * thread)
{
    xkrt_device_global_id_t device_global_id = (xkrt_device_global_id_t) thread->tid;
    time_array_t time_alloc(34, 5);

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
        time_alloc.report<decltype(time_alloc)::pp_1byte_1time>("Memory Size");

    return NULL;
}

static benchmark_node_t driver_notouch_parallel = {
    .name = "host-driver-no-touch-parallel",
    .desc = "Time (allocation) of host memory using the driver host-allocator in parallel on every devices",
    .run = foreach_device<alloc_parallel_run>
};

///////////////////////////
// MEMORY TYPE DETECTION //
///////////////////////////

static void
detect_run(benchmark_node_t * bench)
{
}

static benchmark_node_t detect = {
    .name = "detect",
    .desc = "Detect",
    .run = detect_run
};

//////////////////////////
// D2D memory transfers //
//////////////////////////

typedef enum    parallel_mode_t
{
    P2P,
    ALL,
}               parallel_mode_t;

typedef enum    dir_t
{
    RECV = 0,
    SEND = 1
}               dir_t;

typedef enum    transfer_mode_t
{
    LATENCY,
    BANDWIDTH,
}               transfer_mode_t;

template<int nchunks, parallel_mode_t mode, transfer_mode_t transfer_mode>
static void *
mem_transfer_run_d2d(xkrt_team_t * team, xkrt_thread_t * thread)
{
    tls_t * tls = (tls_t *) team->desc.args;
    assert(tls);

    xkrt_device_global_id_t src_device_global_id = (xkrt_device_global_id_t) thread->tid;
    xkrt_device_t * src_device = runtime.device_get(src_device_global_id);
    assert(src_device);

    ////////////////////////////////////
    // Allocate memory on each device //
    ////////////////////////////////////

    // size of memory to transfer
    // const size_t size = (size_t) 1 * 1024 * 1024 * 1024;
    const size_t size = (transfer_mode == LATENCY) ? 1 : (transfer_mode == BANDWIDTH) ? (size_t) 1 * 512 * 1024 * 1024 : 0;
    assert(size);
    const size_t chunk_size = size / nchunks;

    for (xkrt_device_global_id_t device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
        for (int i = 0 ; i < src_device->nmemories ; ++i)
            for (int j = 0 ; j < 2 ; ++j)
                if ((tls->areas[src_device_global_id][device_global_id][i][j] = src_device->memory_allocate_on(size, i)) == NULL)
                    LOGGER_FATAL("oom");
    runtime.team_barrier(team);

    /////////////////////
    // Do the transfer //
    /////////////////////

    if (mode == P2P)
        runtime.team_critical_begin(team);

    for (int i = 0 ; i < src_device->nmemories ; ++i)
    {
        constexpr int niters = (transfer_mode == LATENCY) ? 1001 : (transfer_mode == BANDWIDTH) ? 5 : 0;
        static_assert(niters);
        time_array_t time(runtime.drivers.devices.n*XKRT_DEVICE_MEMORIES_MAX, niters);

        for (xkrt_device_global_id_t dst_device_global_id = 0 ; dst_device_global_id < runtime.drivers.devices.n ; ++dst_device_global_id)
        {
            xkrt_device_t * dst_device = runtime.device_get(dst_device_global_id);
            assert(dst_device);

            for (int j = 0 ; j < dst_device->nmemories ; ++j)
            {
                xkrt_area_chunk_t * src_chunk = tls->areas[src_device_global_id][dst_device_global_id][i][1];
                assert(src_chunk);

                xkrt_area_chunk_t * dst_chunk = tls->areas[dst_device_global_id][src_device_global_id][j][0];
                assert(dst_chunk);

                for (int iter = -1 ; iter < time.niters ; ++iter)
                {
                    if (mode == ALL)
                        runtime.team_barrier(team);

                    uint64_t t0 = xkrt_get_nanotime();
                    {
                        for (int c = 0 ; c < nchunks ; ++c)
                        {
                            xkrt_memory_copy_async(
                               &runtime,
                                src_device_global_id,
                                dst_device_global_id,
                                (uintptr_t)dst_chunk->ptr + c * chunk_size,
                                src_device_global_id,
                                (uintptr_t)src_chunk->ptr + c * chunk_size,
                                chunk_size
                            );
                        }
                        xkrt_sync(&runtime);
                    }
                    uint64_t tf = xkrt_get_nanotime();

                    if (iter >= 0)
                    {
                        const size_t size = chunk_size * nchunks;
                        const size_t bw = size / ((tf - t0) / 1e9);
                        time.set(dst_device_global_id*XKRT_DEVICE_MEMORIES_MAX+j, iter, bw);
                    }
                } /* iter */
            } /* dst memories */
        } /* dst device */

        if (mode == ALL)
            runtime.team_critical_begin(team);

        LOGGER_INFO("### From Device %u ###", src_device_global_id);
        auto convert = [] (char * buffer, size_t buffer_size, int i) { snprintf(buffer, buffer_size, "%d", i); };
        constexpr metric_t metric = transfer_mode == LATENCY ? METRIC_TIME : METRIC_BW;
        time.report<metric>("Device", convert);

        if (mode == ALL)
            runtime.team_critical_end(team);

    } /* src memories */

    if (mode == P2P)
        runtime.team_critical_end(team);

    runtime.team_barrier(team);

    //////////////////////////////////////
    // Deallocate memory on each device //
    //////////////////////////////////////

    # if 1
    src_device->memory_reset();
    # else
    for (xkrt_device_global_id_t dst_device_global_id = 0 ; device_global_id < runtime.drivers.devices.n ; ++device_global_id)
        for (int i = 0 ; i < src_device->nmemories ; ++i)
            for (int j = 0 ; j < 2 ; ++j)
                src_device->memory_deallocate(tls->areas[src_device_global_id][dst_device_global_id][i][j]);
    # endif

    return NULL;
}

static benchmark_node_t bw_d2d_1_all = {
    .name = "D2D-1-ALLGATHER",
    .desc = "Device (global) memory to device (global) memory bandwidth - 1 chunk sent in parallel between all pair of GPUs",
    .run = foreach_device<mem_transfer_run_d2d<1, ALL, BANDWIDTH>>
};

static benchmark_node_t bw_d2d_16_all = {
    .name = "D2D-16-ALLGATHER",
    .desc = "Device (global) memory to device (global) memory bandwidth - 16 chunk sent in parallel between all pair of GPUs",
    .run = foreach_device<mem_transfer_run_d2d<16, ALL, BANDWIDTH>>
};

static benchmark_node_t bw_d2d_1_p2p = {
    .name = "D2D-1-P2P",
    .desc = "Device (global) memory to device (global) memory bandwidth - 1 chunk sent P2P a pair of GPUs after the other",
    .run = foreach_device<mem_transfer_run_d2d<1, P2P, BANDWIDTH>>
};

static benchmark_node_t bw_d2d_16_p2p = {
    .name = "D2D-16-P2P",
    .desc = "Device (global) memory to device (global) memory bandwidth - 16 chunk sent P2P a pair of GPUs after the other",
    .run = foreach_device<mem_transfer_run_d2d<16, P2P, BANDWIDTH>>
};

static benchmark_node_t lat_d2d = {
    .name = "D2D-LAT",
    .desc = "Device (global) memory to device (global) memory latency",
    .run  = foreach_device<mem_transfer_run_d2d<1, P2P, LATENCY>>
};

//////////////////////////////
// H2D, D2H memory transfer //
//////////////////////////////

typedef enum    direction_t
{
    H2D,
    D2H
}               direction_t;

template <direction_t direction, int nchunks, transfer_mode_t transfer_mode>
static void *
mem_transfer_run(xkrt_team_t * team, xkrt_thread_t * thread)
{
    xkrt_device_global_id_t device_global_id = (xkrt_device_global_id_t) thread->tid;

    xkrt_device_t * device = runtime.device_get(device_global_id);
    assert(device);

    constexpr int niters = (transfer_mode == LATENCY) ? 1001 : (transfer_mode == BANDWIDTH) ? 11 : 0;
    static_assert(niters);
    time_array_t time(device->nmemories, niters);
    for (int i = 0 ; i < device->nmemories ; ++i)
    {
        xkrt_device_memory_info_t * meminfo = device->memories + i;

        /////////////////////
        // allocate memory //
        /////////////////////

        // device alocation
        constexpr size_t size = (transfer_mode == LATENCY) ? 1 : (transfer_mode == BANDWIDTH) ? (size_t) 1 * 512 * 1024 * 1024 : 0;
        static_assert(size);
        constexpr size_t chunk_size = size / nchunks;
        const xkrt_area_chunk_t * chunk = device->memory_allocate_on(size, i);
        if (chunk == NULL)
            LOGGER_FATAL("Out of device memory");

        // host allocation
        void * host_mem = runtime.memory_host_allocate(device_global_id, size);
        xkbm_mem_touch(host_mem, size);

        /////////////////////
        // do the transfer //
        /////////////////////

        const xkrt_device_global_id_t src_device_global_id  = (direction == H2D) ? HOST_DEVICE_GLOBAL_ID  : device_global_id;
        const uintptr_t               src_device_mem        = (direction == H2D) ? (uintptr_t) host_mem   : (uintptr_t) chunk->ptr;
        const xkrt_device_global_id_t dst_device_global_id  = (direction == H2D) ? device_global_id       : HOST_DEVICE_GLOBAL_ID;
        const uintptr_t               dst_device_mem        = (direction == H2D) ? (uintptr_t) chunk->ptr : (uintptr_t) host_mem;

        // only 1 device at a time
        runtime.team_critical_begin(team);
        {
            for (int iter = -3 ; iter < time.niters ; ++iter)
            {
                uint64_t t0 = xkrt_get_nanotime();
                {
                    for (int i = 0 ; i < nchunks ; ++i)
                    {
                        xkrt_memory_copy_async(
                           &runtime,
                            device_global_id,
                            dst_device_global_id,
                            dst_device_mem + i * chunk_size,
                            src_device_global_id,
                            src_device_mem + i * chunk_size,
                            chunk_size
                        );
                    }
                    xkrt_sync(&runtime);
                }
                uint64_t tf = xkrt_get_nanotime();

                if (iter >= 0)
                {
                    const size_t size = chunk_size * nchunks;
                    const size_t bw = size / ((tf - t0) / 1e9);
                    time.set(i, iter, bw);
                }
            }
        }
        runtime.team_critical_end(team);

        /////////////
        // release //
        /////////////

        // device
        runtime.memory_device_deallocate_all(device_global_id);
        runtime.memory_host_deallocate(device_global_id, host_mem, size);

        //////////////////
        // print result //
        //////////////////
        runtime.team_critical_begin(team);
        {
            LOGGER_INFO("--- Device %u ---", device_global_id);
            auto convert = [] (char * buffer, size_t buffer_size, int i) { snprintf(buffer, buffer_size, "%d", i); };
            constexpr metric_t metric = transfer_mode == LATENCY ? METRIC_TIME : METRIC_BW;
            time.report<metric>("MemoryID", convert);
        }
        runtime.team_critical_end(team);
    }

    runtime.team_barrier(team);

    return NULL;
}

static benchmark_node_t bw_h2d_1 = {
    .name = "H2D-1",
    .desc = "Host memory to device (global) memory bandwidth",
    .run = foreach_device<mem_transfer_run<H2D, 1, BANDWIDTH>>
};

static benchmark_node_t bw_d2h_1 = {
    .name = "D2H-1",
    .desc = "Device (global) to host memory memory bandwidth",
    .run = foreach_device<mem_transfer_run<D2H, 1, BANDWIDTH>>
};

static benchmark_node_t bw_h2d_16 = {
    .name = "H2D-16",
    .desc = "Host memory to device (global) memory bandwidth with 16x chunks bulk copies",
    .run = foreach_device<mem_transfer_run<H2D, 16, BANDWIDTH>>
};

static benchmark_node_t bw_d2h_16 = {
    .name = "D2H-16",
    .desc = "Device (global) to host memory memory bandwidth with 16x chunks bulk copies",
    .run = foreach_device<mem_transfer_run<D2H, 16, BANDWIDTH>>
};

static benchmark_node_t lat_h2d = {
    .name = "H2D-LAT",
    .desc = "Host memory to device (global) memory latency",
    .run = foreach_device<mem_transfer_run<H2D, 1, LATENCY>>
};

static benchmark_node_t lat_d2h = {
    .name = "D2H-LAT",
    .desc = "Device (global) memory to host memory latency",
    .run = foreach_device<mem_transfer_run<D2H, 1, LATENCY>>
};

/////////////////////
// XKRT BENCHMARKS //
/////////////////////

static benchmark_node_t kernel = {
    .name = "kernel",
    .desc = "Kernel-related metrics",
    .enabled = 1
};

static benchmark_node_t allocation = {
    .name = "allocation",
    .desc = "Allocation",
    .enabled = 1
};

static benchmark_node_t allocation_host = {
    .name = "allocation-host",
    .desc = "Allocation on the host",
    .enabled = 1
};

static benchmark_node_t allocation_device = {
    .name = "allocation-device",
    .desc = "Allocation on the device",
    .enabled = 1
};

static benchmark_node_t latency = {
    .name = "latency",
    .desc = "Latency",
    .enabled = 1
};
static benchmark_node_t bandwidth = {
    .name = "bandwidth",
    .desc = "Bandwidth",
    .enabled = 1
};
static benchmark_node_t transfer = {
    .name = "transfer",
    .desc = "Transfer",
    .enabled = 1
};

static benchmark_node_t memory = {
    .name = "memory",
    .desc = "Memory-related metrics",
    .enabled = 1
};

static void
check_devices(benchmark_node_t * bench)
{
    if (runtime.drivers.devices.n == 0)
    {
        LOGGER_WARN("No xkrt devices, disabling all tests");
        for (int i = 0 ; i < bench->nchildren ; ++i)
            bench->children[i]->enabled = 0;
    }

}

static benchmark_node_t xkrt = {
    .name = "xkrt",
    .desc = "Metrics on XKRT-supported devices",
    .run = check_devices,
    .enabled = 1
};

void
xkrt_benchmark_push(benchmark_node_t * parent)
{
    benchmark_push_children(parent, &xkrt);

    # define LINK(X, Y) benchmark_push_children(&X, &Y)

    LINK(xkrt, kernel);
    LINK(xkrt, memory);

    LINK(kernel, kernel_launch_latency);

    LINK(memory, allocation);
    LINK(memory, detect);
    LINK(memory, transfer);

    LINK(allocation, allocation_host);
    LINK(allocation_host, system_touch);
    LINK(allocation_host, system_notouch);
    LINK(allocation_host, driver_touch);
    LINK(allocation_host, driver_notouch);
    LINK(allocation_host, driver_notouch_parallel);

    LINK(allocation, allocation_device);
    LINK(allocation_device, allocation_device_driver);
    LINK(allocation_device, allocation_device_runtime);
    LINK(allocation_device, allocation_device_driver_fragmented);
    LINK(allocation_device, allocation_device_runtime_fragmented);

    LINK(transfer, bandwidth);
    LINK(bandwidth, bw_h2d_1);
    LINK(bandwidth, bw_h2d_16);
    LINK(bandwidth, bw_d2h_1);
    LINK(bandwidth, bw_d2h_16);
    LINK(bandwidth, bw_d2d_1_p2p);
    LINK(bandwidth, bw_d2d_16_p2p);
    LINK(bandwidth, bw_d2d_1_all);
    LINK(bandwidth, bw_d2d_16_all);

    LINK(transfer, latency);
    LINK(latency, lat_h2d);
    LINK(latency, lat_d2h);
    LINK(latency, lat_d2d);
}

void
xkrt_benchmark_init(void)
{
    xkrt_init(&runtime);
    kernel_launch_latency_init();
}

void
xkrt_benchmark_deinit(void)
{
    xkrt_deinit(&runtime);
}
