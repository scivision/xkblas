# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>

extern "C"
void
__kmpc_fork_call(ident_t * loc, kmp_int32 argc, kmpc_micro microtask, ...)
{
    unsigned int nthreads = 4;

    // TODO - create a team of nthreads - but the calling thread must be part of the team
    LOGGER_NOT_IMPLEMENTED();
}
