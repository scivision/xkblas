##########################
# INDEPENDENT HOST ASYNC #
##########################

function _host_async_trampoline(fptr::Ptr{Cvoid})
    fref = unsafe_pointer_to_objref(fptr)
    fref[]()
    return
end

function host_async(f::Function)
    cf = @cfunction(_host_async_trampoline, Cvoid, (Ptr{Cvoid},))
    fref = Ref(f)
    host_async(cf, fref)
end

########################
# DEPENDENT HOST ASYNC #
########################

function host_async(f; reads=[], writes=[])
    println("host_async with read/write")
    # TODO: pass to xkrt/xkblas
    # f(reads..., writes...)
end

########################
# Dispatcher for types #
########################

# Memory routines

memory_coherent_async(x, n)         = memory_segment_coherent_async(x, n*sizeof(eltype(x)))
memory_coherent_async(A, lda, m, n) = memory_matrix_coherent_async(A, lda, m, n, sizeof(eltype(A)))

# Kernels (see `xkblas/xkblas.hpp` and convert C++ prototypes)

### Level 1 ###

axpby_async(n, alpha::Float32,    x, beta, y) = saxpby_async(n, alpha, x, b, y)
axpby_async(n, alpha::Float64,    x, beta, y) = daxpby_async(n, alpha, x, b, y)
axpby_async(n, alpha::ComplexF32, x, beta, y) = caxpby_async(n, alpha, x, b, y)
axpby_async(n, alpha::ComplexF64, x, beta, y) = zaxpby_async(n, alpha, x, b, y)

axpy_async(n, alpha::Float32,    x, y)  = saxpy_async(n, alpha, x, y)
axpy_async(n, alpha::Float64,    x, y)  = daxpy_async(n, alpha, x, y)
axpy_async(n, alpha::ComplexF32, x, y)  = caxpy_async(n, alpha, x, y)
axpy_async(n, alpha::ComplexF64, x, y)  = zaxpy_async(n, alpha, x, y)

# TODO: complex version not supported yet, but they will need to change the dispatcher
dot_async(n, x, incx, y, incy, result::Ref{Float32}) = sdot_async(n, x, incx, y, incy, result)
dot_async(n, x, incx, y, incy, result::Ref{Float64}) = ddot_async(n, x, incx, y, incy, result)

# TODO: complex version not supported yet, but they will need to change the dispatcher
scal_async(n, alpha::Float32, x, incx) = sscal_async(n, alpha, x, incx)
scal_async(n, alpha::Float64, x, incx) = dscal_async(n, alpha, x, incx)

### Level 2 ###

gemv_async(transA, m, n, alpha::Float32, A, lda, x, incx, beta, y, incy) = sgemv_async(transA, m, n, Ref(alpha), A, lda, x, incx, beta, y, incy)
gemv_async(transA, m, n, alpha::Float64, A, lda, x, incx, beta, y, incy) = dgemv_async(transA, m, n, Ref(alpha), A, lda, x, incx, beta, y, incy)

### Level 3 ###

