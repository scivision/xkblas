/* ************************************************************************** */
/*                                                                            */
/*   xkrt-support.h                                                          */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/18 15:05:11 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 21:50:13 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_SUPPORT_H__
# define __XKRT_SUPPORT_H__

# if USE_HOST
#  define XKRT_SUPPORT_HOST
# endif /* USE_HOST */

# if USE_CUDA
#  define XKRT_SUPPORT_CUDA
# endif /* USE_CUDA */

# if USE_HIP
#  define XKRT_SUPPORT_HIP
# endif /* USE_HIP */


# if USE_ZE
#  define XKRT_SUPPORT_ZE
# endif /* USE_ZE */

#endif /* __XKRT_SUPPORT_H__ */
