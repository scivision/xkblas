#ifndef __IQUEUE_HPP__
# define __IQUEUE_HPP__

template<typename T>
class IQueue
{
    public:

        /** Add a new object to the deque (by the worker */
        virtual void push(T & obj) = 0;

        /* Remove an object from the deque (by the worker) */
        virtual T pop(void) = 0;

        /* Steal from the deque (by the thief) */
        virtual T steal(void) = 0;
};

#endif /* __IQUEUE_HPP__ */
