# include <xkrt/xkrt.h>

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
export_to_vtk(double * grid, const char * filename, int step)
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
            fprintf(file, "%.2f %.2f 0.0\n", i * DX, j * DY);

    fprintf(file, "POINT_DATA %d\n", NX * NY);
    fprintf(file, "SCALARS temperature float\n");
    fprintf(file, "LOOKUP_TABLE default\n");

    for (int j = 0; j < NY; j++)
        for (int i = 0; i < NX; i++)
            fprintf(file, "%.2f\n", grid[j*NX+i]);

    fclose(file);
}

///////////////
// XKRT TASK //
///////////////


// TODO


//////////////////
// GRID UPDATES //
//////////////////

/* Initialize the grid */
static void
initialize(double * grid1, double * grid2)
{
    for (int i = 0; i < NX; i++)
    {
        for (int j = 0; j < NY; j++)
        {
            const int temp = (i == 0 || i == NX - 1) && (j == 0 || j == NY - 1) ? TEMPERATURE_BOUNDARY : TEMPERATURE_INITIAL;
            grid1[j * NX + i] = grid2[j * NX + i] = temp;
        }
    }
}

/* Submit a tile */
static void
update_tile(double * dgrid, double * sgrid, int tile_x, int tile_y)
{
}

/* Simulate 1 time step */
static void
update(double * dgrid, double * sgrid)
{
    for (int tile_y = 0; tile_y < NY / TS; ++tile_y)
        for (int tile_x = 0; tile_x < NX / TS; ++tile_x)
            update_tile(dgrid, sgrid, tile_x, tile_y);
}

//////////
// MAIN //
//////////

int
main(void)
{
    // Initialize xkrt runtime
    xkrt_init(&runtime);

    // Allocate memory for the temperature grids on the CPU
    double * grid1 = (double *) runtime.memory_host_allocate(0, NX * NY * sizeof(double));
    double * grid2 = (double *) runtime.memory_host_allocate(0, NX * NY * sizeof(double));

    // Set initial conditions
    initialize(grid1, grid2);

    // Time stepping
    for (int step = 0; step < N_STEP; ++step)
    {
        // Update
        update(grid2, grid1);

        // Export every other frames
        if (step % (N_STEP / N_VTK) == 0)
        {
            // Create a cohrency task, to gather data back from gpus
            // TODO : coherent

            // Wait for the completion of all tasks
            xkrt_sync(&runtime);

            // Export frame
            char filename[50];
            int frame = step / (N_STEP / N_VTK);
            snprintf(filename, sizeof(filename), "temperature_grid_%03d.vtk", frame);
            export_to_vtk(grid1, filename, frame);
        }

        // Swap grids
        double * grid = grid1;
        grid1 = grid2;
        grid2 = grid;
    }

    // Finish remaining tasks
    xkrt_sync(&runtime);

    // Deinitialize xkrt runtime
    xkrt_deinit(&runtime);
}
