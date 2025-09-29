# launch using
# ```
#   export LBT_FORCE_F2C=plain
#   export LBT_FORCE_RETSTYLE=normal
#   export LBT_FORCE_INTERFACE=ilp64
#   julia examples/krylov-cg.jl
# ```

using LinearAlgebra, Random

TYPE=Float64
#TYPE=ComplexF64

BLAS.lbt_forward("$(ENV["XKRT_HOME"])/lib/libxkblas_cblas.so")
println(BLAS.vendor())
println(BLAS.get_config())

# Problem setup
n = 4
m, n, k = n, n, n
A = reshape([TYPE(rand()) for _ in 1:(m*k)], m, k)
B = reshape([TYPE(rand()) for _ in 1:(k*n)], k, n)
C = reshape([TYPE(0.0)    for _ in 1:(m*n)], m, n)

alpha = TYPE(1.0)
beta  = TYPE(0.0)

lda, ldb, ldc = m, k, m

BLAS.gemm!('N', 'N', alpha, A, B, beta, C)

# Print XKblas and Julia-native results
if (n <= 64)
    println("XKBlas A = ", A)
    println("XKBlas B = ", B)
    println("XKBlas C = ", C)

    C_julia = alpha * A * B + beta * C
    println(" Julia C = ", C_julia)
end
