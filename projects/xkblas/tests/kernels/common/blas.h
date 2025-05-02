/* ************************************************************************** */
/*                                                                            */
/*   blas.h                                                                   */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __BLAS_H__
# define __BLAS_H__

#if defined(USE_OPENBLAS) || defined(USE_CRAYBLAS)
#  include <cblas.h>
#elif defined(USE_MKL)
#  include <mkl.h>
#  include <mkl_types.h>
#  include <mkl_cblas.h>
#  include <mkl_lapacke.h>
#elif defined(USE_NVHPC)
#  include <nvblas.h>
#else
#  pragma message("No CPU blas libraries")
#  include <xkblas/cblas.h>
#endif

#endif /* __BLAS_H__ */
