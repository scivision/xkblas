/* ************************************************************************** */
/*                                                                            */
/*   main.cc                                                                  */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 04:40:12 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/21 07:07:45 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/xkrt.h>
# include <xkrt/driver/thread-producer.hpp>

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
            fprintf(file, "%.2f\n", GRID(g, i, j));

    fclose(file);
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

extern "C" void diffusion_cuda(cudaStream_t stream, TYPE * src, TYPE * dst, int tile_x, int tile_y);

static void
body_cuda(
    xkrt_stream_cuda_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    assert(stream);

    Task * task = (Task *) instr->kern.vargs;
    assert(task);

    args_t * args = (args_t *) (task + 1);
    assert(args);

    const Access * src = task->accesses + 0;
    const Access * dst = task->accesses + 1;

    cudaStream_t custream = stream->cu.handle.high;
    diffusion_cuda(custream, (TYPE *) src->device_view.addr, (TYPE *) dst->device_view.addr, args->tile_x, args->tile_y);
    CU_SAFE_CALL(cudaEventRecord(stream->cu.events.buffer[idx], custream));
}
# endif /* XKRT_SUPPORT_CUDA */

static void
update_cpu(TYPE * src, TYPE * dst)
{
    for (int i = 1; i < NX - 1; i++)
    {
        for (int j = 1; j < NY - 1; j++)
        {
            GRID(dst, i, j) = GRID(src, i, j) + ALPHA * DT / (DX * DY) * (
                (GRID(src, i+1,   j) - 2 * GRID(src, i, j) + GRID(src, i-1,   j)) / (DX * DX) +
                (GRID(src,   i, j+1) - 2 * GRID(src, i, j) + GRID(src,   i, j-1)) / (DY * DY)
            );
        }
    }
}

# if XKRT_SUPPORT_HOST
static void
body_cpu(
    xkrt_stream_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    Task * task = (Task *) instr->kern.vargs;
    assert(task);

    args_t * args = (args_t *) (task + 1);
    assert(args);

    Access * a_src = task->accesses + 0;
    Access * a_dst = task->accesses + 1;

    TYPE * src = (TYPE *) a_src->device_view.addr;
    TYPE * dst = (TYPE *) a_dst->device_view.addr;

    update_cpu(src, dst);
}
# endif /* XKRT_SUPPORT_HOST */

static void
setup_tasks(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

    # if XKRT_SUPPORT_HOST
    format.f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_cpu;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format.f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda;
    # endif /* XKRT_SUPPORT_CUDA */

    # if 0
    # if XKRT_SUPPORT_ZE
    format.f[XKRT_DRIVER_TYPE_ZE] = (task_format_func_t) body_ze;
    # endif /* XKRT_SUPPORT_ZE */

    # if XKRT_SUPPORT_CL
    format.f[XKRT_DRIVER_TYPE_CL] = (task_format_func_t) body_cl;
    # endif /* XKRT_SUPPORT_CL */
    # endif

    snprintf(format.label, sizeof(format.label), "heat-diffusion");
    diffusion_format_id = task_format_create(&(runtime.task_formats), &format);
}

//////////////////
// GRID UPDATES //
//////////////////

/* Initialize the grid */
static void
initialize(TYPE * grid1, TYPE * grid2)
{
    for (int i = 0; i < NX; i++)
    {
        for (int j = 0; j < NY; j++)
        {
            const int temp = (i == 0 || i == NX - 1 || j == 0 || j == NY - 1) ? TEMPERATURE_BOUNDARY : TEMPERATURE_INITIAL;
            GRID(grid1, i, j) = GRID(grid2, i, j) = temp;
        }
    }
}

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

    const int  x = tile_x * TS;
    const int  y = tile_y * TS;
    const int ld = NX;

    // TODO : dependences are wrong, fix me
    # define NACCESSES 2
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    new(task->accesses + 0) Access(MATRIX_COLMAJOR, src, ld, x, y, TS, TS, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 1) Access(MATRIX_COLMAJOR, dst, ld, x, y, TS, TS, sizeof(TYPE), ACCESS_MODE_RW);
    thread->resolve<NACCESSES>(task);
    # undef NACCESSES

    runtime.task_commit(task);
}

/* Simulate 1 time step */
static void
update(TYPE * src, TYPE * dst)
{
    for (int tile_x = 0; tile_x < NX / TS; ++tile_x)
        for (int tile_y = 0; tile_y < NY / TS; ++tile_y)
            update_tile(src, dst, tile_x, tile_y);
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
    TYPE * grid1 = (TYPE *) runtime.memory_host_allocate(0, NX * NY * sizeof(TYPE));
    TYPE * grid2 = (TYPE *) runtime.memory_host_allocate(0, NX * NY * sizeof(TYPE));

    // Set initial conditions
    initialize(grid1, grid2);

    // Time stepping
    for (int step = 0; step < N_STEP; ++step)
    {
        // Grid swap
        TYPE * src = (step % 2 == 0) ? grid1 : grid2;
        TYPE * dst = (step % 2 == 0) ? grid2 : grid1;

        // Export every other frames
        if (step % (N_STEP / N_VTK) == 0)
        {
            // Create a cohrency task, to gather data back from gpus
            int uplo = 0, memflag = 0, ld = NX;
            xkrt_memory_coherent_async(&runtime, uplo, memflag, NX, NY, dst, ld, sizeof(TYPE));

            // Wait for the completion of all tasks
            xkrt_sync(&runtime);

            // Export frame
            char filename[50];
            int frame = step / (N_STEP / N_VTK);
            snprintf(filename, sizeof(filename), "temperature_grid_%03d.vtk", frame);
            export_to_vtk(dst, filename, frame);
        }

        // Update
        update(src, dst);
    }

    // Finish remaining tasks
    xkrt_sync(&runtime);

    // Deinitialize xkrt runtime
    xkrt_deinit(&runtime);
}
