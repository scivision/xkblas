using LinearAlgebra, Random
using XKBlas

TYPE = Float32

# Problem setup
n = 4
m, n, k = n, n, n
A = [TYPE(rand()) for _ in 1:(m*k)]
B = [TYPE(rand()) for _ in 1:(k*n)]
C = [TYPE(0.0)    for _ in 1:(m*n)]

alpha_vec = TYPE(1.0)
beta_vec  = TYPE(0.0)

lda, ldb, ldc = m, k, m

transA, transB = XKBlas.CblasNoTrans, XKBlas.CblasNoTrans

# Run an XKBlas sequence
XKBlas.init()

@time begin
    XKBlas.gemm_async(
        transA, transB,
        m, n, k,
        alpha_vec,
        A, lda,
        B, ldb,
        beta_vec,
        C, ldc
    )
    XKBlas.memory_coherent_async(C, ldc, m, n)
    XKBlas.sync()
end

XKBlas.deinit()

# Print XKblas and Julia-native results
if (n <= 64)
    println("XKBlas A = ", reshape(A, m, k))
    println("XKBlas B = ", reshape(B, k, n))
    println("XKBlas C = ", reshape(C, m, n))

    C_julia = reshape(A, m, k) * reshape(B, k, n)
    println(" Julia C = ", C_julia)
end
