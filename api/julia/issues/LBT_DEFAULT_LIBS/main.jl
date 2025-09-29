using LinearAlgebra
using Random

BLAS.lbt_forward("/home/rpereira/install/xkrt/debug-a100/lib/libxkblas_cblas.so")
println(BLAS.vendor())
println(BLAS.get_config())


# Generate random single-precision matrices
m, n, k = 3, 3, 3
A = rand(Float32, m, k)
B = rand(Float32, k, n)
C = zeros(Float32, m, n)
α = 1.0f0
β = 0.0f0

BLAS.gemm!('N', 'N', α, A, B, β, C)
