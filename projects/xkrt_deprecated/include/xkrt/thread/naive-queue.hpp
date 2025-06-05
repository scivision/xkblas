/* ************************************************************************** */
/*                                                                            */
/*   naive-queue.hpp                                              .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/17 15:54:31 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:06:56 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __DEQUE_HPP__
# define __DEQUE_HPP__

# include <xkrt/sync/spinlock.h>
# include <xkrt/driver/iqueue.hpp>

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

        NaiveQueue() : list(), lock() {}
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
            SPINLOCK_LOCK(this->lock);

            if (this->is_empty())
            {
                SPINLOCK_UNLOCK(this->lock);
                return nullptr;
            }

            T t = this->list.back();
            this->list.pop_back();

            SPINLOCK_UNLOCK(this->lock);

            return t;
        }

        T
        steal(void)
        {
            return this->pop();
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
