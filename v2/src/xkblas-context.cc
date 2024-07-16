# include "xkblas-context.h"

# include "conf/conf.h"
# include "logger/logger.h"
# include "device/driver.h"
# include "scheduler/thread.hpp"
# include "sync/spinlock.h"

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

// singleton of runtime context
xkblas_context_t xkblas_context = {
    .state = {
        .spinlock = 0,
        .current = XKBLAS_CONTEXT_DEINITIALIZED,
    },
    .conf = {
        .stackblocsize              = (uint64_t)-1,
        .ngpus                      = (uint8_t)-1,
        .gpu_set                    = (uint32_t) ~0,
        .cuda_stream_capacity       = 64,
        .cuda_conc_stream_kernel    = 2,
        .cuda_conc_kernel           = 8,
        .cuda_conc_h2d              = 1,
        .cuda_conc_d2h              = 1,
        .cuda_conc_d2d              = 1,
        .cuda_cache_limit           = 0.98
    },
    .drivers = {0},
};

static inline void
xkblas_register_format(void)
{
    # pragma message(TODO "Register task format")
    // TODO : what does this do ?
    // xkblas_register_task_format();
    // kaapi_register_format_writeback();
    // kaapi_register_format_invalidate();
    // kaapi_register_format_distribute();

    // TODO : and this ?
    // KAAPI_REGISTER_BASICTYPEFORMAT(kaapi_schar_format, signed char, "%hhi")
    // [...]
}

extern "C" void
xkblas_init(void)
{
    XKBLAS_INFO("Initializing Xkblas");

    if (xkblas_context.state.current == XKBLAS_CONTEXT_DEINITIALIZED)
    {
        SPINLOCK_LOCK(xkblas_context.state.spinlock);
        {
            if (xkblas_context.state.current == XKBLAS_CONTEXT_DEINITIALIZED)
            {
                // load
                xkblas_init_conf(&(xkblas_context.conf));
                xkblas_drivers_init(&(xkblas_context.drivers), xkblas_context.conf.ngpus);
                xkblas_context.state.current = XKBLAS_CONTEXT_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(xkblas_context.state.spinlock);
    }
}

extern "C" void
xkblas_deinit(void)
{
    XKBLAS_INFO("Deinitializing Xkblas");

    if (xkblas_context.state.current == XKBLAS_CONTEXT_INITIALIZED)
    {
        SPINLOCK_LOCK(xkblas_context.state.spinlock);
        {
            if (xkblas_context.state.current == XKBLAS_CONTEXT_INITIALIZED)
            {
                xkblas_drivers_deinit(&xkblas_context.drivers);
            }
        }
        SPINLOCK_UNLOCK(xkblas_context.state.spinlock);
    }
}

extern "C" void
xkblas_thread_init(void)
{
    Thread::init();
}

extern "C" void
xkblas_thread_deinit(void)
{
    Thread::deinit();
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
void
xkblas_sync(void)
{
    XKBLAS_INFO("Synchronizing Xkblas");
}

extern "C"
void
xkblas_thread_sync(void)
{
    XKBLAS_INFO("Synchronizing Xkblas thread");
}