gemm_async(transA, transB, m, n, k, alpha::Float32,    A, lda, B, ldb, beta::Float32,    C, ldc)  = sgemm_async(transA, transB, m, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
gemm_async(transA, transB, m, n, k, alpha::Float64,    A, lda, B, ldb, beta::Float64,    C, ldc)  = dgemm_async(transA, transB, m, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
gemm_async(transA, transB, m, n, k, alpha::ComplexF32, A, lda, B, ldb, beta::ComplexF32, C, ldc)  = cgemm_async(transA, transB, m, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
gemm_async(transA, transB, m, n, k, alpha::ComplexF64, A, lda, B, ldb, beta::ComplexF64, C, ldc)  = zgemm_async(transA, transB, m, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)

gemmt_async(uplo, transA, transB, n, k, alpha::Float32,    A, lda, B, ldb, beta::Float32,    C, ldc)  = sgemmt_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
gemmt_async(uplo, transA, transB, n, k, alpha::Float64,    A, lda, B, ldb, beta::Float64,    C, ldc)  = dgemmt_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
gemmt_async(uplo, transA, transB, n, k, alpha::ComplexF32, A, lda, B, ldb, beta::ComplexF32, C, ldc)  = cgemmt_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
gemmt_async(uplo, transA, transB, n, k, alpha::ComplexF64, A, lda, B, ldb, beta::ComplexF64, C, ldc)  = zgemmt_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)

herk_async(uplo, trans, n, k, alpha::Float32,    A, lda, beta::Float32,    C, ldc)  = sherk_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
herk_async(uplo, trans, n, k, alpha::Float64,    A, lda, beta::Float64,    C, ldc)  = dherk_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
herk_async(uplo, trans, n, k, alpha::ComplexF32, A, lda, beta::ComplexF32, C, ldc)  = cherk_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
herk_async(uplo, trans, n, k, alpha::ComplexF64, A, lda, beta::ComplexF64, C, ldc)  = zherk_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)

symm_async(side, uplo, m, n, alpha::Float32,    A, lda, B, ldb, beta::Float32,    C, ldc)  = ssymm_async(uplo, side, m, n, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
symm_async(side, uplo, m, n, alpha::Float64,    A, lda, B, ldb, beta::Float64,    C, ldc)  = dsymm_async(uplo, side, m, n, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
symm_async(side, uplo, m, n, alpha::ComplexF32, A, lda, B, ldb, beta::ComplexF32, C, ldc)  = csymm_async(uplo, side, m, n, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
symm_async(side, uplo, m, n, alpha::ComplexF64, A, lda, B, ldb, beta::ComplexF64, C, ldc)  = zsymm_async(uplo, side, m, n, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)

syr2k_async(uplo, trans, n, k, alpha::Float32,    A, lda, B, ldb, beta::Float32,    C, ldc)  = ssyr2k_async(uplo, trans, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
syr2k_async(uplo, trans, n, k, alpha::Float64,    A, lda, B, ldb, beta::Float64,    C, ldc)  = dsyr2k_async(uplo, trans, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
syr2k_async(uplo, trans, n, k, alpha::ComplexF32, A, lda, B, ldb, beta::ComplexF32, C, ldc)  = csyr2k_async(uplo, trans, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
syr2k_async(uplo, trans, n, k, alpha::ComplexF64, A, lda, B, ldb, beta::ComplexF64, C, ldc)  = zsyr2k_async(uplo, trans, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)

syrk_async(uplo, trans, n, k, alpha::Float32,    A, lda, beta::Float32,    C, ldc)  = ssyrk_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
syrk_async(uplo, trans, n, k, alpha::Float64,    A, lda, beta::Float64,    C, ldc)  = dsyrk_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
syrk_async(uplo, trans, n, k, alpha::ComplexF32, A, lda, beta::ComplexF32, C, ldc)  = csyrk_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)
syrk_async(uplo, trans, n, k, alpha::ComplexF64, A, lda, beta::ComplexF64, C, ldc)  = zsyrk_async(uplo, transA, transB, n, k, Ref(alpha), A, lda, B, ldb, Ref(beta), C, ldc)

trmm_async(side, uplo, transA, diag, m, n, alpha::Float32,    A, lda, B, ldb)  = strmm_async(side, uplo, transA, diag, m, n, Ref(alpha), A, lda, B, ldb)
trmm_async(side, uplo, transA, diag, m, n, alpha::Float64,    A, lda, B, ldb)  = dtrmm_async(side, uplo, transA, diag, m, n, Ref(alpha), A, lda, B, ldb)
trmm_async(side, uplo, transA, diag, m, n, alpha::ComplexF32, A, lda, B, ldb)  = ctrmm_async(side, uplo, transA, diag, m, n, Ref(alpha), A, lda, B, ldb)
trmm_async(side, uplo, transA, diag, m, n, alpha::ComplexF64, A, lda, B, ldb)  = ztrmm_async(side, uplo, transA, diag, m, n, Ref(alpha), A, lda, B, ldb)
