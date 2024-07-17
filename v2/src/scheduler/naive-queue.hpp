#ifndef __DEQUE_HPP__
# define __DEQUE_HPP__

# include "sync/spinlock.h"
# include "scheduler/iqueue.hpp"

# include <atomic>
# include <stack>

/**
 *  A naive queue implementation with a lock, to be used for debugging purposes
 *  of the lock-free queue
 */

template<typename T>
class NaiveQueue : IQueue<T>
{
    public:

        NaiveQueue() : stack(), lock(0) {}
        ~NaiveQueue() {}

        void
        push(const T & t)
        {
            SPINLOCK_LOCK(this->lock);
            {
                this->stack.push(t);
            }
            SPINLOCK_UNLOCK(this->lock);
        }

        T
        pop(void)
        {
            T t;
            SPINLOCK_LOCK(this->lock);
            {
                t = this->stack.top();
                this->stack.pop();
            }
            SPINLOCK_UNLOCK(this->lock);
            return t;
        }

        T
        steal(void)
        {
            return this->pop();
        }

    private:

        std::stack<T> stack;
        spinlock_t lock;
};

#endif /* __DEQUE_HPP__ */
