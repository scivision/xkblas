/* ************************************************************************** */
/*                                                                            */
/*   driver_ze_blas.cc                                                        */
/*                                                                   .-*-.    */
/*   Author: Romain PEREIRA <romain.pereira@inria.fr>              .'* *.'    */
/*                                                              __/_*_*(_     */
/*   Created: 2024/12/17 13:03:43 by Romain PEREIRA            / _______ \    */
/*   Updated: 2024/12/19 11:57:35 by Romain PEREIRA            \_)     (_/    */
/*                                                                            */
/*   License: CeCILL-C                                                        */
/*                                                                            */
/* ************************************************************************** */

# if 0
    // retrieve event handle
    const xkrt_stream_instruction_counter_t wp = stream->super.pending.pos.w % stream->super.pending.capacity;
    ze_event_handle_t ze_event_handle = stream->ze.events.list[wp];

    // retrieve the kernel
    const ze_kernel_desc_t kernel_desc = { 
        .stype = ZE_STRUCTURE_TYPE_KERNEL_DESC,
        .pNext = nullptr,
        .flags = ZE_KERNEL_FLAG_EXPLICIT_RESIDENCY, // TODO : what do we want here ?
        .pKernelName = "GEMM"                       // TODO : what do we want here ?
    };  
    ze_kernel_handle_t ze_kernel_handle = NULL;
    ZE_SAFE_CALL(zeKernelCreate(module, &ze_kernel_desc, &ze_kernel_handle));

    // setup the launch grid
    uint32_t gsx, gsy, gsz; // group sizes
    const size_t sx = args->m;  // TODO : what size is this ?
    const size_t sy = args->n;  // TODO : what size is this ?
    const size_t sz = 0;        // TODO : what size is this ?
    ZE_SAFE_CALL(
        zeKernelSuggestGroupSize(
            kernel,
            sx, sy, sz, 
            &gsx, &gsy, &gsz
        )
    );  
    assert(sz == 1);                                // TODO : not sure about this
    assert((size % sx == 0) && (size % sy) == 0);   // TODO : not sure this is necessary

    const ze_group_count_t ze_group_count = { 
        .groupCountX = size / sx,   // TODO
        .groupCountY = size / sy,   // TODO
        .groupCountZ = 1            // TODO
    };  

    // TODO : may have to use
    # if 0
    sycl::event gemm(sycl::queue& queue, oneapi::math::transpose transa,
                                 oneapi::math::transpose transb, std::int64_t m, std::int64_t n,
                                 std::int64_t k, float alpha, const float* a, std::int64_t lda,
                                 const float* b, std::int64_t ldb, float beta, float* c,
                                 std::int64_t ldc,
                                 const std::vector<sycl::event>& dependencies = {})
    # endif

    // launch kernel
    ZE_SAFE_CALL(
        zeCommandListAppendLaunchKernel(
            stream->ze.command.list,
            ze_kernel_handle,
           &ze_group_count,
            ze_event_handle,
            0, /* numWaitEvents */
            NULL /* phWaitEvents */
        )
    );
# endif
