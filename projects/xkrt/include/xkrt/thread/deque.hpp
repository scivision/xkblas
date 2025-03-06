/* ************************************************************************** */
/*                                                                            */
/*   deque.hpp                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <rpereira@anl.gov>                     .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2025/02/19 19:23:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/06 15:03:26 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL 2.1                                                      */
/*                                                                            */
/* ************************************************************************** */

# ifndef __XKRT_DEQUE_H__
#  define __XKRT_DEQUE_H__

#  include <xkrt/consts.h>
#  include <xkrt/sync/spinlock.h>

#  include <pthread.h>
#  include <atomic>
#  include <new>

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr std::size_t hardware_constructive_interference_size = 64;
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

/* a deque (THE protocol) */
template<typename T, int C>
struct xkrt_deque_t
{
    T tasks[C];
    alignas(hardware_destructive_interference_size) spinlock_t lock;
    alignas(hardware_destructive_interference_size) std::atomic<int> _h;
    alignas(hardware_destructive_interference_size) std::atomic<int> _t;

    xkrt_deque_t() : tasks{}, lock(0), _h(0), _t(0) {}

    void push(T & task);
    T pop(void);
    T steal(void);
};

# endif /* __XKRT_DEQUE_H__ */
