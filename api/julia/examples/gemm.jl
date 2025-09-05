using LinearAlgebra, Random
using XKBlas

# Test matrices
m, n, k = 2, 2, 2
A = [Float32(rand()) for _ in 1:(m*k)]
B = [Float32(rand()) for _ in 1:(k*n)]
C = [Float32(0.0) for _ in 1:(m*n)]

# Scalars as length-1 vectors
alpha_vec = [Float32(1.0)]
beta_vec  = [Float32(0.0)]

# Leading dimensions
lda, ldb, ldc = Cint(m), Cint(k), Cint(m)

# Transpose flags
transA, transB = XKBlas.CblasNoTrans, XKBlas.CblasNoTrans

# Call the function
XKBlas.init()
XKBlas.sgemm_async(transA, transB, Cint(m), Cint(n), Cint(k),
                   pointer(alpha_vec), pointer(A), lda,
                   pointer(B), ldb, pointer(beta_vec), pointer(C), ldc)
XKBlas.sync()
XKBlas.deinit()

# Print results
println("Matrix A = ", reshape(A, m, k))
println("Matrix B = ", reshape(B, k, n))
println("Matrix C = ", reshape(C, m, n))

# Optional verification
C_julia = reshape(A, m, k) * reshape(B, k, n)
println("Julia C = ", C_julia)

