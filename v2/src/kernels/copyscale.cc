# include <cblas.h>

# include "min-max.h"
# include "xkblas-context.h"

# include "device/task-launcher.h"
# include "device/thread-producer.hpp"
# include "logger/todo.h"
# include "logger/logger.h"
# include "kernels/auto-tile.h"
# include "kernels/kernel-type.h"
# include "sync/access.hpp"
# include "sync/alignedas.h"
# include "sync/cache-line-size.hpp"

# include <cassert>

typedef struct alignas(CACHE_LINE_SIZE) args_t
{
    args_t(
        int m, int n,
        bool should_copy, int * IW
    ) :
        m(m), n(n),
        should_copy(should_copy), IW(IW)
    {}

    ~args_t() {}

    const int m;
    const int n;
    const bool should_copy;
    int * IW;

} args_t;

static task_format_id_t format_id;

/* m, n, k are matrix sizes
 * Am, An, ..., Cn are index of the tile begining */
int
xkblas_£copyscale_tile_async(
    xkblas_context_t * context,
    int m, int n,
    bool should_copy,
    int * IW,
    const TYPE * D, int Dm, int Dn, int ldd,
          TYPE * L, int Lm, int Ln, int ldl,
          TYPE * U, int Um, int Un, int ldu
) {
    assert((uintptr_t)D % ldd == 0);
    assert((uintptr_t)L % ldl == 0);
    assert((uintptr_t)U % ldu == 0);

    const uint64_t task_size = sizeof(Task);
    const uint64_t args_size = sizeof(args_t);
    assert(is_alignedas(task_size, CACHE_LINE_SIZE));
    assert(is_alignedas(args_size, CACHE_LINE_SIZE));

    ThreadProducer * thread = ThreadProducer::self();
    uint8_t * mem = thread->allocate(task_size + args_size);
    assert(mem);

    // const int ocr_access = UNSPECIFIED_TASK_ACCESS;
    const int ocr_access = 1;
    Task * task = reinterpret_cast<Task *>  (mem + 0);
    new(task) Task(format_id, ocr_access, UNSPECIFIED_DEVICE_GLOBAL_ID);

    # ifndef NDEBUG
    assert(transA == CblasNoTrans);
    assert(transB == CblasNoTrans);
    snprintf(task->label, sizeof(task->label), "copyscale(D=(%d,%d) ; L=(%d,%d) ; U=(%d,%d))", Dm, Dn, Lm, Ln, Um, Un);
    # endif /* NDEBUG */

    args_t  * args = reinterpret_cast<args_t *>(mem + task_size);
    new(args) args_t(m, n, should_copy, IW);

    # define NACCESSES 3
    static_assert(NACCESSES <= TASK_MAX_ACCESSES);
    new(task->accesses + 0) Access(MATRIX_COLMAJOR, D, ldd, Dm, Dn, n, n, sizeof(TYPE), ACCESS_MODE_R);
    new(task->accesses + 1) Access(MATRIX_COLMAJOR, L, ldl, Lm, Ln, m, n, sizeof(TYPE), ACCESS_MODE_RW);
    new(task->accesses + 2) Access(MATRIX_COLMAJOR, U, ldu, Um, Un, n, m, sizeof(TYPE), ACCESS_MODE_W);
    thread->commit<NACCESSES>(context, task);
    # undef NACCESSES

    return 0;
}

