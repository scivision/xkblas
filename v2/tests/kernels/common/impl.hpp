#ifndef __IMPL_HPP__
# define __IMPL_HPP__

# include <assert.h>

class impl_t
{
    public:

        /* impl name */
        const char * name(void) const;

        /* init/deinit routines */
        void init(void);
        void deinit(void);

        /* wait for the completion of previously sent operations */
        void wait(void);

}; /* impl_t */

#endif /* __IMPL_HPP__ */
