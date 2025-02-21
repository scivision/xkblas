/* ************************************************************************** */
/*                                                                            */
/*   consts.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 00:34:17 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/21 01:10:41 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# ifndef __HEAT_CONSTS_H__
#  define __HEAT_CONSTS_H__

/////////////////////////////
// CONSTS (you can modify) //
/////////////////////////////

/* Number of points per dimension in the grid */
#  define NX 1024
#  define NY 1024

/* Tile size for distributing across gpus */
#  define TS  256

/* Size of a cell (m) */
#  define DX (1e-2)
#  define DY (DX)

/* Thermal diffusivity (copper) */
#  define ALPHA (117 * 1e-6)

/* Boundary conditions (°C) */
#  define TEMPERATURE_BOUNDARY 100
#  define TEMPERATURE_INITIAL    0

/* Number of timesteps */
#  define N_STEP 1000

/* Duration of the simulation (in s.) */
#  define DURATION 10

/* Number of vtk images to generate */
#  define N_VTK 100

//////////////////////////////////////////
//  INFERED FROM CONSTS (do not modify) //
//////////////////////////////////////////

#  define DT (DURATION / (double) N_STEP)

static_assert(NX % TS == 0);
static_assert(NY % TS == 0);

# endif /* __HEAT_CONSTS_H__ */
