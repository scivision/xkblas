#ifndef __DEQUE_HPP__
# define __DEQUE_HPP__

# include "sync/spinlock.h"

# include <atomic>
# include <new>

// THE protocol from 'The Implementation of the Cilk-5 Multithreaded Language'

template<int C>
class Deque
{
    public:

        // TODO : replace all std::atomic access with explicit memory ordering

        /** Add a new object to the deque (by the worker */
        void
        push(void * obj)
        {
            ++T;
        }

        /* Remove an object from the deque (by the worker) */
        void *
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

        /* Steal from the deque (by the thief) */
        void *
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

        void * objects[C];

        // TODO : what to alignas(std::hardware_constructive_interference_size) ?
        spinlock_t lock;
        std::atomic<int> H;
        std::atomic<int> T;
};

#endif /* __DEQUE_HPP__ */
