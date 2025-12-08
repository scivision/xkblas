/*
** Copyright 2024,2025 INRIA
**
** Contributors :
** Thierry Gautier, thierry.gautier@inrialpes.fr
** Romain PEREIRA, romain.pereira@inria.fr + rpereira@anl.gov
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/

#ifndef __XKBLAS_H__
# define __XKBLAS_H__

# include <stddef.h>
# include <stdint.h>

# include <xkrt/xkrt.h>

/* XKBLAS LEGACY C INTERFACES */

# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

    /* return the xkrt runtime */
    xkrt_runtime_t * xkblas_xkrt_runtime_get(void);

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

    /* generic taks spawning routines */
    void xkblas_async(
        xkrt_device_global_id_t device_global_id,
        void (*func)(void *),
        void * args
    );

    void xkblas_async_with_accesses(
        xkrt_device_global_id_t device_global_id,
        void (*func)(void *),
        void * args,
        const xkrt_access_t * accesses,
        const int naccesses
    );

    void xkblas_async_with_format(
        const xkrt_device_global_id_t device_global_id,
        const xkrt_task_format_id_t fmtid,
        const void * args,
        const size_t args_size
    );

    void xkblas_async_with_format_with_accesses(
        const xkrt_device_global_id_t device_global_id,
        const xkrt_task_format_id_t fmtid,
        const void * args,
        const size_t args_size,
        const xkrt_access_t * accesses,
        const int naccesses
    );

    /* Julia versions, that require special flags */
    void xkblas_detachable_async(
        xkrt_device_global_id_t device_global_id,
        void (*func)(void *),
        void * args
    );

    void xkblas_detachable_async_with_accesses(
        xkrt_device_global_id_t device_global_id,
        void (*func)(void *),
        void * args,
        const xkrt_access_t * accesses,
        const int naccesses
    );

    void xkblas_detachable_async_with_format(
        const xkrt_device_global_id_t device_global_id,
        const xkrt_task_format_id_t fmtid,
        const void * args,
        const size_t args_size
    );

    void xkblas_detachable_async_with_format_with_accesses(
        const xkrt_device_global_id_t device_global_id,
        const xkrt_task_format_id_t fmtid,
        const void * args,
        const size_t args_size,
        const xkrt_access_t * accesses,
        const int naccesses
    );

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

    /* synchronous memory registration */
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

    /* task formats */
    xkrt_task_format_id_t xkblas_task_format_put(const char * label);

    int xkblas_task_format_set(
        xkrt_task_format_id_t fmtid,
        xkrt_task_format_target_t target,
        xkrt_task_format_func_t func
    );

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

    /* see xkblas_sync() */
    //DEPRECATED("Use `xkblas_memory_natrix_coherent_async` instead")
        int xkblas_memory_coherent_async(
            int uplo, int memflag,
            size_t M, size_t N,
            void* A, size_t LD, size_t eltsize
        );

    #if defined(__STDC_NO_COMPLEX__)
    # error "Compiler support for complex number is required."
    #else
    # include <complex.h>
    typedef float _Complex Complex32_t;
    typedef double _Complex Complex64_t;
    typedef double CFloat64_t;
    #endif

    #include <xkblas/sroutines.h>
    #include <xkblas/droutines.h>
    #include <xkblas/croutines.h>
    #include <xkblas/zroutines.h>

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
