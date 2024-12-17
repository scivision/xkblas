/* ************************************************************************** */
/*                                                                            */
/*   xkblas-kernel-type.h                                                     */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:49 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:49 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKBLAS_KERNEL_TYPE_H__
# define __XKBLAS_KERNEL_TYPE_H__

typedef enum    xkblas_kernel_type_t
{
    XKBLAS_KERNEL_TYPE_COPYSCALE,
    XKBLAS_KERNEL_TYPE_GEMM,
    XKBLAS_KERNEL_TYPE_GEMMT,
    XKBLAS_KERNEL_TYPE_SYMM,
    XKBLAS_KERNEL_TYPE_SYR2K,
    XKBLAS_KERNEL_TYPE_SYRK,
    XKBLAS_KERNEL_TYPE_TRMM,
    XKBLAS_KERNEL_TYPE_TRSM,
    XKBLAS_KERNEL_TYPE_MAX,

}               xkblas_kernel_type_t;

[[deprecated("Use `xkblas_kernel_type_t` instead")]]
typedef enum    xkblas_kernel_type_deprecated_t
{
    KERN_COPYSCALE  = XKBLAS_KERNEL_TYPE_COPYSCALE,
    KERN_GEMM       = XKBLAS_KERNEL_TYPE_GEMM,
    KERN_GEMMT      = XKBLAS_KERNEL_TYPE_GEMMT,
    KERN_SYMM       = XKBLAS_KERNEL_TYPE_SYMM,
    KERN_SYR2K      = XKBLAS_KERNEL_TYPE_SYR2K,
    KERN_SYRK       = XKBLAS_KERNEL_TYPE_SYRK,
    KERN_TRMM       = XKBLAS_KERNEL_TYPE_TRMM,
    KERN_TRSM       = XKBLAS_KERNEL_TYPE_TRSM,
    KERN_MAX        = XKBLAS_KERNEL_TYPE_MAX

}               xkblas_kernel_type_deprecated_t;

typedef xkblas_kernel_type_deprecated_t xkblas_kernel_t;

#endif /* __XKBLAS_KERNEL_TYPE_H__ */
