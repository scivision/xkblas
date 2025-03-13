/* ************************************************************************** */
/*                                                                            */
/*   main.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 04:40:12 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/13 14:24:51 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/logger/metric.h>
# include <xkrt/driver/thread.hpp>

# include <stdio.h>
# include <stdlib.h>

# include <heat/consts.h>

/* Making a global xkrt context of simplicity purposes */
static xkrt_runtime_t runtime;
static int TSX = 0;
static int TSY = 0;

////////////////
// VTK EXPORT //
////////////////

/* Export the grid to a vtk file */
void
export_to_vtk(TYPE * g, const char * filename, int step)
{
    FILE * file = fopen(filename, "w");
    if (file == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    fprintf(file, "# vtk DataFile Version 3.0\n");
    fprintf(file, "2D Heat Diffusion Time Step %d\n", step);
    fprintf(file, "ASCII\n");
    fprintf(file, "DATASET STRUCTURED_GRID\n");
    fprintf(file, "DIMENSIONS %d %d 1\n", NX, NY);
    fprintf(file, "POINTS %d float\n", NX * NY);

    for (int j = 0; j < NY; j++)
        for (int i = 0; i < NX; i++)
            fprintf(file, "%.2f %.2f 0.0\n", i*DX, j*DY);

    fprintf(file, "POINT_DATA %d\n", NX * NY);
    fprintf(file, "SCALARS temperature float\n");
    fprintf(file, "LOOKUP_TABLE default\n");

    for (int j = 0; j < NY; j++)
        for (int i = 0; i < NX; i++)
            fprintf(file, "%.2f\n", GRID(g, i, j, LD));

    fclose(file);
}

static void
export_to_vtk(int frame, TYPE * grid)
{
    // Export frame
    char filename[50];
    snprintf(filename, sizeof(filename), "temperature_grid_%03d.vtk", frame);
    export_to_vtk(grid, filename, frame);
}

typedef struct  args_export_t
{
    TYPE * grid;
    int frame;
    args_export_t(TYPE * grid, int frame) : grid(grid), frame(frame) {}
}               args_export_t;

static void
body_export_vtk(task_t * task)
{
    args_export_t * args = (args_export_t *) TASK_ARGS(task);
    LOGGER_INFO("Exporting frame %d/%d", args->frame+1, N_VTK);
    export_to_vtk(args->frame, args->grid);
}

static task_format_id_t export_vtk_format_id;

static void
maybe_export(int step, TYPE * grid)
{
    if (N_VTK)
    {
        if (step % (N_STEP / N_VTK) == 0)
        {
            int frame = step / (N_STEP / N_VTK);

            # if 1
            Thread * thread = Thread::self();

            # define AC 1
            constexpr task_flag_bitfield_t flags = TASK_FLAG_DEPENDENT;
            constexpr size_t task_size = task_compute_size(flags, AC);
            constexpr size_t args_size = sizeof(args_export_t);

            task_t * task = thread->allocate_task(task_size + args_size);
            new(task) task_t(export_vtk_format_id, flags);

            task_dep_info_t * dep = TASK_DEP_INFO(task);
            new (dep) task_dep_info_t(AC);

            args_export_t * args = (args_export_t *) TASK_ARGS(task, task_size);
            new(args) args_export_t(grid, frame);

            # ifndef NDEBUG
            snprintf(task->label, sizeof(task->label), "export-vtk-%d", frame);
            # endif

            static_assert(AC <= TASK_MAX_ACCESSES);
            access_t * accesses = TASK_ACCESSES(task, flags);
            {
                const ssize_t x0 = 0;
                const ssize_t y0 = 0;
                const ssize_t x1 = NX;
                const ssize_t y1 = NY;
                const  size_t sx = x1 - x0;
                const  size_t sy = y1 - y0;
                new(accesses + 0) access_t(task, MATRIX_COLMAJOR, grid, LD, x0, y0, sx, sy, sizeof(TYPE), ACCESS_MODE_R);
            }
            thread->resolve<AC>(task, accesses);
            # undef AC

            runtime.task_commit(task);

            # else

            xkrt_coherency_host_async(&runtime, MATRIX_COLMAJOR, grid, LD, NX, NY, sizeof(TYPE));
            xkrt_sync(&runtime);
            export_to_vtk(frame, grid);

            # endif
        }
    }
}

////////////////////
// DIFFUSION TASK //
////////////////////

static task_format_id_t diffusion_format_id;

typedef struct  args_t
{
    TYPE * src;
    TYPE * dst;
    const int tile_x;
    const int tile_y;
    args_t(TYPE * src, TYPE * dst, int tx, int ty) : src(src), dst(dst), tile_x(tx), tile_y(ty) {}
}               args_t;

# if XKRT_SUPPORT_CUDA

# include <xkrt/driver/driver-cu.h>
# include <xkrt/logger/logger-cu.h>

extern "C" void diffusion_cuda(cudaStream_t stream, TYPE * src, int ld_src, TYPE * dst, int ld_dst, int tile_x, int tile_y, int tsx, int tsy);

static void
body_cuda(
    xkrt_stream_cu_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    task_t * task = (task_t *) instr->kern.vargs;
    args_t * args = (args_t *) TASK_ARGS(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * a_src = accesses + 0;
    const access_t * a_dst = accesses + 1;

    TYPE * src = (TYPE *) a_src->device_view.addr;
    TYPE * dst = (TYPE *) a_dst->device_view.addr;
    const size_t ld_src = a_src->device_view.ld;
    const size_t ld_dst = a_dst->device_view.ld;

    // offset boundary access so the kernel receive the correct pointer
    if (args->tile_x == 0)
        dst = dst - 1;
    else
        src = src + 1;

    if (args->tile_y == 0)
        dst = dst - ld_dst;
    else
        src = src + ld_src;

    // submit kernel
    cudaStream_t custream = stream->cu.handle.high;
    diffusion_cuda(
        custream,
        src, ld_src,
        dst, ld_dst,
        args->tile_x, args->tile_y,
        TSX, TSY
    );
    CUDA_SAFE_CALL(cudaEventRecord(stream->cu.events.buffer[idx], custream));
}
# endif /* XKRT_SUPPORT_CUDA */

# if XKRT_SUPPORT_ZE

# include <xkrt/driver/driver-ze.h>
# include <xkrt/logger/logger-ze.h>
# include <ze_api.h>

static unsigned char SPIRV_DIFFUSION_KERNEL[] = {
    # include "kernels/diffusion.spv.bytes"
};

static xkrt_driver_module_t ZE_MODULES[XKRT_DEVICES_MAX];
static xkrt_driver_module_fn_t ZE_KERNELS[XKRT_DEVICES_MAX];

static void
body_ze(
    xkrt_stream_ze_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    task_t * task = (task_t *) instr->kern.vargs;
    args_t * args = (args_t *) TASK_ARGS(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * a_src = accesses + 0;
    const access_t * a_dst = accesses + 1;

    TYPE * src = (TYPE *) a_src->device_view.addr;
    TYPE * dst = (TYPE *) a_dst->device_view.addr;
    const int ld_src = (int) a_src->device_view.ld;
    const int ld_dst = (int) a_dst->device_view.ld;

    // offset boundary access so the kernel receive the correct pointer
    if (args->tile_x == 0)
        dst = dst - 1;
    else
        src = src + 1;

    if (args->tile_y == 0)
        dst = dst - ld_dst;
    else
        src = src + ld_src;
    // kernel(__global TYPE * src, int ld_src, __global TYPE * dst, int ld_dst, int tile_x, int tile_y, int tsx, int tsy)
    ze_kernel_handle_t fn = (ze_kernel_handle_t) ZE_KERNELS[stream->device->inherited.driver_id];
    src = NULL;
    dst = NULL;
    ZE_SAFE_CALL(zeKernelSetArgumentValue(fn, 0, sizeof(TYPE *),    &src));
    ZE_SAFE_CALL(zeKernelSetArgumentValue(fn, 1, sizeof(int),       &ld_src));
    ZE_SAFE_CALL(zeKernelSetArgumentValue(fn, 2, sizeof(TYPE *),    &dst));
    ZE_SAFE_CALL(zeKernelSetArgumentValue(fn, 3, sizeof(int),       &ld_dst));
    ZE_SAFE_CALL(zeKernelSetArgumentValue(fn, 4, sizeof(int),       &args->tile_x));
    ZE_SAFE_CALL(zeKernelSetArgumentValue(fn, 5, sizeof(int),       &args->tile_y));
    ZE_SAFE_CALL(zeKernelSetArgumentValue(fn, 6, sizeof(int),       &TSX));
    ZE_SAFE_CALL(zeKernelSetArgumentValue(fn, 7, sizeof(int),       &TSY));

    uint32_t gx, gy, gz;
    ZE_SAFE_CALL(zeKernelSuggestGroupSize(fn, TSX, TSY, 1, &gx, &gy, &gz));
    ZE_SAFE_CALL(zeKernelSetGroupSize(fn, gx, gy, gz));

    ze_group_count_t gc = { (uint32_t) TSX, (uint32_t) TSY, 1 };
    ze_event_handle_t event = stream->ze.events.list[idx];
    ZE_SAFE_CALL(zeCommandListAppendLaunchKernel(stream->ze.command.list, fn, &gc, event, 0, NULL));
}
# endif /* XKRT_SUPPORT_CUDA */

static void
update_cpu(TYPE * src, TYPE * dst)
{
    for (int i = 1; i < NX - 1; i++)
    {
        for (int j = 1; j < NY - 1; j++)
        {
            GRID(dst, i, j, LD) = GRID(src, i, j, LD) + ALPHA * DT / (DX * DY) * (
                (GRID(src, i+1,   j, LD) - 2 * GRID(src, i, j, LD) + GRID(src, i-1,   j, LD)) / (DX * DX) +
                (GRID(src,   i, j+1, LD) - 2 * GRID(src, i, j, LD) + GRID(src,   i, j-1, LD)) / (DY * DY)
            );
        }
    }
}

////////////////
// XKRT TASKS //
////////////////

static void
setup_tasks(void)
{
    // compile kernels
    {
        # if XKRT_SUPPORT_ZE
        xkrt_driver_t * driver = runtime.driver_get(XKRT_DRIVER_TYPE_ZE);
        assert(driver);

        for (int i = 0 ; i < runtime.drivers.devices.n ; ++i)
        {
            ZE_MODULES[i] = driver->f_module_load(i, SPIRV_DIFFUSION_KERNEL, sizeof(SPIRV_DIFFUSION_KERNEL));
            assert(ZE_MODULES[i]);

            ZE_KERNELS[i] = driver->f_module_get_fn(ZE_MODULES[i], "diffusion_cl_kernel");
            assert(ZE_KERNELS[i]);
        }
        # endif /* XKRT_SUPPORT_ZE */
    }

    // diffusion
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));

        # if XKRT_SUPPORT_CUDA
        format.f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda;
        # endif /* XKRT_SUPPORT_CUDA */

        # if XKRT_SUPPORT_ZE
        format.f[XKRT_DRIVER_TYPE_ZE] = (task_format_func_t) body_ze;
        # endif /* XKRT_SUPPORT_ZE */

        snprintf(format.label, sizeof(format.label), "heat-diffusion");
        diffusion_format_id = task_format_create(&(runtime.formats.list), &format);
    }

    // export vtk
    {
        task_format_t format;
        memset(&format, 0, sizeof(task_format_t));

        format.f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_export_vtk;
        snprintf(format.label, sizeof(format.label), "export-vtk");
        export_vtk_format_id = task_format_create(&(runtime.formats.list), &format);
    }
}

