/* ************************************************************************** */
/*                                                                            */
/*   xkblas.cc                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/15 17:01:38 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/09/02 19:52:01 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Pierre-Etienne POLET <pierre-etienne.polet@inria.fr>             */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

# include <atomic>
# include <stdlib.h>
# include <string.h>

# include <xkblas/xkblas.hpp>

# include <xkrt/runtime.h>
# include <xkrt/sync/spinlock.h>

XKRT_NAMESPACE_USE;

////////////////////////////
// Methods implementation //
////////////////////////////

void
xkblas_t::init(void)
{
    if (this->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
    {
        SPINLOCK_LOCK(this->state.spinlock);
        {
            if (this->state.current == XKBLAS_CONTEXT_DEINITIALIZED)
            {
                // init xkrt
                this->runtime.init();

                // create task formats
                # define CREATE(P, K)                                                                   \
                    do {                                                                                \
                        task_format_t format;                                                           \
                        memset(&format, 0, sizeof(task_format_t));                                      \
                        this->task_format_create_##K<P>(&format);                                       \
                        snprintf(format.label, sizeof(format.label), #P#K);                             \
                        this->formats.K[P] = task_format_create(&this->runtime.formats.list, &format);  \
                    } while (0);
                memset(&this->formats, 0, sizeof(this->formats));
                XKBLAS_FORALL_PRECISIONS_AND_KERNELS(CREATE);
                # undef CREATE

                // done
                this->state.current = XKBLAS_CONTEXT_INITIALIZED;
            }
        }
        SPINLOCK_UNLOCK(this->state.spinlock);
    }
}

void
xkblas_t::deinit(void)
{
    if (this->state.current == XKBLAS_CONTEXT_INITIALIZED)
    {
        SPINLOCK_LOCK(this->state.spinlock);
        {
            if (this->state.current == XKBLAS_CONTEXT_INITIALIZED)
            {
                this->state.current = XKBLAS_CONTEXT_DEINITIALIZED;
                this->runtime.deinit();
            }
        }
        SPINLOCK_UNLOCK(this->state.spinlock);
    }
}

//////////////////////////////
//  Runtime initialization  //
//////////////////////////////

static xkblas_t context;

// singleton of runtime context
xkblas_t *
xkblas_get(void)
{
    return &context;
}

runtime_t *
xkblas_xkrt_runtime_get(void)
{
    xkblas_t * context = xkblas_get();
    return &(context->runtime);
}

extern "C"
int
xkblas_init(void)
{
    xkblas_t * context = xkblas_get();
    assert(context);

    context->init();
    return 0;
}

extern "C"
void
xkblas_deinit(void)
{
    xkblas_t * context = xkblas_get();
    assert(context);

    context->deinit();
}

extern "C"
uint64_t
xkblas_get_nanotime(void)
{
    return get_nanotime();
}

//////////////////////////////
//  Runtime synchronize     //
//////////////////////////////

extern "C"
void
xkblas_sync(void)
{
    xkblas_t * context = xkblas_get();
    assert(context);

    context->runtime.task_wait();
}
