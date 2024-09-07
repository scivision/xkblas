#ifndef __XKBLAS_CONTEXT_H__
# define __XKBLAS_CONTEXT_H__

# include "conf/conf.h"
# include "device/driver.h"
# include "device/memory-tree.hpp"
# include "sync/spinlock.h"

typedef enum    xkblas_context_state_t
{
    XKBLAS_CONTEXT_DEINITIALIZED = 0,
    XKBLAS_CONTEXT_INITIALIZED,
}               xkblas_context_state_t;

typedef struct  xkblas_context_t
{
    /* context state */
    struct {
        spinlock_t spinlock;
        volatile std::atomic<xkblas_context_state_t> current;
    } state;

    /* user conf */
    xkblas_conf_t conf;

    /* worker thread to copy data asynchronously */
    ThreadWorker * memory_coherent_worker_thread;

    /* driver list */
    xkblas_drivers_t drivers;

    /* memory state on each device */
    MemoryTree memtree;

}               xkblas_context_t;

// TODO : currently using a global variable to preserve previous 'xkblas_init'
// and 'xkblas_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'xkblas_context_t' argument that the user must keep
// track of
xkblas_context_t * xkblas_context_get(void);

/* submit a ready task */
void xkblas_context_submit_task(xkblas_context_t * context, Task * task);

/* memory async thread management */
void xkblas_memory_coherent_async_worker_thread_init(xkblas_context_t * context);
void xkblas_memory_coherent_async_impl(
    int uplo, int memflag,
    int m, int n,
    void * ptr, int ld,
    unsigned int sizeof_type
);

#endif /* __XKBLAS_CONTEXT_H__ */
