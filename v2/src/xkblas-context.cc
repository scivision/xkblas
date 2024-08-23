# include "xkblas-context.h"

# include "conf/conf.h"
# include "kernels/kernel-task-format-register.h"
# include "logger/logger.h"
# include "device/driver.h"
# include "device/thread-producer.hpp"
# include "sync/spinlock.h"

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

// singleton of runtime context
xkblas_context_t *
xkblas_context_get(void)
{
    # pragma message(TODO "Optimize this default conf")

    static xkblas_context_t ctx = {
        .state = {
            .spinlock = 0,
            .current = XKBLAS_CONTEXT_DEINITIALIZED,
        },
        .conf = {},
        .drivers = {},
    };

    return &ctx;
}

static inline void
xkblas_task_format_register(void)
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

    # include "kernels/kernel-task-format-register.cc"
}

extern "C" void
xkblas_init(void)
{
    XKBLAS_INFO("Initializing Xkblas");

    xkblas_context_t * ctx = xkblas_context_get();
    if (ctx->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
    {
        SPINLOCK_LOCK(ctx->state.spinlock);
        {
            if (ctx->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
            {
                // load
                xkblas_init_conf(&(ctx->conf));
                xkblas_task_format_register();
                xkblas_drivers_init(&(ctx->drivers), ctx->conf.ngpus);
                ctx->state.current = XKBLAS_CONTEXT_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(ctx->state.spinlock);
    }
}

extern "C" void
xkblas_deinit(void)
{
    XKBLAS_INFO("Deinitializing Xkblas");

    xkblas_context_t * ctx = xkblas_context_get();
    if (ctx->state.current == XKBLAS_CONTEXT_INITIALIZED)
    {
        SPINLOCK_LOCK(ctx->state.spinlock);
        {
            if (ctx->state.current == XKBLAS_CONTEXT_INITIALIZED)
            {
                xkblas_drivers_deinit(&ctx->drivers);
            }
        }
        SPINLOCK_UNLOCK(ctx->state.spinlock);
    }
}

extern "C" void
xkblas_thread_init(void)
{
    ThreadProducer::init();
}

extern "C" void
xkblas_thread_deinit(void)
{
    ThreadProducer::deinit();
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
void
xkblas_sync(void)
{
    XKBLAS_INFO("Synchronizing Xkblas");
    sleep(1);

    XKBLAS_INFO("Exporting memory tree...");
    xkblas_context_t * ctx = xkblas_context_get();
    ctx->memtree.export_pdf("memory");
    XKBLAS_INFO("Done");
    exit(0);

    XKBLAS_INFO("Infinite loop... CTRL+C to exit");
    while (1)
        sleep(1);
}

extern "C"
void
xkblas_thread_sync(void)
{
    XKBLAS_INFO("Synchronizing Xkblas thread");
}
