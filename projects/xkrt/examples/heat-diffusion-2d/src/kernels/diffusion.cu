/* ************************************************************************** */
/*                                                                            */
/*   diffusion.cu                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 04:45:52 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/22 02:24:45 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <heat/consts.h>
# include <xkrt/min-max.h>

// Number of threads per block line
#  define DTS (MIN(32, TS))
static_assert(DTS <= TS);
static_assert(TS % DTS == 0);

/* A naive kernel to update the grid */
__global__
void
diffusion_cuda_kernel(TYPE * src, int ld_src, TYPE * dst, int ld_dst, int tile_x, int tile_y)
{
    const int li = blockIdx.x * blockDim.x + threadIdx.x;
    const int lj = blockIdx.y * blockDim.y + threadIdx.y;
    const int  i = tile_x * TS + li;
    const int  j = tile_y * TS + lj;

    # if 1
    // boundary conditions fixed
    if (i > 0 && i < NX - 1 && j > 0 && j < NY - 1)
    {
        GRID(dst, li, lj, ld_dst) = GRID(src, li, lj, ld_src) + ALPHA * DT / (DX * DY) * (
                (GRID(src, li+1,   lj, ld_src) - 2 * GRID(src, li, lj, ld_src) + GRID(src, li-1,   lj, ld_src)) / (DX * DX) +
                (GRID(src,   li, lj+1, ld_src) - 2 * GRID(src, li, lj, ld_src) + GRID(src,   li, lj-1, ld_src)) / (DY * DY)
            );
    }
    # else
    GRID(dst, li, lj, ld_dst) = GRID(src, li, lj, ld_src);
    # endif
}

# include <xkrt/driver/driver-cuda.h>
# include <xkrt/logger/logger-cu.h>

extern "C"
void
diffusion_cuda(
    cudaStream_t stream,
    TYPE * src, int ld_src,
    TYPE * dst, int ld_dst,
    int tile_x, int tile_y
) {
    // how many threads we need in total
    dim3 T = {TS, TS, 1};

    // block dim
    dim3 B(DTS, DTS, 1);

    // grid
    dim3 G((T.x + B.x - 1) / B.x,  (T.y + B.y - 1) / B.y, 1);

    // kernel launch
    diffusion_cuda_kernel<<<G, B, 0, stream>>>(src, ld_src, dst, ld_dst, tile_x, tile_y);
}