int
xkblas_£copyscale_async(
    int m, int n,
    bool should_copy,
    int * IW,
    const TYPE * D, int ldd,
          TYPE * L, int ldl,
          TYPE * U, int ldu
) {

    if (ldd < n)
    {
        XKBLAS_FATAL("Invalid value for ldd\n");
        return -5;
    }

    if (ldl < n)
    {
        XKBLAS_FATAL("Invalid value for ldl");
        return -7;
    }

    if (ldu < m)
    {
        XKBLAS_FATAL("Invalid value for ldu");
        return -9;
    }

    // TODO check if something to do ... n_cols > 0

    xkblas_context_t * context = xkblas_context_get();
    assert(context);

    int * tile = context->conf.kernels[XKBLAS_KERNEL_TYPE_COPYSCALE].tile;
    if (tile[0] == 0 || tile[1] == 0)
    {
        int args[2] = {m, n};
        xkblas_kernel_auto_tile(XKBLAS_KERNEL_TYPE_COPYSCALE, args, tile);
    }

    /* set tiling parameters */
    const int Dmb = tile[1];
    const int Dnb = tile[1];
    const int Lmb = tile[0];
    const int Lnb = tile[1];
    const int Umb = tile[1];
    const int Unb = tile[0];

    const int Dm  = n;
    const int Dn  = n;
    const int Lm  = m;
    const int Ln  = n;
    const int Um  = n;
    const int Un  = m;

    const int Dmt = XKBLAS_NUM_OF_TILES(Dm, Dmb);
    const int Dnt = XKBLAS_NUM_OF_TILES(Dn, Dnb);
    const int Lmt = XKBLAS_NUM_OF_TILES(Lm, Lmb);
    const int Lnt = XKBLAS_NUM_OF_TILES(Ln, Lnb);
    const int Umt = XKBLAS_NUM_OF_TILES(Um, Umb);
    const int Unt = XKBLAS_NUM_OF_TILES(Un, Unb);

    # define D(i, j) D, i*Dmb, j*Dnb
    # define L(i, j) L, i*Lmb, j*Lnb
    # define U(i, j) U, i*Umb, j*Unb

    for (int tm = 0; tm < Lmt ; ++tm)
    {
        const int bs_m = (tm == Lmt-1) ? (m-tm*Lnb) : Lnb;
        for( int tn = 0; tn < Lnt ; ++tn )
        {
            const int bs_n = (tn == Lnt-1) ? (n-tn*Lmb) : Lmb;
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

# if USE_CUDA
#  include "device/cublas-helper.h"

extern "C"
int
cuda_£copyscale(
    cudaStream_t cuda_stream,
    int m, int n,
    bool should_copy, int * IW,
    const CU_TYPE * D, int ldd,
          CU_TYPE * L, int ldl,
          CU_TYPE * U, int ldu
);

static void
body_cuda(void * vlauncher)
{
    task_launcher_t * launcher = (task_launcher_t *) vlauncher;
    assert(launcher);

    cublasStatus_t res;
    cublasHandle_t handle = (cublasHandle_t) launcher->handle;

    const Access * D = launcher->task->accesses + 0;
    const Access * L = launcher->task->accesses + 1;
    const Access * U = launcher->task->accesses + 2;

    assert(D->device_view.addr % D->host_view.sizeof_type == 0);
    assert(L->device_view.addr % L->host_view.sizeof_type == 0);
    assert(U->device_view.addr % U->host_view.sizeof_type == 0);
    assert(handle);

    args_t * args = (args_t *) (launcher->task + 1);
    assert(args);

    cudaStream_t cuda_stream;
    res = cublasGetStream( (cublasHandle_t) handle, &cuda_stream );
    xkblas_cublas_status_check(res);

    cuda_£copyscale(
        cuda_stream,
        args->m, args->n,
        args->should_copy, args->IW,
        (const CU_TYPE *) D->device_view.addr, D->device_view.ld,
        (      CU_TYPE *) L->device_view.addr, L->device_view.ld,
        (      CU_TYPE *) U->device_view.addr, U->device_view.ld
    );
}

# endif /* USE_CUDA */

# ifdef USE_CPU

/* CPU driver */
extern "C"
int
xkblas_zcopyscale_native(
    int m, int n,
    bool should_copy,
    int * IW,
    const TYPE * D, int ldd,
          TYPE * L, int ldl,
          TYPE * U, int ldu
) {
    // TODO need to check validity ??
    int bsizecopy = 250;
    for( int i_row_start = 0; i_row_start < m; i_row_start += bsizecopy )
    {
        int blocksize = MIN(bsizecopy, m - i_row_start * bsizecopy);
        for( int i_col = 0; i_col < n; i_col++ )
        {
            // TODO implement 2x2 case (if needed)
            TYPE A11 = 1.0/D[i_col + i_col * ldd];
            if( should_copy )
            {
                for( int i_row = 0; i_row < blocksize; i_row++ )
                {
                    U[ (i_row_start+i_row) + i_col * ldu  ] = L[ (i_row_start+i_row) * ldl + i_col ];
                }
            }
            for( int i_row = 0; i_row < blocksize; i_row++ )
            {
                L[ (i_row_start+i_row) * ldl + i_col ] *= A11;
            }

        }
    }

    return 0;
}

static void
body_cpu(void * args)
{
    XKBLAS_DEBUG("Executing a copyscale on cpu");
    xkblas_zcopyscale_native(...);
}

# endif /* USE_CPU */

//////////////////////////
// TASK FORMAT REGISTER //
//////////////////////////

void
register_£copyscale_format(void)
{
    task_format_t format;
    memset(&format, 0, sizeof(task_format_t));

# ifdef USE_CPU
    format.f[XKBLAS_DRIVER_TYPE_CPU] = body_cpu;
# endif /* USE_CPU */
# ifdef USE_CUDA
    format.f[XKBLAS_DRIVER_TYPE_CUDA] = body_cuda;
# endif /* USE_CUDA */
    snprintf(format.label, sizeof(format.label), "£copyscale");
    format.target = TASK_FORMAT_TARGET_DRIVER;
    format_id = task_format_create(&format);
}