//////////////////
// GRID UPDATES //
//////////////////

/* Initialize the grid */
static void
initialize(TYPE * grid1, TYPE * grid2)
{
    LOGGER_WARN("Initializing grid on the host");

    memset(grid1, 0, NX*NY*sizeof(TYPE));
    memset(grid2, 0, NX*NY*sizeof(TYPE));

    for (int i = 0; i < NX; ++i)
    {
        GRID(grid1, i,    0, LD) = GRID(grid2, i,    0, LD) = TEMPERATURE_BOUNDARY;
        GRID(grid1, i, NY-1, LD) = GRID(grid2, i, NY-1, LD) = TEMPERATURE_BOUNDARY;
    }

    for (int i = 0; i < NY; ++i)
    {
        GRID(grid1,    0, i, LD) = GRID(grid2,    0, i, LD) = TEMPERATURE_BOUNDARY;
        GRID(grid1, NX-1, i, LD) = GRID(grid2, NX-1, i, LD) = TEMPERATURE_BOUNDARY;
    }

    LOGGER_WARN("Initialized grid on the host");
}

// omp interfaces would look like
//
//  # pragma omp task   format(diffusion_format_id)                                     \
//                      access(read:  matrix(colmajor, src, ld, x0, y0, sx, sy))        \
//                      access(write: matrix(colmajor, dst, LD, x0, y0, sx, sy))
//
// with some
//
//  omp_task_format_id_t diffusion_format_id;
//  # pragma omp task-format(diffusion_format_id) create
//
//  # pragma omp task-format(diffusion_format_id) target(LEVEL_ZERO)
//      body_level_zero();  // task context is implicit, can retrieve accesses
//
//  # pragna omp task-format(diffusion_format_id) target(CUDA)
//      body_cuda();
//
//  [...]

