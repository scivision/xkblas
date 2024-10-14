#ifndef __LOCKABLE_HPP__
# define __LOCKABLE_HPP__

# include "sync/spinlock.h"

/* an abstract object that can be locked */
class Lockable {

    public:
        spinlock_t spinlock;

        volatile bool locked;

    public:
        Lockable() : spinlock(), locked(false) {}
        ~Lockable() {}

    public:

        inline void
        lock(void)
        {
            SPINLOCK_LOCK(this->spinlock);
            this->locked = true;
        }

        inline void
        unlock(void)
        {
            this->locked = false;
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
