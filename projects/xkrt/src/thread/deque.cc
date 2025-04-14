/* ************************************************************************** */
/*                                                                            */
/*   deque.cc                                                                 */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:44 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/20 00:17:10 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/task/task.hpp>
# include <xkrt/thread/deque.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/sync/mem.h>

// TODO : PROBLEM - this impl assumes `push` and `pop` are called from the same
// 'producer' thread and `steal` from any other thread.
// It does not support 'giving' tasks, that is, having a thread different from
// the producer pushing a task into this queue

template <typename T, int C>
void
xkrt_deque_t<T, C>::push(T const & task)
{
    int idx = _t++;
    tasks[idx%C] = task;
}

template <typename T, int C>
T
xkrt_deque_t<T,C>::pop(void)
{
    task_t * task;
    int idx = --_t;
    if (_h > _t)
    {
        ++_t;
        SPINLOCK_LOCK(lock);
        {
            int idx = --_t;
            if (_h > idx || ((task = tasks[idx%C]) == NULL))
            {
                ++_t;
                SPINLOCK_UNLOCK(lock);
                return NULL; // FAILURE
            }
            else
                tasks[idx%C] = NULL;
        }
        SPINLOCK_UNLOCK(lock);
    }
    else
    {
        task = tasks[idx%C];
        tasks[idx%C] = NULL;
    }
    return task; // SUCCESS
}

template <typename T, int C>
T
xkrt_deque_t<T,C>::steal(void)
{
    task_t * task;
    SPINLOCK_LOCK(lock);
    {
        int idx = _h++;
        if (idx >= _t || ((task = tasks[idx%C]) == NULL))
        {
            --_h;
            SPINLOCK_UNLOCK(lock);
            return NULL;  // FAILURE
        }
        tasks[idx%C] = NULL;
    }
    SPINLOCK_UNLOCK(lock);
    return task;  // SUCCESS
}

// Explicit instantiation
template struct xkrt_deque_t<task_t *, 4096>;
