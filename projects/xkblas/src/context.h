/* ************************************************************************** */
/*                                                                            */
/*   context.h                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/15 17:01:38 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:23:09 by Romain PEREIRA         / _______ \       */
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

#ifndef __CONTEXT_H__
# define __CONTEXT_H__

# include "conf.h"

# include <xkrt/xkrt.h>
# include <xkrt/sync/spinlock.h>

# include <atomic>
# include <stdint.h>

typedef enum    xkblas_context_state_t : uint8_t
{
    XKBLAS_CONTEXT_DEINITIALIZED = 0,
    XKBLAS_CONTEXT_INITIALIZED,
}               xkblas_context_state_t;

typedef struct  xkblas_context_t
{
    xkrt_runtime_t runtime;
    struct {
        spinlock_t spinlock;
        volatile std::atomic<xkblas_context_state_t> current;
    } state;

    xkblas_conf_t conf;

}               xkblas_context_t;

// TODO : currently using a global variable to preserve previous 'xkblas_init'
// and 'xkblas_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'xkblas_context_t' argument that the user must keep
// track of
extern "C"
xkblas_context_t * xkblas_context_get(void);

extern "C"
xkrt_runtime_t * xkblas_xkrt_runtime_get(void);

# if 0
/* memory async thread management */
void xkblas_memory_coherent_async_worker_thread_init(xkblas_context_t * context);
void xkblas_memory_coherent_async_register_format(void);
# endif

task_format_id_t xkblas_task_format_create(task_format_t * format);

#endif /* __CONTEXT_H__ */
