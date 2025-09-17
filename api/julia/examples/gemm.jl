using LinearAlgebra, Random
using XKBlas

# Test matrices
n = 2
m, n, k = n, n, n
A = [Float32(rand()) for _ in 1:(m*k)]
B = [Float32(rand()) for _ in 1:(k*n)]
C = [Float32(0.0)    for _ in 1:(m*n)]

# Scalars as length-1 vectors
alpha_vec = [Float32(1.0)]
beta_vec  = [Float32(0.0)]

# Leading dimensions
lda, ldb, ldc = m, k, m

# Transpose flags
transA, transB = XKBlas.Transpose.N, XKBlas.Transpose.N

# Call the function
XKBlas.init()
XKBlas.sgemm_async(
    transA, transB,
    m, n, k,
    alpha_vec,
    A, lda,
    B, ldb,
    beta_vec,
    C, ldc
)
XKBlas.memory_matrix_coherent_async(C, ldc, m, n, sizeof(Float32))
XKBlas.sync()
XKBlas.deinit()

# Print results
println("XKBlas A = ", reshape(A, m, k))
println("XKBlas B = ", reshape(B, k, n))
println("XKBlas C = ", reshape(C, m, n))

# Optional verification
C_julia = reshape(A, m, k) * reshape(B, k, n)
println(" Julia C = ", C_julia)
