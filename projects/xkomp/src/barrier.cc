# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

extern "C"
void
__kmpc_barrier(
    ident_t * loc,
    kmp_int32 global_tid
) {
    xkomp_t * xkomp = xkomp_get();
    xkrt_thread_t * thread = xkrt_thread_t::get_tls();
    assert(thread);

    xkomp->runtime.team_barrier<true>(thread->team, thread);
}
