#ifndef __LOCKABLE_HPP__
# define __LOCKABLE_HPP__

# include "sync/spinlock.h"

/* an abstract object that can be locked */
class Lockable {

    public:
        spinlock_t spinlock;

    public:
        Lockable() : spinlock{0} {}
        ~Lockable() {}

    public:

        void
        lock(void)
        {
            SPINLOCK_LOCK(this->spinlock);
        }

        void
        unlock(void)
        {
            SPINLOCK_UNLOCK(this->spinlock);
        }

        bool
        is_locked(void) const
        {
            return SPINLOCK_ISLOCKED(this->spinlock);
        }
};

#endif /* __LOCKABLE_HPP__ */
