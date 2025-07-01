# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdarg.h>

extern "C"
int32_t
__kmpc_single(ident_t * loc, int32_t gtid)
{
    xkrt_thread_t * tls = xkrt_thread_t::get_tls();
    assert(tls);

    return tls->tid == 0;
}

extern "C"
void
__kmpc_end_single(ident_t *loc, kmp_int32 global_tid)
{

}
