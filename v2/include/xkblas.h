#ifndef __XKBLAS_H__
# define __XKBLAS_H__

extern "C" {

    /* initialize the runtime */
    void xkblas_init(void);

    /* Wait for the completion of asynchronous operation spawned on the calling threads */
    void xkblas_sync(void);

    /* deinitialize the runtime */
    void xkblas_deinit(void);


};

#endif /* __XKBLAS_H__ */
