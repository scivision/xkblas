/* ************************************************************************** */
/*                                                                            */
/*   lockable.hpp                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:48 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:48 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOCKABLE_HPP__
# define __LOCKABLE_HPP__

# include "sync/spinlock.h"

/* an abstract object that can be locked */
class Lockable {

    public:
        spinlock_t spinlock;

        # ifndef NDEBUG
        volatile bool locked;
        # endif /* NDEBUG */

    public:
        # ifndef NDEBUG
        Lockable() : spinlock(), locked(false) {}
        # else
        Lockable() : spinlock() {}
        # endif /* NDEBUG */

        ~Lockable() {}

    public:

        inline void
        lock(void)
        {
            SPINLOCK_LOCK(this->spinlock);
            # ifndef NDEBUG
            this->locked = true;
            # endif /* NDEBUG */
        }

        inline void
        unlock(void)
        {
            # ifndef NDEBUG
            this->locked = false;
            # endif /* NDEBUG */
            SPINLOCK_UNLOCK(this->spinlock);
        }

        #ifndef NDEBUG
        bool
        is_locked(void) const
        {
            return this->locked;
        }
        # endif /* NDEBUG */
};

#endif /* __LOCKABLE_HPP__ */
