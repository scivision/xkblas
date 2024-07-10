# include "conf/conf.h"
# include "logger/logger.h"
# include "device/devices.h"
# include "sync/spinlock.h"

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

static inline void
xkblas_register_format(void)
{
    XKBLAS_NOT_IMPLEMENTED();
    // TODO : what does this do ?
    // xkblas_register_task_format();
    // kaapi_register_format_writeback();
    // kaapi_register_format_invalidate();
    // kaapi_register_format_distribute();

    // TODO : and this ?
    // KAAPI_REGISTER_BASICTYPEFORMAT(kaapi_schar_format, signed char, "%hhi")
    // [...]
}

static inline void
__xkblas_init(void)
{
    xkblas_init_conf();
    xkblas_register_format();
    xkblas_devices_init();
}

extern "C" int
xkblas_init(void)
{
    XKBLAS_INFO("Initializing Xkblas");

    static std::atomic<int> initialized = 0;
    static spinlock_t spinlock = 0;

    if (initialized == 0)
    {
        SPINLOCK_LOCK(spinlock)
        {
            if (initialized == 0)
            {
                __xkblas_init();
                initialized = 1;
            }
        }
        SPINLOCK_UNLOCK(spinlock);
    }

    return 0;
}

extern "C" void
xkblas_deinit(void)
{
    XKBLAS_INFO("Deinitializing Xkblas");
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
