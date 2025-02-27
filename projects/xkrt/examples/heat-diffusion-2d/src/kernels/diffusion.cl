/* ************************************************************************** */
/*                                                                            */
/*   diffusion.cl                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 04:45:52 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/27 18:30:36 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# include <heat/consts.h>

/* A naive kernel to update the grid */
__kernel
void
diffusion_cuda_kernel(__global TYPE * src, int ld_src, __global TYPE * dst, int ld_dst, int tile_x, int tile_y)
{
    const int li = get_global_id(0);
    const int lj = get_global_id(1);
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
