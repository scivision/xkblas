using Random
using SparseArrays
using SparseMatricesCSR
using XKBlas

function random_csr_arrays(m::Int, n::Int; density::Float64=0.2, rng=Random.default_rng())
    # Step 1: generate a random sparse CSC matrix with Float64 values
    A_csc = sprand(rng, m, n, density)  # default element type = Float64

    # Step 2: convert to CSR format
    A_csr = SparseMatrixCSR(A_csc)

    # Step 3: extract internal arrays (do not modify directly unless you know what you’re doing)
    rowptr = A_csr.rowptr      # Vector{Int} of length m+1
    colind = A_csr.colval      # Vector{Int} of column indices
    values = A_csr.nzval       # Vector{Float64} of nonzero values

    println(rowptr)
    println(colind)

    return rowptr, colind, values, A_csr
end

# Example usage
m = 2
n = 2
density=0.2
rows, cols, values, A = random_csr_arrays(m, n, density=density)
nnz = length(values)
index_base = 1
index_type = sizeof(cols[1]) * 8

X = rand(n)
Y = 0.0 * rand(m)
alpha = [1.0]
beta  = [0.0]
transA = XKBlas.CblasNoTrans

XKBlas.init()
XKBlas.dspmv_async(alpha, transA, index_base, index_type, m, n, nnz, rows, cols, values, X, beta, Y)
XKBlas.memory_segment_coherent_async(Y, m * sizeof(Y[1]))
XKBlas.deinit()

println("A =")
display(Matrix(A))  # dense view for clarity
println("X = ", X)

println("XKBlas Y = ", Y)
println(" Julia Y = ", A * X)
