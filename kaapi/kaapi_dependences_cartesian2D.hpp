#ifndef __KAAPI_DEPENDENCES_CARTESIAN2D_HPP__
# define __KAAPI_DEPENDENCES_CARTESIAN2D_HPP__
# include "impl/access-interval-multi-tree.hpp"

extern "C" {
# include "kaapi.h"
};

class KaapiCartesian2DTree : public AccessIntervalMultiTree<2> {

    public:
        KaapiCartesian2DTree(kaapi_thread_t * th) : thread(th), AccessIntervalMultiTree<2>() {}

        void
        on_hazard(
            const Region<2> & rx, void * x,
            const Region<2> & ry, void * y
        ) const {
            kaapi_task_t * tx = (kaapi_task_t *) x;
            kaapi_task_t * ty = (kaapi_task_t *) y;
            // TODO - set dependence
            std::cout << tx << " -> " << ty << std::endl;
        }

    private:
        kaapi_thread_t * thread;
};

#endif /* __KAAPI_DEPENDENCES_CARTESIAN2D_HPP__ */
