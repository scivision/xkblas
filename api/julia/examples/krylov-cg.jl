# launch using
# ```
#   export LBT_FORCE_F2C=plain
#   export LBT_FORCE_RETSTYLE=normal
#   export LBT_FORCE_INTERFACE=ilp64
#   julia examples/krylov-cg.jl
# ```

using Krylov

using LinearAlgebra # norm
using SparseArrays  # spdiagm

# TODO: should use `LBT_DEFAULT_LIBS` instead, but its not working.
# using explicit `lbt_forward` here works though
BLAS.lbt_forward("$(ENV["XKRT_HOME"])/lib/libxkblas_cblas.so")

# Print blas config
println(BLAS.vendor())
println(BLAS.get_config())

# Symmetric and positive definite systems.
function symmetric_definite(n :: Int=10; FC=Float64)
  α = FC <: Complex ? FC(im) : one(FC)
  A = spdiagm(-1 => α * ones(FC, n-1), 0 => 4 * ones(FC, n), 1 => conj(α) * ones(FC, n-1))
  b = A * FC[1:n;]
  return A, b
end

# Run CG
cg_tol = 1.0e-6
for FC in (Float64, ComplexF64)
    A, b = symmetric_definite(FC=FC)
    (x, stats) = cg(A, b, itmax=10)
    r = b - A * x
    resid = norm(r) / norm(b)
    @assert resid ≤ cg_tol "Failure"
    println("Success")
    println(resid)
end
