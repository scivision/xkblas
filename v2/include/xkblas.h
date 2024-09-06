#ifndef __XKBLAS_H__
# define __XKBLAS_H__

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

    /* Fetch memory from device to host with respect to previous device
     * accesses - the address space passed must be included in a previous xkblas call */
    void xkblas_memory_coherent_async(
        int uplo, int memflag,
        int m, int n,
        void * ptr, int ld,
        unsigned int sizeof_type
    );

};

#endif /* __XKBLAS_H__ */
