/* ************************************************************************** */
/*                                                                           */
/*   ze_blas.cc                                                               */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:57:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

// TODO : having to use sycl here is super ugly, but Intel do not seems to
// provide the kernels direcly, so we could pass them via a
// zeCommandListAdKernelLaunch - or even to simply call a gemm with a command list/queue
// see https://www.intel.com/content/www/us/en/docs/dpcpp-cpp-compiler/developer-guide-reference/2024-0/intel-oneapi-level-zero-backend-specification.html

# include <xkrt/logger/logger-ze.h>
# include <xkrt/driver/driver-ze.h>

# include <sycl/sycl.hpp>
# include <sycl/ext/oneapi/backend/level_zero.hpp>
# include <oneapi/mkl.hpp>

# include <ze_api.h>

# define SOURCE_CODE(...) #__VA_ARGS__

///////////////
// Utilities //
///////////////
static void
ze_blas_load(
    xkrt_stream_ze_t * stream,
    ze_module_handle_t * module,
    const char * src
){
    assert(stream);
    assert(*module == NULL);
    assert(src);

    ze_context_handle_t ze_context;
    ZE_SAFE_CALL(zeCommandListGetContextHandle(stream->ze.command.list, &ze_context));

    ze_module_desc_t module_desc = {
        .stype = ZE_STRUCTURE_TYPE_MODULE_DESC,
        .pNext = NULL,
        ZE_MODULE_FORMAT_IL_SPIRV, static_cast<uint32_t>(binary.size()),
        binary.data(), nullptr, nullptr};
    ze_module_handle_t module = nullptr;
    status = zeModuleCreate(context, device, &module_desc, &module, nullptr);

    LOGGER_FATAL("TODO");
}

static inline void
ze_blas_load_ensure(
    xkrt_stream_ze_t * stream,
    ze_module_handle_t * module,
    const char * src
) {
    if (*module == NULL)
        ze_blas_load(stream, module, src);
}

//////////
// GEMM //
//////////

ze_module_handle_t GEMM_NAIVE_MODULE = NULL;
const char * GEMM_NAIVE_SRC = SOURCE_CODE(
    __kernel
    void
    GEMM_NAIVE_SRC(
        int m, int n, int k,
        const float alpha,
        __global const float * A, int lda,
        __global const float * B, int ldb,
        const float beta,
        __global       float * C, int ldc
    ) {
      int j = get_global_id(0);
      int i = get_global_id(1);
      float sum = 0.0f;
      for (int k = 0; k < size; ++k)
          sum += a[i * lda + k] * b[k * ldb + j];
      C[i * size + j] = sum;
    }
);

void
ze_blas_sgemm(
    xkrt_stream_ze_t * stream,
    int transA, int transB,
    int m, int n, int k,
    const float * alpha,
    const float * A, int lda,
    const float * B, int ldb,
    const float * beta,
          float * C, int ldc
) {
    ze_blas_load_ensure(stream, &GEMM_NAIVE_MODULE, GEMM_NAIVE_SRC);

    const xkrt_stream_instruction_counter_t wp = stream->super.pending.pos.w % stream->super.pending.capacity;
    ze_event_handle_t ze_event_handle = stream->ze.events.list[wp];


    LOGGER_FATAL("TODO");

    # if 0
    // create sycl context
    ze_context_handle_t ze_context;
    ZE_SAFE_CALL(zeCommandListGetContextHandle(stream->ze.command.list, &ze_context));

    std::vector<sycl::device>(1, dev);

    sycl::backend_input_t<sycl::backend::ext_oneapi_level_zero, sycl::context> interop_context{
        ze_context, ze_devices, sycl::ext::oneapi::level_zero::ownership::keep
    };
    sycl::context context = sycl::make_context<sycl::backend::ext_oneapi_level_zero>(interop_context);

    # if 0
    // create sycl queue from level zero command list
    sycl::queue queue = sycl::make_queue<sycl::backend::ext_oneapi_level_zero>(
        stream->ze.command.list,
        ze_context
    );
    # endif

    // TODO : set trans properly using passed arguments
    auto transa = oneapi::mkl::transpose::nontrans;
    auto transb = oneapi::mkl::transpose::nontrans;
    oneapi::mkl::blas::column_major::gemm(queue, transa, transb, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
    # endif
    LOGGER_FATAL("LAUNCH ASYNCHRONOUS KERNEL AND LINK TO EVENT");
}
