using CEnum: CEnum, @cenum

function caxpby_async(n, alpha, x, beta, y)
    @ccall libxkblas.xkblas_caxpby_async(n::Cint, alpha::ComplexF32, x::Ptr{ComplexF32}, beta::ComplexF32, y::Ptr{ComplexF32})::Cint
end

function caxpy_async(n, alpha, x, y)
    @ccall libxkblas.xkblas_caxpy_async(n::Cint, alpha::ComplexF32, x::Ptr{ComplexF32}, y::Ptr{ComplexF32})::Cint
end

function cdot_async(x, y, result)
    @ccall libxkblas.xkblas_cdot_async(x::Ptr{ComplexF32}, y::Ptr{ComplexF32}, result::Ptr{ComplexF32})::Cint
end

# no prototype is found for this function at ckernels.h:32:9, please use with caution
function cdivcopy_async()
    @ccall libxkblas.xkblas_cdivcopy_async()::Cint
end

function cfill(n, x, v)
    @ccall libxkblas.xkblas_cfill(n::Cint, x::Ptr{ComplexF32}, v::ComplexF32)::Cint
end

function cnrm2_async(n, x, result)
    @ccall libxkblas.xkblas_cnrm2_async(n::Cint, x::Ptr{ComplexF32}, result::Ptr{Cfloat})::Cint
end

# no prototype is found for this function at ckernels.h:38:9, please use with caution
function cscalcopy_async()
    @ccall libxkblas.xkblas_cscalcopy_async()::Cint
end

function cscale_async(n, s, x)
    @ccall libxkblas.xkblas_cscale_async(n::Cint, s::ComplexF32, x::Ptr{ComplexF32})::Cint
end

function ccopyscale_async(m, n, should_copy, IW, D, ldd, L, ldl, U, ldu)
    @ccall libxkblas.xkblas_ccopyscale_async(m::Cint, n::Cint, should_copy::Cint, IW::Ptr{Cint}, D::Ptr{ComplexF32}, ldd::Cint, L::Ptr{ComplexF32}, ldl::Cint, U::Ptr{ComplexF32}, ldu::Cint)::Cint
end

