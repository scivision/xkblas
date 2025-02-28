/* ************************************************************************** */
/*                                                                            */
/*   main.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 04:40:12 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/27 22:38:06 by Romain PEREIRA            \_)     (_/    */
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

static void
maybe_export(int step, TYPE * grid)
{
    if (N_VTK)
    {
        if (step % (N_STEP / N_VTK) == 0)
        {
            // Create a cohrency task, to gather data back from gpus
            int uplo = 0, memflag = 0;
            xkrt_coherency_host_async(&runtime, MATRIX_COLMAJOR, grid, LD, NX, NY, sizeof(TYPE));

            // TODO : instead of a sync+coherent, maybe make a host task that reads data
            // Wait for the completion of all tasks
            xkrt_sync(&runtime);

            // export grid
            int frame = step / (N_STEP / N_VTK);
            export_to_vtk(frame, grid);
        }
    }
}

///////////////
// XKRT TASK //
///////////////

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

# include <xkrt/driver/driver-cuda.h>
# include <xkrt/logger/logger-cu.h>

extern "C" void diffusion_cuda(cudaStream_t stream, TYPE * src, int ld_src, TYPE * dst, int ld_dst, int tile_x, int tile_y);

static void
body_cuda(
    xkrt_stream_cuda_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    Task * task = (Task *) instr->kern.vargs;
    args_t * args = (args_t *) (task + 1);

    const Access * a_src = task->accesses + 0;
    const Access * a_dst = task->accesses + 1;

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
        args->tile_x, args->tile_y
    );
    CUDA_SAFE_CALL(cudaEventRecord(stream->cu.events.buffer[idx], custream));
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

static void
setup_tasks(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

    # if XKRT_SUPPORT_CUDA
    format.f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda;
    # endif /* XKRT_SUPPORT_CUDA */

    # if 0
    # if XKRT_SUPPORT_HOST
    format.f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_cpu;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_ZE
    format.f[XKRT_DRIVER_TYPE_ZE] = (task_format_func_t) body_ze;
    # endif /* XKRT_SUPPORT_ZE */

    # if XKRT_SUPPORT_CL
    format.f[XKRT_DRIVER_TYPE_CL] = (task_format_func_t) body_cl;
    # endif /* XKRT_SUPPORT_CL */
    # endif

    snprintf(format.label, sizeof(format.label), "heat-diffusion");
    diffusion_format_id = task_format_create(&(runtime.formats.list), &format);
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
update_tile(TYPE * src, TYPE * dst, int tile_x, int tile_y)
{
    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);

    ThreadProducer * thread = ThreadProducer::self();
    uint8_t * mem = thread->allocate(task_size + args_size);

    // const size_t ocr_access = UNSPECIFIED_TASK_ACCESS;
    const size_t ocr_access = 1;
    Task * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(diffusion_format_id, ocr_access, UNSPECIFIED_DEVICE_GLOBAL_ID);

    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(src, dst, tile_x, tile_y);

    const int ntx = NUM_OF_TILES(NX, TS);
    const int nty = NUM_OF_TILES(NY, TS);

    const int x = (tile_x * TS);
    const int y = (tile_y * TS);

    # define NACCESSES 2
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    {
        const ssize_t x0 = MAX(x-1, 0);
        const ssize_t y0 = MAX(y-1, 0);
        const ssize_t x1 = MIN(x+TS+1, NX);
        const ssize_t y1 = MIN(y+TS+1, NY);
        const  size_t sx = x1 - x0;
        const  size_t sy = y1 - y0;
        new(task->accesses + 0) Access(MATRIX_COLMAJOR, src, LD, x0, y0, sx, sy, sizeof(TYPE), ACCESS_MODE_R);
    }
    {
        const ssize_t x0 = MAX(x, 1);

        const ssize_t y0 = MAX(y, 1);
        const ssize_t x1 = MIN(x+TS, NX-1);
        const ssize_t y1 = MIN(y+TS, NY-1);
        const  size_t sx = x1 - x0;
        const  size_t sy = y1 - y0;
        new(task->accesses + 1) Access(MATRIX_COLMAJOR, dst, LD, x0, y0, sx, sy, sizeof(TYPE), ACCESS_MODE_W);
    }
    thread->resolve<NACCESSES>(task);
    # undef NACCESSES

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label), "diffusion(%d, %d)", tile_x, tile_y);
    # endif

    runtime.task_commit(task);
}

