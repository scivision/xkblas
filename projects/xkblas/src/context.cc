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
        }
    };

    return &context;
}

void xkblas_task_format_register(void);

extern "C"
int
xkblas_init(void)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    if (context->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
    {
        SPINLOCK_LOCK(context->state.spinlock);
        {
            if (context->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
            {
                xkrt_init(&(context->runtime));
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
                xkrt_deinit(&(context->runtime));
            }
        }
        SPINLOCK_UNLOCK(context->state.spinlock);
    }
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

/////////////////////////////////////////////
// Dirty stuff so it compiles without changing the rest of the world //
/////////////////////////////////////////////
task_format_id_t
xkblas_task_format_create(task_format_t * format)
{
    xkblas_context_t * context = xkblas_context_get();
    assert(context);
    return task_format_create(&(context->runtime.task_formats), format);
}
