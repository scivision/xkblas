# include <xkomp/xkomp.h>

# include <xkrt/logger/logger.h>
# include <kmp.h>
# include <stdint.h>


xkrt_runtime_t  _runtime;
xkrt_runtime_t * runtime = NULL;

extern "C"
kmp_int32
__kmpc_global_thread_num(ident_t * loc)
{
    if (runtime == NULL)
    {
        runtime = &_runtime;
        xkrt_init(runtime);
    }

    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    return tls->gtid;
}