/* Simulate 1 time step */
static void
update(TYPE * src, TYPE * dst)
{
    # if 1
    const int ntx = NUM_OF_TILES(NX, TS);
    const int nty = NUM_OF_TILES(NY, TS);
    for (int tile_x = 0; tile_x < ntx; ++tile_x)
        for (int tile_y = 0; tile_y < nty; ++tile_y)
            update_tile(src, dst, tile_x, tile_y);
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
    setup_tasks();

    // Allocate memory for the temperature grids on the CPU
    const size_t s = sizeof(TYPE);
    # if 1
    const uintptr_t alignon = NX * s;
    const uintptr_t   mem   = (uintptr_t) runtime.memory_host_allocate(0, 2 * NX * NY * s + alignon);
    TYPE * grid1 = (TYPE *) (mem + (alignon - (mem % alignon)) + 0 * s * (NX * NY));
    TYPE * grid2 = (TYPE *) (mem + (alignon - (mem % alignon)) + 1 * s * (NX * NY));
    # else
    TYPE * grid1 = (TYPE *) runtime.memory_host_allocate(0, NX * NY * s);
    TYPE * grid2 = (TYPE *) runtime.memory_host_allocate(0, NX * NY * s);
    # endif

    // Set initial conditions
    initialize(grid1, grid2);

    // Create tasks to distribute memory
    # if 1
    xkrt_coherency_distribute_packed_2D_halo_async(&runtime, MATRIX_COLMAJOR, grid1, LD, NX, NY, sizeof(TYPE), 0, 0);
    # elif 0
    xkrt_coherency_distribute_packed_2D_async(&runtime, MATRIX_COLMAJOR, grid1, LD, NX, NY, sizeof(TYPE));
    # elif 0
    xkrt_coherency_distribute_cyclic_2D_halo_async(&runtime, MATRIX_COLMAJOR, grid1, LD, NX, NY, TS, TS, sizeof(TYPE), 1, 1);
    xkrt_coherency_distribute_cyclic_2D_halo_async(&runtime, MATRIX_COLMAJOR, grid2, LD, NX, NY, TS, TS, sizeof(TYPE), 1, 1);
    # elif 0
    xkrt_coherency_distribute_cyclic_2D_async(&runtime, MATRIX_COLMAJOR, grid1, LD, NX, NY, TS, TS, sizeof(TYPE));
    xkrt_coherency_distribute_cyclic_2D_async(&runtime, MATRIX_COLMAJOR, grid2, LD, NX, NY, TS, TS, sizeof(TYPE));
    # else
    # endif

    uint64_t t0 = xkrt_get_nanotime();

    // Time stepping
    for (int step = 0; step < N_STEP; ++step)
    {
        // Grid swap
        TYPE * src = (step % 2 == 0) ? grid1 : grid2;
        TYPE * dst = (step % 2 == 0) ? grid2 : grid1;

        // Update
        update(src, dst);
        if (step % (N_STEP / 10) == 0)
            LOGGER_WARN("Progress: %.2lf%%", step / (double)N_STEP*100);

        // Export every other frames
        maybe_export(step, dst);
    }

    uint64_t tf = xkrt_get_nanotime();
    LOGGER_WARN("Took %.2lf s", (tf-t0)/1e9);

    // Finish remaining tasks
    xkrt_sync(&runtime);

    // Deinitialize xkrt runtime
    xkrt_deinit(&runtime);
}
