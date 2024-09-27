#ifndef __DEQUE_HPP__
# define __DEQUE_HPP__

# include "sync/spinlock.h"
# include "device/iqueue.hpp"

# include <atomic>
# include <assert.h>
# include <list>

/**
 *  A naive list implementation with a lock, to be used for debugging purposes
 *  of the lock-free list
 */

template<typename T>
class NaiveQueue : IQueue<T>
{
    public:

        NaiveQueue() : list(), lock(0) {}
        ~NaiveQueue() {}

        void
        push(const T & t)
        {
            SPINLOCK_LOCK(this->lock);
            {
                this->list.push_back(t);
            }
            SPINLOCK_UNLOCK(this->lock);
        }

        T
        pop(void)
        {
            if (!this->list.empty())
            {
                SPINLOCK_LOCK(this->lock);

                # if 1
                T t = this->list.front();
                this->list.pop_front();
                # else
                T t = this->list.back();
                this->list.pop_back();
                # endif

                SPINLOCK_UNLOCK(this->lock);

                return t;
            }
            return nullptr;
        }

        T
        steal(void)
        {
            assert(0); // no workstealing implemented
            return nullptr;
        }

        bool
        is_empty(void) const
        {
            return this->list.empty();
        }

    private:
        std::list<T> list;
        spinlock_t lock;
};

#endif /* __DEQUE_HPP__ */
