# include <xkomp/xkomp.h>

# include <xkrt/logger/logger.h>
# include <kmp.h>
# include <stdint.h>

xkomp_t  _xkomp;
xkomp_t * xkomp;

extern "C"
xkomp_t *
xkomp_get(void)
{
    if (xkomp == NULL)
    {
        xkomp = &_xkomp;
        xkrt_init(&xkomp->runtime);
    }

    return xkomp;
}

extern "C"
kmp_int32
__kmpc_global_thread_num(ident_t * loc)
{
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    return tls->gtid;
}

extern "C"
int
omp_get_thread_num(void)
{
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    return tls->tid;
}

extern "C"
int
omp_get_num_threads(void)
{
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    return tls->team->priv.nthreads;
}
