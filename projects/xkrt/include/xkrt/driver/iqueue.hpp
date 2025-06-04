/* ************************************************************************** */
/*                                                                            */
/*   iqueue.hpp                                                   .-*-.       */
/*                                                              .'* *.'       */
/*   Created: 2024/07/16 16:15:23 by Romain Pereira          __/_*_*(_        */
/*   Updated: 2025/06/03 18:00:29 by Romain PEREIRA         / _______ \       */
/*                                                          \_)     (_/       */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/*   Author: Thierry GAUTIER <thierry.gautier@inrialpes.fr>                   */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>                         */
/*                                                                            */
/*   Copyright: see AUTHORS                                                   */
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