/* Submit a tile */
static void
update_tile(TYPE * src, TYPE * dst, int tile_x, int tile_y, int step)
{
    Thread * thread = Thread::self();

    # define AC 2
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t);

    task_t * task = thread->allocate_task(task_size + args_size);
    new(task) task_t(diffusion_format_id, flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr uint8_t ocr_access = 1;
    new (dev) task_dev_info_t(UNSPECIFIED_DEVICE_GLOBAL_ID, ocr_access);

    args_t * args = (args_t *) TASK_ARGS(task, task_size);
    new(args) args_t(src, dst, tile_x, tile_y);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "diffusion(%d, %d, s=%d)", tile_x, tile_y, step);
    # endif

    const int ntx = NUM_OF_TILES(NX, TSX);
    const int nty = NUM_OF_TILES(NY, TSY);

    const int x = (tile_x * TSX);
    const int y = (tile_y * TSY);

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    {
        const ssize_t x0 = MAX(x-1, 0);
        const ssize_t y0 = MAX(y-1, 0);
        const ssize_t x1 = MIN(x+TSX+1, NX);
        const ssize_t y1 = MIN(y+TSY+1, NY);
        const  size_t sx = x1 - x0;
        const  size_t sy = y1 - y0;
        new(accesses + 0) access_t(task, MATRIX_COLMAJOR, src, LD, x0, y0, sx, sy, sizeof(TYPE), ACCESS_MODE_R);
    }
    {
        const ssize_t x0 = MAX(x, 1);

        const ssize_t y0 = MAX(y, 1);
        const ssize_t x1 = MIN(x+TSX, NX-1);
        const ssize_t y1 = MIN(y+TSY, NY-1);
        const  size_t sx = x1 - x0;
        const  size_t sy = y1 - y0;
        new(accesses + 1) access_t(task, MATRIX_COLMAJOR, dst, LD, x0, y0, sx, sy, sizeof(TYPE), ACCESS_MODE_W);
    }
    thread->resolve<AC>(task, accesses);
    # undef AC

    runtime.task_commit(task);
}

