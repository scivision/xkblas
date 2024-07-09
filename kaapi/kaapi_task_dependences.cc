# include "impl/access-interval-multi-tree.hpp"
# include "region.hpp"

# include <iostream>

extern "C" {
# include "kaapi_impl.h"
};

class KaapiCartesian2DTree : public AccessIntervalMultiTree<2>
{
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


static inline KaapiCartesian2DTree *
__dependences_cartesian2D_ensure_regions(kaapi_thread_t * thread)
{
    kaapi_context_t * kctx  = kaapi_thread2context(thread);
    if (kctx->regions == NULL)
        kctx->regions = new KaapiCartesian2DTree(thread);
    assert(kctx->regions);
    return static_cast<KaapiCartesian2DTree *>(kctx->regions);
}

static inline access_mode_t
__access_mode_convert(kaapi_access_mode_t mode)
{
    if (mode & KAAPI_ACCESS_MODE_W)
        return OUT;

    // TODO : if this explodes, you have to implement 'OUTSET' mode as follows :-)
    // if (mode & KAAPI_ACCESS_MODE_CW)
    //  return OUTSET;
    assert((mode & KAAPI_ACCESS_MODE_CW) == 0);

    return IN;
}

inline void
kaapi_dependences_cartesian2D_intersect(
    kaapi_thread_t * thread,
    kaapi_task_t * task,
    kaapi_access_mode_t mode,
    int x0, int y0,
    int x1, int y1
) {
    KaapiCartesian2DTree * tree = __dependences_cartesian2D_ensure_regions(thread);
    access_mode_t am = __access_mode_convert(mode);
    interval_t intervals[2] = {
        { .a = x0, .b = x1 },
        { .a = y0, .b = y1 },
    };
    Region<2> region(intervals);
    tree->intersect(am, region, task);
}

inline void
kaapi_dependences_cartesian2D_insert(
    kaapi_thread_t * thread,
    kaapi_task_t * task,
    kaapi_access_mode_t mode,
    int x0, int y0,
    int x1, int y1
) {
    KaapiCartesian2DTree * tree = __dependences_cartesian2D_ensure_regions(thread);
    access_mode_t am = __access_mode_convert(mode);
    interval_t intervals[2] = {
        { .a = x0, .b = x1 },
        { .a = y0, .b = y1 },
    };
    Region<2> region(intervals);
    tree->insert(am, region, task);
}

template<int N>
void
kaapi_dependences_cartesian2D_N(
    kaapi_thread_t * thread,
    kaapi_task_t * task,
    kaapi_access_mode_t mode[N],
    int coords[N][4]
) {
    for (int n = 0 ; n < N ; ++n)
        kaapi_dependences_cartesian2D_intersect(
                thread, task,
                mode[n],
                coords[n][0], coords[n][1],
                coords[n][2], coords[n][3]
        );

    for (int n = 0 ; n < N ; ++n)
        kaapi_dependences_cartesian2D_insert(
                thread, task,
                mode[n],
                coords[n][0], coords[n][1],
                coords[n][2], coords[n][3]
        );
}

extern "C" void
kaapi_dependences_cartesian2D(
    kaapi_thread_t * thread,
    kaapi_task_t * task,
    kaapi_access_mode_t mode,
    int x0, int y0,
    int x1, int y1
) {
    kaapi_dependences_cartesian2D_intersect(thread, task, mode, x0, y0, x1, y1);
    kaapi_dependences_cartesian2D_insert   (thread, task, mode, x0, y0, x1, y1);
}

extern "C" void
kaapi_dependences_cartesian2D_3(
    kaapi_thread_t * thread,
    kaapi_task_t * task,
    kaapi_access_mode_t mode[3],
    int coords[3][4]
) {
    kaapi_dependences_cartesian2D_N<3>(thread, task, mode, coords);
}


