/* ************************************************************************** */
/*                                                                            */
/*   copyscale.cc                                                             */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:47 by Romain PEREIRA            / _______ \    */
/*   Updated: 2025/03/25 21:57:23 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# include "context.h"
# include "auto-tile.h"
# include "xkblas/kernel-type.h"
# include "xkblas/cblas.h"
# include "xkblas/xkblas-experimental.h"

# include <xkrt/xkrt-support.h>
# include <xkrt/driver/thread.hpp>
# include <xkrt/logger/logger.h>
# include <xkrt/logger/todo.h>
# include <xkrt/min-max.h>
# include <xkrt/memory/access.hpp>
# include <xkrt/memory/cache-line-size.hpp>

# include <cassert>

typedef struct args_t
{
    public:
        const size_t m;
        const size_t n;
        const int should_copy;
        int * IW;

        args_t(
            size_t m, size_t n,
            int should_copy, int * IW
        ) :
            m(m), n(n),
            should_copy(should_copy), IW(IW)
        {}
        ~args_t() {}

} args_t;

static task_format_id_t format_id;

/* m, n, k are matrix sizes
 * Am, An, ..., Cn are index of the tile begining */
int
xkblas_£copyscale_tile_async(
    xkblas_context_t * context,
    size_t m, size_t n,
    int should_copy,
    int * IW,
    const TYPE * D, const size_t Dm, const size_t Dn, int ldd,
          TYPE * L, const size_t Lm, const size_t Ln, int ldl,
          TYPE * U, const size_t Um, const size_t Un, int ldu
) {
    Thread * thread = Thread::self();

    # define AC 3
    constexpr task_flag_bitfield_t flags = TASK_FLAG_DEVICE | TASK_FLAG_DEPENDENT;
    constexpr size_t task_size = task_compute_size(flags, AC);
    constexpr size_t args_size = sizeof(args_t);

    task_t * task = thread->allocate_task(task_size + args_size);
    new(task) task_t(format_id, flags);

    task_dep_info_t * dep = TASK_DEP_INFO(task);
    new (dep) task_dep_info_t(AC);

    task_dev_info_t * dev = TASK_DEV_INFO(task);
    constexpr size_t ocr_access = 1;
    new (dev) task_dev_info_t(UNSPECIFIED_DEVICE_GLOBAL_ID, ocr_access);

    args_t * args = (args_t *) TASK_ARGS(task, task_size);
    new(args) args_t(m, n, should_copy, IW);

    # ifndef NDEBUG
    snprintf(task->label, sizeof(task->label),
            "copyscale(D=(%zu,%zu) ; L=(%zu,%zu) ; U=(%zu,%zu))", Dm, Dn, Lm, Ln, Um, Un);
    # endif /* NDEBUG */

    static_assert(AC <= TASK_MAX_ACCESSES);
    access_t * accesses = TASK_ACCESSES(task, flags);
    new(accesses + 0) access_t(task, MATRIX_COLMAJOR, D, ldd, Dm, Dn, n, n, sizeof(TYPE), ACCESS_MODE_R , ACCESS_CONCURRENCY_SEQUENTIAL, XKBLAS_ACCESS_SCOPE);
    new(accesses + 1) access_t(task, MATRIX_COLMAJOR, L, ldl, Lm, Ln, n, m, sizeof(TYPE), ACCESS_MODE_RW, ACCESS_CONCURRENCY_SEQUENTIAL, XKBLAS_ACCESS_SCOPE);
    new(accesses + 2) access_t(task, MATRIX_COLMAJOR, U, ldu, Um, Un, m, n, sizeof(TYPE), ACCESS_MODE_W , ACCESS_CONCURRENCY_SEQUENTIAL, XKBLAS_ACCESS_SCOPE);
    thread->resolve<AC>(task, accesses);
    # undef AC

    context->runtime.task_commit(task);

    return 0;
}

