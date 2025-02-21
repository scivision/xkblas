/* ************************************************************************** */
/*                                                                            */
/*   consts.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 00:34:17 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/02/21 20:07:41 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# ifndef __HEAT_CONSTS_H__
#  define __HEAT_CONSTS_H__

/////////////////////////////
// CONSTS (you can modify) //
/////////////////////////////

/* type to use for the temperature */
#  define TYPE double

/* Number of points per dimension in the grid */
#  define NX 16
#  define NY NX

/* Tile size for distributing across gpus */
#  define TS (8)

/* Size of a cell (m) */
#  define DX (1.0)
#  define DY (DX)

/* Thermal diffusivity */
#  define ALPHA (0.5)

/* Boundary conditions (°C) */
#  define TEMPERATURE_BOUNDARY 100
#  define TEMPERATURE_INITIAL    0

/* Number of timesteps */
#  define N_STEP 5

/* Duration of the simulation (in s.) */
#  define DT 0.1

/* Number of vtk images to generate */
#  define N_VTK MIN(10, N_STEP) // N_STEP

//////////////////////////////////////////
//  INFERED FROM CONSTS (do not modify) //
//////////////////////////////////////////

#  define DURATION (DT * (double) N_STEP)

static_assert(NX % TS == 0);
static_assert(NY % TS == 0);

# define GRID(G, I, J, L) (G[(J)*L+(I)])
# define LD NX

# endif /* __HEAT_CONSTS_H__ */
