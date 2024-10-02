#ifndef __XKBLAS_H__
# define __XKBLAS_H__

# include <stddef.h>

/**
 *  XKBLAS LEGACY INTERFACES - implemented on top of xkblas/v2
 */
extern "C" {

    /* initialize the runtime (must be called by the main thread) */
    void xkblas_init(void);

    /* Wait for the completion of every threads */
    void xkblas_sync(void);

    /* deinitialize the runtime (must be called by the main thread) */
    void xkblas_deinit(void);

    /* Synchronize host memory with devices memory on the passed address space.
     * Restriction: concurrent 'xkblas_memory_coherent_async' on overlaping address spaces has an undefined behavior */
    void xkblas_memory_coherent_async(
        int uplo, int memflag,
        int m, int n,
        void * ptr, int ld,
        unsigned int sizeof_type
    );

    void * xkblas_malloc(size_t size);
    void xkblas_free(void * ptr, size_t size);
};

#endif /* __XKBLAS_H__ */
