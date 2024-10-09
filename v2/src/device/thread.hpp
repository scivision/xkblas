#ifndef __THREAD_HPP__
# define __THREAD_HPP__

# include "sync/cache-line-size.hpp"

# include <cstdint>

# ifndef THREAD_MAX_MEMORY
#  define THREAD_MAX_MEMORY (128*1024*1024)
# endif /* THREAD_MAX_MEMORY */

class alignas(CACHE_LINE_SIZE) Thread
{
    public:

        Thread();
        virtual ~Thread();

    public:

        /* allocates memory */
        uint8_t * allocate(uint64_t size);

        /* free all allocated memory */
        void deallocate_all(void);

    private:

        /* tasks stack */
        uint8_t * memory_stack_bottom;

        /* next free task pointer in the stack */
        uint8_t * memory_stack_ptr;

};

#endif /* __THREAD_HPP__ */
