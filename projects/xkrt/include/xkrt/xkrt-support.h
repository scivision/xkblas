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

/* If the runtime was compiled with HOST kernel execution support */
# define XKRT_SUPPORT_HOST  0

/* If the runtime was compiled with Mvidia's CUDA support */
# define XKRT_SUPPORT_CUDA  1

/* If the runtime was compiled with Intel's Level Zero support */
# define XKRT_SUPPORT_ZE    0

/* If the kernel was compiled with OpenCL support */
# define XKRT_SUPPORT_CL 0

/* If the kernel was compiled with run-time statistics enabled */
# define XKRT_SUPPORT_STATS 1

#endif /* __XKRT_SUPPORT_H__ */