function cgemm_async(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_cgemm_async(transA::Cint, transB::Cint, m::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function cgemmt_async(uplo, transA, transB, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_cgemmt_async(uplo::Cint, transA::Cint, transB::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function cherk_async(uplo, transA, n, k, alpha, A, lda, beta, C, ldc)
    @ccall libxkblas.xkblas_cherk_async(uplo::Cint, transA::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function csyrk_async(uplo, trans, n, k, alpha, A, lda, beta, C, ldc)
    @ccall libxkblas.xkblas_csyrk_async(uplo::Cint, trans::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function ctrsm_async(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb)
    @ccall libxkblas.xkblas_ctrsm_async(side::Cint, uplo::Cint, transA::Cint, diag::Cint, m::Cint, n::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint)::Cint
end

function ctrmm_async(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb)
    @ccall libxkblas.xkblas_ctrmm_async(side::Cint, uplo::Cint, transA::Cint, diag::Cint, m::Cint, n::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint)::Cint
end

function csyr2k_async(uplo, trans, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_csyr2k_async(uplo::Cint, trans::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function csymm_async(side, uplo, m, n, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_csymm_async(side::Cint, uplo::Cint, m::Cint, n::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function cpotrf_async(uplo, n, A, lda)
    @ccall libxkblas.xkblas_cpotrf_async(uplo::Cint, n::Cint, A::Ptr{ComplexF32}, lda::Cint)::Cint
end

function cspmv_async(alpha, transA, nrows, ncols, nnz, csr_row_offsets, csr_col_indices, csr_values, X, beta, Y)
    @ccall libxkblas.xkblas_cspmv_async(alpha::Ptr{ComplexF32}, transA::Cint, nrows::Cint, ncols::Cint, nnz::Cint, csr_row_offsets::Ptr{Cint}, csr_col_indices::Ptr{Cint}, csr_values::Ptr{ComplexF32}, X::Ptr{ComplexF32}, beta::Ptr{ComplexF32}, Y::Ptr{ComplexF32})::Cint
end

function daxpby_async(n, alpha, x, beta, y)
    @ccall libxkblas.xkblas_daxpby_async(n::Cint, alpha::Cdouble, x::Ptr{Cdouble}, beta::Cdouble, y::Ptr{Cdouble})::Cint
end

function daxpy_async(n, alpha, x, y)
    @ccall libxkblas.xkblas_daxpy_async(n::Cint, alpha::Cdouble, x::Ptr{Cdouble}, y::Ptr{Cdouble})::Cint
end

function ddot_async(x, y, result)
    @ccall libxkblas.xkblas_ddot_async(x::Ptr{Cdouble}, y::Ptr{Cdouble}, result::Ptr{Cdouble})::Cint
end

# no prototype is found for this function at dkernels.h:32:9, please use with caution
function ddivcopy_async()
    @ccall libxkblas.xkblas_ddivcopy_async()::Cint
end

function dfill(n, x, v)
    @ccall libxkblas.xkblas_dfill(n::Cint, x::Ptr{Cdouble}, v::Cdouble)::Cint
end

function dnrm2_async(n, x, result)
    @ccall libxkblas.xkblas_dnrm2_async(n::Cint, x::Ptr{Cdouble}, result::Ptr{Cfloat})::Cint
end

# no prototype is found for this function at dkernels.h:38:9, please use with caution
function dscalcopy_async()
    @ccall libxkblas.xkblas_dscalcopy_async()::Cint
end

function dscale_async(n, s, x)
    @ccall libxkblas.xkblas_dscale_async(n::Cint, s::Cdouble, x::Ptr{Cdouble})::Cint
end

function dcopyscale_async(m, n, should_copy, IW, D, ldd, L, ldl, U, ldu)
    @ccall libxkblas.xkblas_dcopyscale_async(m::Cint, n::Cint, should_copy::Cint, IW::Ptr{Cint}, D::Ptr{Cdouble}, ldd::Cint, L::Ptr{Cdouble}, ldl::Cint, U::Ptr{Cdouble}, ldu::Cint)::Cint
end

function dgemm_async(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_dgemm_async(transA::Cint, transB::Cint, m::Cint, n::Cint, k::Cint, alpha::Ptr{Cdouble}, A::Ptr{Cdouble}, lda::Cint, B::Ptr{Cdouble}, ldb::Cint, beta::Ptr{Cdouble}, C::Ptr{Cdouble}, ldc::Cint)::Cint
end

function dgemmt_async(uplo, transA, transB, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_dgemmt_async(uplo::Cint, transA::Cint, transB::Cint, n::Cint, k::Cint, alpha::Ptr{Cdouble}, A::Ptr{Cdouble}, lda::Cint, B::Ptr{Cdouble}, ldb::Cint, beta::Ptr{Cdouble}, C::Ptr{Cdouble}, ldc::Cint)::Cint
end

function dherk_async(uplo, transA, n, k, alpha, A, lda, beta, C, ldc)
    @ccall libxkblas.xkblas_dherk_async(uplo::Cint, transA::Cint, n::Cint, k::Cint, alpha::Ptr{Cdouble}, A::Ptr{Cdouble}, lda::Cint, beta::Ptr{Cdouble}, C::Ptr{Cdouble}, ldc::Cint)::Cint
end

function dsyrk_async(uplo, trans, n, k, alpha, A, lda, beta, C, ldc)
    @ccall libxkblas.xkblas_dsyrk_async(uplo::Cint, trans::Cint, n::Cint, k::Cint, alpha::Ptr{Cdouble}, A::Ptr{Cdouble}, lda::Cint, beta::Ptr{Cdouble}, C::Ptr{Cdouble}, ldc::Cint)::Cint
end

function dtrsm_async(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb)
    @ccall libxkblas.xkblas_dtrsm_async(side::Cint, uplo::Cint, transA::Cint, diag::Cint, m::Cint, n::Cint, alpha::Ptr{Cdouble}, A::Ptr{Cdouble}, lda::Cint, B::Ptr{Cdouble}, ldb::Cint)::Cint
end

function dtrmm_async(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb)
    @ccall libxkblas.xkblas_dtrmm_async(side::Cint, uplo::Cint, transA::Cint, diag::Cint, m::Cint, n::Cint, alpha::Ptr{Cdouble}, A::Ptr{Cdouble}, lda::Cint, B::Ptr{Cdouble}, ldb::Cint)::Cint
end

function dsyr2k_async(uplo, trans, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_dsyr2k_async(uplo::Cint, trans::Cint, n::Cint, k::Cint, alpha::Ptr{Cdouble}, A::Ptr{Cdouble}, lda::Cint, B::Ptr{Cdouble}, ldb::Cint, beta::Ptr{Cdouble}, C::Ptr{Cdouble}, ldc::Cint)::Cint
end

function dsymm_async(side, uplo, m, n, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_dsymm_async(side::Cint, uplo::Cint, m::Cint, n::Cint, alpha::Ptr{Cdouble}, A::Ptr{Cdouble}, lda::Cint, B::Ptr{Cdouble}, ldb::Cint, beta::Ptr{Cdouble}, C::Ptr{Cdouble}, ldc::Cint)::Cint
end

function dpotrf_async(uplo, n, A, lda)
    @ccall libxkblas.xkblas_dpotrf_async(uplo::Cint, n::Cint, A::Ptr{Cdouble}, lda::Cint)::Cint
end

function dspmv_async(alpha, transA, nrows, ncols, nnz, csr_row_offsets, csr_col_indices, csr_values, X, beta, Y)
    @ccall libxkblas.xkblas_dspmv_async(alpha::Ptr{Cdouble}, transA::Cint, nrows::Cint, ncols::Cint, nnz::Cint, csr_row_offsets::Ptr{Cint}, csr_col_indices::Ptr{Cint}, csr_values::Ptr{Cdouble}, X::Ptr{Cdouble}, beta::Ptr{Cdouble}, Y::Ptr{Cdouble})::Cint
end

function saxpby_async(n, alpha, x, beta, y)
    @ccall libxkblas.xkblas_saxpby_async(n::Cint, alpha::Cfloat, x::Ptr{Cfloat}, beta::Cfloat, y::Ptr{Cfloat})::Cint
end

function saxpy_async(n, alpha, x, y)
    @ccall libxkblas.xkblas_saxpy_async(n::Cint, alpha::Cfloat, x::Ptr{Cfloat}, y::Ptr{Cfloat})::Cint
end

function sdot_async(x, y, result)
    @ccall libxkblas.xkblas_sdot_async(x::Ptr{Cfloat}, y::Ptr{Cfloat}, result::Ptr{Cfloat})::Cint
end

# no prototype is found for this function at skernels.h:32:9, please use with caution
function sdivcopy_async()
    @ccall libxkblas.xkblas_sdivcopy_async()::Cint
end

function sfill(n, x, v)
    @ccall libxkblas.xkblas_sfill(n::Cint, x::Ptr{Cfloat}, v::Cfloat)::Cint
end

function snrm2_async(n, x, result)
    @ccall libxkblas.xkblas_snrm2_async(n::Cint, x::Ptr{Cfloat}, result::Ptr{Cfloat})::Cint
end

# no prototype is found for this function at skernels.h:38:9, please use with caution
function sscalcopy_async()
    @ccall libxkblas.xkblas_sscalcopy_async()::Cint
end

function sscale_async(n, s, x)
    @ccall libxkblas.xkblas_sscale_async(n::Cint, s::Cfloat, x::Ptr{Cfloat})::Cint
end

function scopyscale_async(m, n, should_copy, IW, D, ldd, L, ldl, U, ldu)
    @ccall libxkblas.xkblas_scopyscale_async(m::Cint, n::Cint, should_copy::Cint, IW::Ptr{Cint}, D::Ptr{Cfloat}, ldd::Cint, L::Ptr{Cfloat}, ldl::Cint, U::Ptr{Cfloat}, ldu::Cint)::Cint
end

function sgemm_async(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_sgemm_async(transA::Cint, transB::Cint, m::Cint, n::Cint, k::Cint, alpha::Ptr{Cfloat}, A::Ptr{Cfloat}, lda::Cint, B::Ptr{Cfloat}, ldb::Cint, beta::Ptr{Cfloat}, C::Ptr{Cfloat}, ldc::Cint)::Cint
end

function sgemmt_async(uplo, transA, transB, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_sgemmt_async(uplo::Cint, transA::Cint, transB::Cint, n::Cint, k::Cint, alpha::Ptr{Cfloat}, A::Ptr{Cfloat}, lda::Cint, B::Ptr{Cfloat}, ldb::Cint, beta::Ptr{Cfloat}, C::Ptr{Cfloat}, ldc::Cint)::Cint
end

function sherk_async(uplo, transA, n, k, alpha, A, lda, beta, C, ldc)
    @ccall libxkblas.xkblas_sherk_async(uplo::Cint, transA::Cint, n::Cint, k::Cint, alpha::Ptr{Cfloat}, A::Ptr{Cfloat}, lda::Cint, beta::Ptr{Cfloat}, C::Ptr{Cfloat}, ldc::Cint)::Cint
end

function ssyrk_async(uplo, trans, n, k, alpha, A, lda, beta, C, ldc)
    @ccall libxkblas.xkblas_ssyrk_async(uplo::Cint, trans::Cint, n::Cint, k::Cint, alpha::Ptr{Cfloat}, A::Ptr{Cfloat}, lda::Cint, beta::Ptr{Cfloat}, C::Ptr{Cfloat}, ldc::Cint)::Cint
end

function strsm_async(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb)
    @ccall libxkblas.xkblas_strsm_async(side::Cint, uplo::Cint, transA::Cint, diag::Cint, m::Cint, n::Cint, alpha::Ptr{Cfloat}, A::Ptr{Cfloat}, lda::Cint, B::Ptr{Cfloat}, ldb::Cint)::Cint
end

function strmm_async(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb)
    @ccall libxkblas.xkblas_strmm_async(side::Cint, uplo::Cint, transA::Cint, diag::Cint, m::Cint, n::Cint, alpha::Ptr{Cfloat}, A::Ptr{Cfloat}, lda::Cint, B::Ptr{Cfloat}, ldb::Cint)::Cint
end

function ssyr2k_async(uplo, trans, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_ssyr2k_async(uplo::Cint, trans::Cint, n::Cint, k::Cint, alpha::Ptr{Cfloat}, A::Ptr{Cfloat}, lda::Cint, B::Ptr{Cfloat}, ldb::Cint, beta::Ptr{Cfloat}, C::Ptr{Cfloat}, ldc::Cint)::Cint
end

function ssymm_async(side, uplo, m, n, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_ssymm_async(side::Cint, uplo::Cint, m::Cint, n::Cint, alpha::Ptr{Cfloat}, A::Ptr{Cfloat}, lda::Cint, B::Ptr{Cfloat}, ldb::Cint, beta::Ptr{Cfloat}, C::Ptr{Cfloat}, ldc::Cint)::Cint
end

function spotrf_async(uplo, n, A, lda)
    @ccall libxkblas.xkblas_spotrf_async(uplo::Cint, n::Cint, A::Ptr{Cfloat}, lda::Cint)::Cint
end

function sspmv_async(alpha, transA, nrows, ncols, nnz, csr_row_offsets, csr_col_indices, csr_values, X, beta, Y)
    @ccall libxkblas.xkblas_sspmv_async(alpha::Ptr{Cfloat}, transA::Cint, nrows::Cint, ncols::Cint, nnz::Cint, csr_row_offsets::Ptr{Cint}, csr_col_indices::Ptr{Cint}, csr_values::Ptr{Cfloat}, X::Ptr{Cfloat}, beta::Ptr{Cfloat}, Y::Ptr{Cfloat})::Cint
end

function init()
    @ccall libxkblas.xkblas_init()::Cint
end

function get_device_count(count)
    @ccall libxkblas.xkblas_get_device_count(count::Ptr{Cint})::Cint
end

function sync()
    @ccall libxkblas.xkblas_sync()::Cvoid
end

function deinit()
    @ccall libxkblas.xkblas_deinit()::Cvoid
end

function memory_segment_coherent_async(ptr, size)
    @ccall libxkblas.xkblas_memory_segment_coherent_async(ptr::Ptr{Cvoid}, size::Csize_t)::Cvoid
end

function memory_matrix_coherent_async(ptr, ld, m, n, sizeof_type)
    @ccall libxkblas.xkblas_memory_matrix_coherent_async(ptr::Ptr{Cvoid}, ld::Csize_t, m::Csize_t, n::Csize_t, sizeof_type::Csize_t)::Cvoid
end

function host_async(func, args)
    @ccall libxkblas.xkblas_host_async(func::Ptr{Cvoid}, args::Ptr{Cvoid})::Cvoid
end

function unified_alloc(size)
    @ccall libxkblas.xkblas_unified_alloc(size::Csize_t)::Ptr{Cvoid}
end

function unified_free(ptr, size)
    @ccall libxkblas.xkblas_unified_free(ptr::Ptr{Cvoid}, size::Csize_t)::Cvoid
end

function host_alloc(size)
    @ccall libxkblas.xkblas_host_alloc(size::Csize_t)::Ptr{Cvoid}
end

function host_free(ptr, size)
    @ccall libxkblas.xkblas_host_free(ptr::Ptr{Cvoid}, size::Csize_t)::Cvoid
end

@cenum xkblas_mode_math_t::UInt32 begin
    XKBLAS_DEFAULT_MATH = 0
    XKBLAS_TENSOR_OP_MATH = 1
end

function set_modemath(mode)
    @ccall libxkblas.xkblas_set_modemath(mode::xkblas_mode_math_t)::Cvoid
end

function register_memory(ptr, sz)
    @ccall libxkblas.xkblas_register_memory(ptr::Ptr{Cvoid}, sz::UInt64)::Cint
end

function unregister_memory(ptr, sz)
    @ccall libxkblas.xkblas_unregister_memory(ptr::Ptr{Cvoid}, sz::UInt64)::Cint
end

function memory_register_tiled_async(ptr, sz, n)
    @ccall libxkblas.xkblas_memory_register_tiled_async(ptr::Ptr{Cvoid}, sz::Csize_t, n::Cint)::Cint
end

function memory_unregister_tiled_async(ptr, sz, n)
    @ccall libxkblas.xkblas_memory_unregister_tiled_async(ptr::Ptr{Cvoid}, sz::Csize_t, n::Cint)::Cint
end

function memory_touch_tiled_async(ptr, sz, n)
    @ccall libxkblas.xkblas_memory_touch_tiled_async(ptr::Ptr{Cvoid}, sz::Csize_t, n::Cint)::Cint
end

function get_ngpus()
    @ccall libxkblas.xkblas_get_ngpus()::Cint
end

function get_nanotime()
    @ccall libxkblas.xkblas_get_nanotime()::UInt64
end

"""
    xkblas_malloc(size)

///////////////////////////////
"""
function malloc(size)
    @ccall libxkblas.xkblas_malloc(size::Csize_t)::Ptr{Cvoid}
end

function free(ptr, size)
    @ccall libxkblas.xkblas_free(ptr::Ptr{Cvoid}, size::Csize_t)::Cvoid
end

function set_param(nb, p)
    @ccall libxkblas.xkblas_set_param(nb::Csize_t, p::Csize_t)::Cvoid
end

function finalize()
    @ccall libxkblas.xkblas_finalize()::Cvoid
end

function memory_invalidate_caches()
    @ccall libxkblas.xkblas_memory_invalidate_caches()::Cvoid
end

function register_memory_async(ptr, sz)
    @ccall libxkblas.xkblas_register_memory_async(ptr::Ptr{Cvoid}, sz::UInt64)::UInt64
end

function unregister_memory_async(ptr, sz)
    @ccall libxkblas.xkblas_unregister_memory_async(ptr::Ptr{Cvoid}, sz::UInt64)::Cint
end

function register_memory_waitall()
    @ccall libxkblas.xkblas_register_memory_waitall()::Cint
end

function memory_coherent_async(uplo, memflag, M, N, A, LD, eltsize)
    @ccall libxkblas.xkblas_memory_coherent_async(uplo::Cint, memflag::Cint, M::Csize_t, N::Csize_t, A::Ptr{Cvoid}, LD::Csize_t, eltsize::Csize_t)::Cint
end

const Complex32_t = ComplexF32

const Complex64_t = ComplexF32

const CFloat64_t = Cdouble

function blas2cblas_trans(trans)
    @ccall libxkblas.xkblas_blas2cblas_trans(trans::Cstring)::Cint
end

function blas2cblas_side(side)
    @ccall libxkblas.xkblas_blas2cblas_side(side::Cstring)::Cint
end

function blas2cblas_fill(uplo)
    @ccall libxkblas.xkblas_blas2cblas_fill(uplo::Cstring)::Cint
end

function blas2cblas_diag(diag)
    @ccall libxkblas.xkblas_blas2cblas_diag(diag::Cstring)::Cint
end

function zaxpby_async(n, alpha, x, beta, y)
    @ccall libxkblas.xkblas_zaxpby_async(n::Cint, alpha::ComplexF32, x::Ptr{ComplexF32}, beta::ComplexF32, y::Ptr{ComplexF32})::Cint
end

function zaxpy_async(n, alpha, x, y)
    @ccall libxkblas.xkblas_zaxpy_async(n::Cint, alpha::ComplexF32, x::Ptr{ComplexF32}, y::Ptr{ComplexF32})::Cint
end

function zdot_async(x, y, result)
    @ccall libxkblas.xkblas_zdot_async(x::Ptr{ComplexF32}, y::Ptr{ComplexF32}, result::Ptr{ComplexF32})::Cint
end

# no prototype is found for this function at zkernels.h:32:9, please use with caution
function zdivcopy_async()
    @ccall libxkblas.xkblas_zdivcopy_async()::Cint
end

function zfill(n, x, v)
    @ccall libxkblas.xkblas_zfill(n::Cint, x::Ptr{ComplexF32}, v::ComplexF32)::Cint
end

function znrm2_async(n, x, result)
    @ccall libxkblas.xkblas_znrm2_async(n::Cint, x::Ptr{ComplexF32}, result::Ptr{Cfloat})::Cint
end

# no prototype is found for this function at zkernels.h:38:9, please use with caution
function zscalcopy_async()
    @ccall libxkblas.xkblas_zscalcopy_async()::Cint
end

function zscale_async(n, s, x)
    @ccall libxkblas.xkblas_zscale_async(n::Cint, s::ComplexF32, x::Ptr{ComplexF32})::Cint
end

function zcopyscale_async(m, n, should_copy, IW, D, ldd, L, ldl, U, ldu)
    @ccall libxkblas.xkblas_zcopyscale_async(m::Cint, n::Cint, should_copy::Cint, IW::Ptr{Cint}, D::Ptr{ComplexF32}, ldd::Cint, L::Ptr{ComplexF32}, ldl::Cint, U::Ptr{ComplexF32}, ldu::Cint)::Cint
end

function zgemm_async(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_zgemm_async(transA::Cint, transB::Cint, m::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function zgemmt_async(uplo, transA, transB, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_zgemmt_async(uplo::Cint, transA::Cint, transB::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function zherk_async(uplo, transA, n, k, alpha, A, lda, beta, C, ldc)
    @ccall libxkblas.xkblas_zherk_async(uplo::Cint, transA::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function zsyrk_async(uplo, trans, n, k, alpha, A, lda, beta, C, ldc)
    @ccall libxkblas.xkblas_zsyrk_async(uplo::Cint, trans::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function ztrsm_async(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb)
    @ccall libxkblas.xkblas_ztrsm_async(side::Cint, uplo::Cint, transA::Cint, diag::Cint, m::Cint, n::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint)::Cint
end

function ztrmm_async(side, uplo, transA, diag, m, n, alpha, A, lda, B, ldb)
    @ccall libxkblas.xkblas_ztrmm_async(side::Cint, uplo::Cint, transA::Cint, diag::Cint, m::Cint, n::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint)::Cint
end

function zsyr2k_async(uplo, trans, n, k, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_zsyr2k_async(uplo::Cint, trans::Cint, n::Cint, k::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function zsymm_async(side, uplo, m, n, alpha, A, lda, B, ldb, beta, C, ldc)
    @ccall libxkblas.xkblas_zsymm_async(side::Cint, uplo::Cint, m::Cint, n::Cint, alpha::Ptr{ComplexF32}, A::Ptr{ComplexF32}, lda::Cint, B::Ptr{ComplexF32}, ldb::Cint, beta::Ptr{ComplexF32}, C::Ptr{ComplexF32}, ldc::Cint)::Cint
end

function zpotrf_async(uplo, n, A, lda)
    @ccall libxkblas.xkblas_zpotrf_async(uplo::Cint, n::Cint, A::Ptr{ComplexF32}, lda::Cint)::Cint
end

function zspmv_async(alpha, transA, nrows, ncols, nnz, csr_row_offsets, csr_col_indices, csr_values, X, beta, Y)
    @ccall libxkblas.xkblas_zspmv_async(alpha::Ptr{ComplexF32}, transA::Cint, nrows::Cint, ncols::Cint, nnz::Cint, csr_row_offsets::Ptr{Cint}, csr_col_indices::Ptr{Cint}, csr_values::Ptr{ComplexF32}, X::Ptr{ComplexF32}, beta::Ptr{ComplexF32}, Y::Ptr{ComplexF32})::Cint
end

# exports
const PREFIXES = [""]
for name in names(@__MODULE__; all=true), prefix in PREFIXES
    if startswith(string(name), prefix)
        @eval export $name
    end
end

