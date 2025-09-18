/* ************************************************************************** */
/*                                                                            */
/*   xkblas.h                                                     .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/09 11:22:22 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/18 02:41:34 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __XKBLAS_H__
# define __XKBLAS_H__

# include <stddef.h>
# include <stdint.h>

/* XKBLAS LEGACY C INTERFACES */

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

    /* make memory coherent on some physical memory */
    void xkblas_memory_segment_coherent_async(void * ptr, size_t size);

    void xkblas_memory_matrix_coherent_async(
        void * ptr, size_t ld,
        size_t m, size_t n,
        size_t sizeof_type
    );

    /* run a host function in a task */
    void xkblas_host_async(void (*func)(void *), void * args);

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

    int xkblas_register_memory(void * ptr, uint64_t sz);
    int xkblas_unregister_memory(void * ptr, uint64_t sz);

    /* create 'n' tasks that (un)register the memory block that write virtually to
     * the memory segments */
    int xkblas_memory_register_tiled_async(void * ptr, size_t sz, int n);
    int xkblas_memory_unregister_tiled_async(void * ptr, size_t sz, int n);
    int xkblas_memory_touch_tiled_async(void * ptr, size_t sz, int n);

    /* get number of gpus */
    int xkblas_get_ngpus(void);

    /* get time in nanoseconds */
    uint64_t xkblas_get_nanotime(void);

    //////////////////////////////////
    // //DEPRECATED LEGACY INTERFACES //
    //////////////////////////////////

    //DEPRECATED("Use `xkblas_host_alloc(size_t)` instead")
        void * xkblas_malloc(size_t size);

    //DEPRECATED("Use `xkblas_host_free(size_t)` instead")
        void xkblas_free(void * ptr, size_t size);

    /* NB = tile size. 0 == value computed at runtime */
    //DEPRECATED("No replacement available yet for setting conf parameters")
        void xkblas_set_param(size_t nb, size_t p);

    /* <=> xkblas_deinit - call to release runtime after an xkblas_init call */
    //DEPRECATED("Use `xkblas_deinit(void)` instead")
        void xkblas_finalize(void);

    /* invalidate device memory */
    //DEPRECATED("No replacement available yet (`caches` name may not be the most appropriate here)")
        void xkblas_memory_invalidate_caches(void);

    /* spawn an independent tasks that register the passed memory */
    //DEPRECATED("Use `xkblas_register_memory_tiled_async` instead")
        uint64_t xkblas_register_memory_async(void * ptr, uint64_t sz);

    /* spawn an independent tasks that unregister the passed memory */
    //DEPRECATED("Use `xkblas_unregister_memory_tiled_async` instead")
        int xkblas_unregister_memory_async(void * ptr, uint64_t sz);

    /* see xkblas_sync() */
    //DEPRECATED("Use `xkblas_sync` instead")
        int xkblas_register_memory_waitall(void);

    #if defined(__STDC_NO_COMPLEX__)
    # error "Compiler support for complex number is required."
    #else
    # include <complex.h>
    typedef float _Complex Complex32_t;
    typedef double _Complex Complex64_t;
    typedef double CFloat64_t;
    #endif

    #include <xkblas/skernels.h>
    #include <xkblas/dkernels.h>
    #include <xkblas/ckernels.h>
    #include <xkblas/zkernels.h>

    #include <xkblas/cblas.h>

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
