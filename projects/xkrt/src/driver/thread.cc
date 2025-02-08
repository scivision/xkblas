/* ************************************************************************** */
/*                                                                            */
/*   thread.cc                                                                */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:59:23 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include <xkrt/driver/thread.hpp>
# include <xkrt/logger/logger.h>

# include <cassert>
# include <cstring>

Thread::Thread()
{
    this->capacity = THREAD_MAX_MEMORY;
    while (1)
    {
        this->memory_stack_bottom = (uint8_t *) malloc(this->capacity);
        if (this->memory_stack_bottom)
            break ;

        this->capacity = (size_t) (this->capacity * 2 / 3);
        if (this->capacity == 0)
            this->memory_stack_bottom = NULL;
    }
    this->memory_stack_ptr = this->memory_stack_bottom;
    assert(this->memory_stack_bottom);
    memset(this->memory_stack_bottom, 0, this->capacity);
}

Thread::~Thread()
{
}

uint8_t *
Thread::allocate(uint64_t size)
{
    # if 1
    if (this->memory_stack_ptr >= this->memory_stack_bottom + THREAD_MAX_MEMORY)
        LOGGER_FATAL("Stack overflow ! Increase `THREAD_MAX_MEMORY` and recompile");
    uint8_t * memory = this->memory_stack_ptr;
    this->memory_stack_ptr += size;
    return memory;
    # else
    return (uint8_t *) malloc(size);
    # endif
}

void
Thread::deallocate_all(void)
{
    this->memory_stack_ptr = this->memory_stack_bottom;
}
