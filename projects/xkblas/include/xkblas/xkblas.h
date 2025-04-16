/* ************************************************************************** */
/*                                                                            */
/*   xkblas.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:49 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/04/11 16:57:21 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKBLAS_H__
# define __XKBLAS_H__

# include <stddef.h>
# include <stdint.h>

/* XKBLAS LEGACY INTERFACES */

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

    /* initialize the runtime (must be called by the main thread) */
    int xkblas_init(void);

    /* return the number of devices available */
    int xkblas_get_device_count(int * count);

    /* Wait for the completion of every threads */
    void xkblas_sync(void);

    /* deinitialize the runtime (must be called by the main thread) */
    void xkblas_deinit(void);

    /* Synchronize host memory with devices memory on the passed address space.
     * Restriction: concurrent 'xkblas_memory_coherent_async' on overlaping address spaces has an undefined behavior */
    void xkblas_memory_coherent_async(int uplo, int memflag, int m, int n, void * ptr, int ld, unsigned int sizeof_type);

    /* create one task per device to replicate fully the passed matrix */
    void xkblas_replicate_async(void * ptr, int ld, int m, int n, unsigned int sizeof_type);

    /* alloc unified memory */
    void * xkblas_unified_alloc(size_t size);

    /* release unified memory */
    void xkblas_unified_free(void * ptr, size_t size);

    /* alloc pinned memory */
    void * xkblas_host_alloc(size_t size);

    /* release pinned memory */
    void xkblas_host_free(void * ptr, size_t size);

    typedef enum    xkblas_mode_math_t
    {
        XKBLAS_DEFAULT_MATH,
        XKBLAS_TENSOR_OP_MATH

    }               xkblas_mode_math_t;

    /* set the mode math for the next kernel */
    void xkblas_set_modemath(xkblas_mode_math_t mode);

    /* ??? */
    uint64_t xkblas_register_memory_async(void * ptr, uint64_t sz);
    int xkblas_unregister_memory(void * ptr, uint64_t sz);
    int xkblas_register_memory_waitall(void);

    int xkblas_get_ngpus(void);

    //////////////////////////////////
    // DEPRECATED LEGACY INTERFACES //
    //////////////////////////////////

    [[deprecated("Use `xkblas_host_alloc(size_t)` instead")]]
        void * xkblas_malloc(size_t size);

    [[deprecated("Use `xkblas_host_free(size_t)` instead")]]
        void xkblas_free(void * ptr, size_t size);

    /* NB = tile size. 0 == value computed at runtime */
    [[deprecated("No replacement available yet for setting conf parameters")]]
        void xkblas_set_param(size_t nb, size_t p);

    /* <=> xkblas_deinit - call to release runtime after an xkblas_init call */
    [[deprecated("Use `xkblas_deinit(void)` instead")]]
        void xkblas_finalize(void);

    /* invalidate device memory */
    [[deprecated("No replacement available yet (`caches` name may not be the most appropriate here)")]]
        void xkblas_memory_invalidate_caches(void);

#if defined(__STDC_NO_COMPLEX__)
# error "Compiler support for complex number is required."
#else
# include <complex.h>
typedef float _Complex Complex32_t;
typedef double _Complex Complex64_t;
typedef double CFloat64_t;
#endif
#include "kernels.h"
#include "cblas.h"
static inline int xkblas_blas2cblas_trans( const char* trans )
{
  switch (trans[0]) {
    case 'n':
    case 'N': return CblasNoTrans;
    case 't':
    case 'T': return CblasTrans;
    case 'c':
    case 'C': return CblasConjTrans;
    default:
      return -1;
  }
}

static inline int xkblas_blas2cblas_side( const char* side )
{
  switch (side[0]) {
    case 'l':
    case 'L': return CblasLeft;
    case 'r':
    case 'R': return CblasRight;
    default:
      return -1;
  }
}

static inline int xkblas_blas2cblas_fill( const char* uplo )
{
  switch (uplo[0]) {
    case 'l':
    case 'L': return CblasLower;
    case 'u':
    case 'U': return CblasUpper;
    default:
      return -1;
  }
}

static inline int xkblas_blas2cblas_diag( const char* diag )
{
  switch (diag[0]) {
    case 'n':
    case 'N': return CblasNonUnit;
    case 'u':
    case 'U': return CblasUnit;
    default:
      return -1;
  }
}
# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */

#endif /* __XKBLAS_H__ */
