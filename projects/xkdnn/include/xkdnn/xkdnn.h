/* ************************************************************************** */
/*                                                                            */
/*   xkdnn.h                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:49 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/24 22:00:02 by Romain PEREIRA            \_)     (_/    */
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
    int xkdnn_init(void);

    /* return the number of devices available */
    int xkdnn_get_device_count(int * count);

    /* Wait for the completion of every threads */
    void xkdnn_sync(void);

    /* deinitialize the runtime (must be called by the main thread) */
    void xkdnn_deinit(void);

    /* Synchronize host memory with devices memory on the passed address space.
     * Restriction: concurrent 'xkdnn_memory_coherent_async' on overlaping address spaces has an undefined behavior */
    void xkdnn_memory_coherent_async(int uplo, int memflag, int m, int n, void * ptr, int ld, unsigned int sizeof_type);

    /* alloc unified memory */
    void * xkdnn_unified_alloc(size_t size);

    /* release unified memory */
    void xkdnn_unified_free(void * ptr, size_t size);

    /* alloc pinned memory */
    void * xkdnn_host_alloc(size_t size);

    /* release pinned memory */
    void xkdnn_host_free(void * ptr, size_t size);

    typedef enum    xkdnn_mode_math_t
    {
        XKBLAS_DEFAULT_MATH,
        XKBLAS_TENSOR_OP_MATH

    }               xkdnn_mode_math_t;

    /* set the mode math for the next kernel */
    void xkdnn_set_modemath(xkdnn_mode_math_t mode);

    /* ??? */
    uint64_t xkdnn_register_memory_async(void * ptr, uint64_t sz);
    int xkdnn_unregister_memory(void * ptr, uint64_t sz);
    int xkdnn_register_memory_waitall(void);

    int xkdnn_get_ngpus(void);

    //////////////////////////////////
    // DEPRECATED LEGACY INTERFACES //
    //////////////////////////////////

    [[deprecated("Use `xkdnn_host_alloc(size_t)` instead")]]
        void * xkdnn_malloc(size_t size);

    [[deprecated("Use `xkdnn_host_free(size_t)` instead")]]
        void xkdnn_free(void * ptr, size_t size);

    /* NB = tile size. 0 == value computed at runtime */
    [[deprecated("No replacement available yet for setting conf parameters")]]
        void xkdnn_set_param(size_t nb, size_t p);

    /* <=> xkdnn_deinit - call to release runtime after an xkdnn_init call */
    [[deprecated("Use `xkdnn_deinit(void)` instead")]]
        void xkdnn_finalize(void);

    /* invalidate device memory */
    [[deprecated("No replacement available yet (`caches` name may not be the most appropriate here)")]]
        void xkdnn_memory_invalidate_caches(void);

# ifdef __cplusplus
}; /* extern "C" */
# endif /* __cplusplus */

#endif /* __XKBLAS_H__ */