extern "C"
int
xkblas_£copyscale_async(
    int m, int n,
    int should_copy,
    int * IW,
    const TYPE * D, int ldd,
          TYPE * L, int ldl,
          TYPE * U, int ldu
) {

    if (ldd < n)
    {
        LOGGER_FATAL("Invalid value for ldd\n");
        return -5;
    }

    if (ldl < n)
    {
        LOGGER_FATAL("Invalid value for ldl");
        return -7;
    }

    if (ldu < m)
    {
        LOGGER_FATAL("Invalid value for ldu");
        return -9;
    }

    // TODO check if something to do ... n_cols > 0

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    size_t ts = context->conf.kernels[XKBLAS_KERNEL_TYPE_COPYSCALE].tile;
    if (ts == 0)
    {
        int args[2] = {m, n};
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_COPYSCALE, args, &ts);
    }

    /* set tiling parameters */
    const size_t Dmb = ts;
    const size_t Dnb = ts;
    const size_t Lmb = ts;
    const size_t Lnb = ts;
    const size_t Umb = ts;
    const size_t Unb = ts;

    const size_t Dm  = n;
    const size_t Dn  = n;
    const size_t Lm  = m;
    const size_t Ln  = n;
    const size_t Um  = n;
    const size_t Un  = m;

    const size_t Dmt = NUM_OF_TILES(Dm, Dmb);
    const size_t Dnt = NUM_OF_TILES(Dn, Dnb);
    const size_t Lmt = NUM_OF_TILES(Lm, Lmb);
    const size_t Lnt = NUM_OF_TILES(Ln, Lnb);
    const size_t Umt = NUM_OF_TILES(Um, Umb);
    const size_t Unt = NUM_OF_TILES(Un, Unb);

    # define D(i, j) D, i*Dmb, j*Dnb
    # define L(i, j) L, i*Lmb, j*Lnb
    # define U(i, j) U, i*Umb, j*Unb

    for (int tm = 0; tm < Lmt ; ++tm)
    {
        const size_t bs_m = (tm == Lmt-1) ? (m-tm*Lnb) : Lnb;
        for( int tn = 0; tn < Lnt ; ++tn )
        {
            const size_t bs_n = (tn == Lnt-1) ? (n-tn*Lmb) : Lmb;
            xkblas_£copyscale_tile_async(
                context,
                bs_m, bs_n,
                should_copy, IW,
                D(tn, tn), ldd,
                L(tn, tm), ldl,
                U(tm, tn), ldu
            );
        }
    }

    # undef D
    # undef L
    # undef U

    return 0;
}

# if XKRT_SUPPORT_CUDA
#  include <xkblas/cublas-helper.h>
#  include <xkrt/driver/driver-cu.h>

extern "C"
int
cuda_£copyscale(
    cudaStream_t cuda_stream,
    int m, int n,
    int should_copy, int * IW,
    const CU_TYPE * D, int ldd,
          CU_TYPE * L, int ldl,
          CU_TYPE * U, int ldu
);

static void
body_cuda(
    xkrt_stream_cu_t * stream,
    xkrt_stream_instruction_t * instr,
    xkrt_stream_instruction_counter_t idx
) {
    assert(stream);

    cudaStream_t cuda_stream = stream->cu.handle.high;
    assert(cuda_stream);

    task_t * task = (task_t *) instr->kern.vargs;
    assert(task);

    const access_t * accesses = TASK_ACCESSES(task);
    const access_t * D = accesses + 0;
    const access_t * L = accesses + 1;
    const access_t * U = accesses + 2;

    assert(D->device_view.addr % D->host_view.sizeof_type == 0);
    assert(L->device_view.addr % L->host_view.sizeof_type == 0);
    assert(U->device_view.addr % U->host_view.sizeof_type == 0);

    args_t * args = (args_t *) TASK_ARGS(task);
    assert(args);

    cuda_£copyscale(
        cuda_stream,
        (int) args->m, (int) args->n,
        args->should_copy, args->IW,
        (const CU_TYPE *) D->device_view.addr, (int) D->device_view.ld,
        (      CU_TYPE *) L->device_view.addr, (int) L->device_view.ld,
        (      CU_TYPE *) U->device_view.addr, (int) U->device_view.ld
    );

    XKBLAS_CUBLAS_CALL_POST();
}

# endif /* XKRT_SUPPORT_CUDA */

/* HOST driver */
extern "C"
int
xkblas_£copyscale_native(
    int m, int n,
    int should_copy,
    int * IW,
    const TYPE * D, int ldd,
          TYPE * L, int ldl,
          TYPE * U, int ldu
) {
    // TODO need to check validity ??

    const TYPE one = (TYPE) 1.0;
    size_t bsizecopy = 250;

    for( size_t i_row_start = 0; i_row_start < m; i_row_start += bsizecopy )
    {
        size_t blocksize = MIN(bsizecopy, m - i_row_start * bsizecopy);
        for( size_t i_col = 0; i_col < n; i_col++ )
        {
            // TODO implement 2x2 case (if needed)
            TYPE A11 = one / D[i_col + i_col * ldd];
            if( should_copy )
            {
                for( size_t i_row = 0; i_row < blocksize; i_row++ )
                {
                    U[ (i_row_start+i_row) + i_col * ldu  ] = L[ (i_row_start+i_row) * ldl + i_col ];
                }
            }
            for( size_t i_row = 0; i_row < blocksize; i_row++ )
            {
                L[ (i_row_start+i_row) * ldl + i_col ] *= A11;
            }

        }
    }

    return 0;
}

# if XKRT_SUPPORT_HOST

static void
body_cpu(void * args)
{
    LOGGER_DEBUG("Executing a copyscale on cpu");
    xkblas_zcopyscale_native(...);
}

# endif /* XKRT_SUPPORT_HOST */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£copyscale_format(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

    # if XKRT_SUPPORT_HOST
    format.f[XKRT_DRIVER_TYPE_HOST] = (task_format_func_t) body_cpu;
    # endif /* XKRT_SUPPORT_HOST */

    # if XKRT_SUPPORT_CUDA
    format.f[XKRT_DRIVER_TYPE_CUDA] = (task_format_func_t) body_cuda;
    # endif /* XKRT_SUPPORT_CUDA */

    snprintf(format.label, sizeof(format.label), "£copyscale");
    format_id = xkblas_task_format_create(&format);
}
