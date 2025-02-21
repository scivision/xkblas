/* ************************************************************************** */
/*                                                                            */
/*   diffusion.cu                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 04:45:52 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/21 06:21:52 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# include <heat/consts.h>

/* A naive kernel to update the grid */
__global__
void
diffusion_cuda_kernel(TYPE * src, TYPE * dst, int tile_x, int tile_y)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x + tile_x * TS;
    int j = blockIdx.y * blockDim.y + threadIdx.y + tile_y * TS;

    if (i > 0 && i < NX - 1 && j > 0 && j < NY - 1)
    {
        GRID(dst, i, j) = GRID(src, i, j) + ALPHA * DT / (DX * DY) * (
                (GRID(src, i+1,   j) - 2 * GRID(src, i, j) + GRID(src, i-1,   j)) / (DX * DX) +
                (GRID(src,   i, j+1) - 2 * GRID(src, i, j) + GRID(src,   i, j-1)) / (DY * DY)
            );
    }
}

# include <xkrt/driver/driver-cuda.h>
# include <xkrt/logger/logger-cu.h>

extern "C"
void
diffusion_cuda(cudaStream_t stream, TYPE * src, TYPE * dst, int tile_x, int tile_y)
{
    dim3 blockDim(TS, TS);
    dim3 gridDim((NX + blockDim.x - 1) / blockDim.x, (NY + blockDim.y - 1) / blockDim.y);

    diffusion_cuda_kernel<<<gridDim, blockDim, 0, stream>>>(src, dst, tile_x, tile_y);
}
