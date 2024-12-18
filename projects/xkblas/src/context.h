/* ************************************************************************** */
/*                                                                            */
/*   context.h                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/18 15:31:10 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CONTEXT_H__
# define __CONTEXT_H__

# include <ptr/sync/spinlock.h>

# include <atomic>
# include <stdint.h>

typedef enum    xkblas_context_state_t : uint8_t
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

}               xkblas_context_t;

// TODO : currently using a global variable to preserve previous 'xkblas_init'
// and 'xkblas_deinit' interfaces that takes no arguments.  Instead, we should
// have them taking an 'xkblas_context_t' argument that the user must keep
// track of
extern "C"
xkblas_context_t * xkblas_context_get(void);

# if 0
/* memory async thread management */
void xkblas_memory_coherent_async_worker_thread_init(xkblas_context_t * context);
void xkblas_memory_coherent_async_register_format(void);
# endif

#endif /* __CONTEXT_H__ */
