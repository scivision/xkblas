/* ************************************************************************** */
/*                                                                            */
/*   deque.hpp                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:52:46 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEQUE_HPP__
# define __DEQUE_HPP__

# include <xkrt/sync/spinlock.h>
# include <xkrt/device/iqueue.hpp>

# include <atomic>

// TODO : unused - there is no workstealing implemented

// THE protocol from 'The Implementation of the Cilk-5 Multithreaded Language'

template<typename OBJ, int C>
class Deque : IQueue<OBJ>
{
    public:

        Deque() : objects(), lock(0), T(0), H(0) {}
        virtual ~Deque() {}

        void
        push(OBJ & obj)
        {
            ++T;
        }

        OBJ
        pop(void)
        {
            --T;
            if (H > T)
            {
                ++T;
                SPINLOCK_LOCK(lock);
                {
                    --T;
                    if (H > T)
                    {
                        ++T;
                        SPINLOCK_UNLOCK(lock);
                        return nullptr; // FAILURE
                    }
                }
                SPINLOCK_UNLOCK(lock);
            }
            return nullptr; // SUCCESS
        }

        OBJ
        steal(void)
        {
            SPINLOCK_LOCK(lock);
            {
                ++H;
                if (H > T)
                {
                    --H;
                    SPINLOCK_UNLOCK(lock);
                    return nullptr; // SUCCESS
                }
            }
            SPINLOCK_UNLOCK(lock);
            return nullptr; // FAILURE
        }

    private:

        OBJ objects[C];

        // TODO : what to alignas(std::hardware_constructive_interference_size) ?
        // TODO : replace all std::atomic access with explicit memory ordering
        spinlock_t lock;
        std::atomic<int> H;
        std::atomic<int> T;
};

#endif /* __DEQUE_HPP__ */
