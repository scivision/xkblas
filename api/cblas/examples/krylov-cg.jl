using Krylov

using LinearAlgebra # norm
using SparseArrays  # spdiagm

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
end
