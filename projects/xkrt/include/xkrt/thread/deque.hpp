/* ************************************************************************** */
/*                                                                            */
/*   deque.hpp                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/19 19:23:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/05/02 18:36:54 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# ifndef __XKRT_DEQUE_H__
#  define __XKRT_DEQUE_H__

#  include <xkrt/consts.h>
#  include <xkrt/memory/alignas.h>
#  include <xkrt/sync/spinlock.h>

#  include <pthread.h>
#  include <atomic>
#  include <new>

/* a deque (THE protocol) */
template<typename T, int C>
struct xkrt_deque_t
{
    T tasks[C];
    alignas(hardware_destructive_interference_size) spinlock_t lock;
    alignas(hardware_destructive_interference_size) std::atomic<int> _h;
    alignas(hardware_destructive_interference_size) std::atomic<int> _t;

    xkrt_deque_t() : tasks{}, lock(0), _h(0), _t(0) {}

    void push(T const & task);
    T pop(void);
    T steal(void);
};

# endif /* __XKRT_DEQUE_H__ */
