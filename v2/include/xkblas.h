#ifndef __XKBLAS_H__
# define __XKBLAS_H__

extern "C" {

    /* initialize the runtime (must be called by the main thread) */
    void xkblas_init(void);

    /* Wait for the completion of every threads */
    void xkblas_sync(void);

    /* deinitialize the runtime (must be called by the main thread) */
    void xkblas_deinit(void);

    /* initialize the thread (must be called by any threads) */
    void xkblas_thread_deinit(void);
};

#endif /* __XKBLAS_H__ */
