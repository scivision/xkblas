/* ************************************************************************** */
/*                                                                            */
/*   lockable.hpp                                                 .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/09/16 15:58:39 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:06:18 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@outlook.com>                      */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef __LOCKABLE_HPP__
# define __LOCKABLE_HPP__

# include <xkrt/sync/spinlock.h>

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
