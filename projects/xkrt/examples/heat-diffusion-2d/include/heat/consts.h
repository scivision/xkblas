/* ************************************************************************** */
/*                                                                            */
/*   consts.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/21 00:34:17 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/17 16:20:28 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: ???                                                             */
/*                                                                            */
/* ************************************************************************** */

# ifndef __HEAT_CONSTS_H__
#  define __HEAT_CONSTS_H__

////////////////////
// YOU CAN CHANGE //
////////////////////

/* type to use for the temperature */
#  define TYPE float

/* Number of timesteps */
#  define N_STEP 200

/* Number of vtk images to generate */
#  define N_VTK MIN(10, N_STEP)

/* Thermal diffusivity */
#  define ALPHA (1.11e-4f)

/* Boundary conditions (°C) */
#  define TEMPERATURE_BOUNDARY 100

//////////////////////////////////////////////////
// YOU CAN CHANGE BUT BE CAUTIOUS FOR STABILITY //
//////////////////////////////////////////////////

// TIME STEP FORMULAE IS
//
//  D(i,j) = S(i,j) + ALPHA * DT / (DX * DY) * (
//          (S(i+1,  j) - 2 * S(i,j) + S(i-1,  j)) / (DX * DX) +
//          (S(  i,j+1) - 2 * S(i,j) + S(  i,j-1)) / (DY * DY)
//      );

/* Number of points per dimension in the grid */
//#  define NX (16384)
//#  define NX (32768+8192)
#  define NX (16)
#  define NY NX

/* Size of a cell (m) */
#  define DX (1.0f)
#  define DY (DX)

/* Duration of the simulation (in s.) */
#  define DT (0.5f*(DX*DX * DY*DY) / (2.0f*ALPHA*(DX*DX + DY*DY)))

//////////////////////////////////////////
//  INFERED FROM CONSTS (do not modify) //
//////////////////////////////////////////

#  define DURATION (DT * (TYPE) N_STEP)

# define GRID(G, I, J, L) (G[(J)*L+(I)])
# define LD NX

# endif /* __HEAT_CONSTS_H__ */