/* Simulate 1 time step */
static void
update(TYPE * src, TYPE * dst, int step)
{
    # if 1
    const int ntx = NUM_OF_TILES(NX, TSX);
    const int nty = NUM_OF_TILES(NY, TSY);
    for (int tile_x = 0; tile_x < ntx; ++tile_x)
        for (int tile_y = 0; tile_y < nty; ++tile_y)
            update_tile(src, dst, tile_x, tile_y, step);
    # else
    update_cpu(src, dst);
    # endif
}

//////////
// MAIN //
//////////

int
main(void)
{
    // Initialize xkrt runtime
    xkrt_init(&runtime);
    if (runtime.drivers.devices.n == 0)
        LOGGER_FATAL("No devices found");

    // compute data partitioning
    TSX = NX / 1;
    TSY = NY / runtime.drivers.devices.n;

    // setup task format
    setup_tasks();

    // Allocate memory for the temperature grids on the CPU
    const size_t s = sizeof(TYPE);
    # if 1
    const uintptr_t alignon = NX * s;
    const size_t size = 2 * (NX * NY * s + alignon);
    const uintptr_t mem = (uintptr_t) runtime.memory_host_allocate(0, size);
    TYPE * grid1 = (TYPE *) (mem + (alignon - (mem % alignon)) + 0 * s * (NX * NY));
    TYPE * grid2 = (TYPE *) (mem + (alignon - (mem % alignon)) + 1 * s * (NX * NY));
    assert((uintptr_t)grid1 % alignon == 0);
    assert((uintptr_t)grid1 + NX * NY * s < mem + size);
    assert((uintptr_t)grid2 % alignon == 0);
    assert((uintptr_t)grid2 + NX * NY * s < mem + size);
    # else
    TYPE * grid1 = (TYPE *) runtime.memory_host_allocate(0, NX * NY * s);
    TYPE * grid2 = (TYPE *) runtime.memory_host_allocate(0, NX * NY * s);
    # endif

    // Set initial conditions
    initialize(grid1, grid2);

    // Create tasks to distribute memory
    xkrt_coherency_distribute_cyclic_2D_halo_async(&runtime, MATRIX_COLMAJOR, grid1, LD, NX, NY, TSX, TSY, sizeof(TYPE), 1, 1);
    xkrt_coherency_distribute_cyclic_2D_halo_async(&runtime, MATRIX_COLMAJOR, grid2, LD, NX, NY, TSX, TSY, sizeof(TYPE), 1, 1);

    uint64_t t0 = xkrt_get_nanotime();

    // Time stepping
    for (int step = 0; step < N_STEP; ++step)
    {
        // Grid swap
        TYPE * src = (step % 2 == 0) ? grid1 : grid2;
        TYPE * dst = (step % 2 == 0) ? grid2 : grid1;

        // Update
        update(src, dst, step);
        if (step % (N_STEP / 10 + 1) == 0)
            LOGGER_WARN("Progress: %.2lf%%", step / (double)N_STEP*100);

        // Export every other frames
        maybe_export(step, dst);
    }

    uint64_t tf = xkrt_get_nanotime();
    LOGGER_WARN("Graph Creation Took %.2lf s", (tf-t0)/1e9);
    // Finish remaining tasks
    xkrt_sync(&runtime);
    tf = xkrt_get_nanotime();
    LOGGER_WARN("Graph Execution Took %.2lf s", (tf-t0)/1e9);

    // Deinitialize xkrt runtime
    xkrt_deinit(&runtime);

    return 0;
}
