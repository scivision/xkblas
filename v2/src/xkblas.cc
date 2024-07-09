# include "logger.h"
# include "spinlock.h"
# include "xkblas_conf.h"

# include <atomic>
# include <stdlib.h>
# include <string.h>

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

static inline void
__xkblas_init(void)
{
    xkblas_init_conf();
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
