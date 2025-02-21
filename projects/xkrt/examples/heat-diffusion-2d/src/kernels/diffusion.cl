# include <heat/consts.h>

/* A naive kernel to update the grid */
__kernel
void
diffusion_cuda_kernel(__global double * dgrid, __global double * sgrid, int tile_x, int tile_y)
{
    int i = get_global_id(0) + tile_x * TS;
    int j = get_global_id(1) + tile_y * TS;

    if (i > 0 && i < NX - 1 && j > 0 && j < NY - 1) {
        dgrid[j * NX + i] = sgrid[j * NX + i] + ALPHA * DT / (DX * DY) * (
                (sgrid[j * NX + i + 1] - 2 * sgrid[j * NX + i] + sgrid[j * NX + i - 1]) / (DX * DX) +
                (sgrid[(j + 1) * NX + i] - 2 * sgrid[j * NX + i] + sgrid[(j - 1) * NX + i]) / (DY * DY)
        );
    }
}
