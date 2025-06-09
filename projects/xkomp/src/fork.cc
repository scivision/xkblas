# include <xkomp/xkomp.h>
# include <xkomp/kmp.h>

# include <xkrt/logger/logger.h>

# include <assert.h>
# include <stdarg.h>

int
__kmp_invoke_microtask(
    kmpc_micro f,
    int gtid,
    int tid,
    int argc,
    void ** p_argv
) {
    switch (argc)
    {
        default:
            LOGGER_FATAL("Too many args (%d)\n", argc);
        case 0:
            (*f)(&gtid, &tid);
            break;
        case 1:
            (*f)(&gtid, &tid, p_argv[0]);
            break;
        case 2:
            (*f)(&gtid, &tid, p_argv[0], p_argv[1]);
            break;
        case 3:
            (*f)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2]);
            break;
        case 4:
            (*f)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3]);
            break;
        case 5:
            (*f)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4]);
            break;
        case 6:
            (*f)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4], p_argv[5]);
            break;
        case 7:
            (*f)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4], p_argv[5], p_argv[6]);
            break;
        case 8:
            (*f)(&gtid, &tid, p_argv[0], p_argv[1], p_argv[2], p_argv[3], p_argv[4], p_argv[5], p_argv[6], p_argv[7]);
            break;
    }
    return 1;
}

typedef struct  wargs_t
{
    int argc;
    void ** args;
    kmpc_micro f;
}               wargs_t;

static void *
__kmpc_fork_call_wrapper(
    xkrt_team_t * team,
    xkrt_thread_t * thread
) {
    assert(team);
    assert(thread);

    wargs_t * wargs = (wargs_t *) team->desc.args;
	__kmp_invoke_microtask(wargs->f, thread->gtid, thread->tid, wargs->argc, wargs->args);

    return NULL;
}

extern "C"
void
__kmpc_fork_call(
    ident_t * loc,
    kmp_int32 argc,
    kmpc_micro f,
    ...
) {
    // copy parallel region routine arguments
    va_list args;
    void ** args_copy = (void **) malloc(sizeof(void *) * argc);
    assert(args_copy);
    va_start(args, f);
    for (kmp_int32 i = 0; i < argc; ++i)
        args_copy[i] = va_arg(args, void *);
    va_end(args);

    // parse number pf threads
    LOGGER_NOT_IMPLEMENTED();
    unsigned int nthreads = 4;

    // create wrapper args
    wargs_t wargs = {
        .argc       = argc,
        .args       = args_copy,
        .f  = f
    };

    // create the team
    xkrt_team_t team = XKRT_TEAM_STATIC_INITIALIZER;
    team.desc.routine           = __kmpc_fork_call_wrapper;
    team.desc.args              = &wargs;
    team.desc.nthreads          = nthreads;
    team.desc.master_is_member  = true;

    xkomp_t * xkomp = xkomp_get();
    xkomp->runtime.team_create(&team);
    xkomp->runtime.team_join(&team);

    // free args
    free(args_copy);
}
