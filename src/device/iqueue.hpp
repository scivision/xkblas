/* ************************************************************************** */
/*                                                                            */
/*   iqueue.hpp                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/17 13:03:44 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

#ifndef __IQUEUE_HPP__
# define __IQUEUE_HPP__

template<typename T>
class IQueue
{
    public:

        /** Add a new object to the deque (by the worker */
        virtual void push(const T & obj) = 0;

        /* Remove an object from the deque (by the worker) */
        virtual T pop(void) = 0;

        /* Steal from the deque (by the thief) */
        virtual T steal(void) = 0;

        /* Return true if the queue is empty, false otherwise */
        virtual bool is_empty(void) const = 0;
};

#endif /* __IQUEUE_HPP__ */
