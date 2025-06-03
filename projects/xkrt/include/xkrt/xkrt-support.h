/* ************************************************************************** */
/*                                                                            */
/*   xkrt-support.h                                               .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/08/06 13:12:59 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:08:08 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKRT_SUPPORT_H__
# define __XKRT_SUPPORT_H__

/* If the runtime was compiled with Mvidia's CUDA support */
# define XKRT_SUPPORT_CUDA  1

/* If the runtime was compiled with Intel's Level Zero support */
# define XKRT_SUPPORT_ZE    0

/* If the runtime was compiled with SYCL support (for Level Zero interop and mkl) */
# define XKRT_SUPPORT_SYCL  0

/* If the kernel was compiled with OpenCL support */
# define XKRT_SUPPORT_CL 0

/* If the kernel was compiled with run-time statistics enabled */
# define XKRT_SUPPORT_STATS 1

/* If runtime was compiled with cairo support */
# define XKRT_SUPPORT_CAIRO 0

#endif /* __XKRT_SUPPORT_H__ */
