/* ************************************************************************** */
/*                                                                            */
/*   context.cc                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 19:23:49 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "context.h"

# if __has_include("kernels/generated/kernel-task-format-register.h")
#  include "kernels/generated/kernel-task-format-register.h"
# else
#  error "Please run 'python3 generate.py' in the 'kernels/' directory to generate source files"
# endif

# include <xkrt/xkrt.h>
# include <xkrt/sync/spinlock.h>

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
    static xkblas_context_t context = {
        .state = {
            .spinlock = 0,
            .current = { XKBLAS_CONTEXT_DEINITIALIZED }
        },
        .runtime
    };

    return &context;
}

static inline void
xkblas_task_format_register(void)
{
    # include "kernels/generated/kernel-task-format-register.cc"
}

extern "C"
int
xkblas_init(void)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    xkrt_init(&(context->runtime));

    if (context->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
    {
        SPINLOCK_LOCK(context->state.spinlock);
        {
            if (context->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
            {
                xkblas_task_format_register();
                context->state.current = XKBLAS_CONTEXT_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(context->state.spinlock);
    }
    return 0;
}

extern "C"
void
xkblas_deinit(void)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    if (context->state.current == XKBLAS_CONTEXT_INITIALIZED)
    {
        SPINLOCK_LOCK(context->state.spinlock);
        {
            if (context->state.current == XKBLAS_CONTEXT_INITIALIZED)
            {
                context->state.current = XKBLAS_CONTEXT_DEINITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(context->state.spinlock);
    }

    xkrt_deinit(&(context->runtime));
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
void
xkblas_sync(void)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    xkrt_sync(&(context->runtime));
}
