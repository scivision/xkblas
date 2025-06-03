/* ************************************************************************** */
/*                                                                            */
/*   deque.hpp                                                    .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2025/03/06 15:05:42 by Romain PEREIRA          __/_*_*(_        */
/*   Updated: 2025/06/03 18:06:50 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
