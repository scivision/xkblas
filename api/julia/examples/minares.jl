using XKBlas

XKBlas.init()   # TODO NEW

# Just a representative portion of the minares solver
n = 4

A     = [Float32(rand()) for _ in 1:(n*n)]

q     = [Float32(rand()) for _ in 1:(n*1)]
vₖ    = [Float32(rand()) for _ in 1:(n*1)]
vₖ₊₁  = [Float32(rand()) for _ in 1:(n*1)]

βₖ₊₁  =  Float32(rand())
βₖ₊₂  =  Float32(rand())
αₖ₊₁  =  Float32(rand())
cₖ    =  Float32(rand())
sₖ    =  Float32(rand())
γbarₖ =  Float32(rand())

# idk about these yet
ℓ    = 16
iter = 0

######################
# SOLVER STARTS HERE #
######################

# Continue the Lanczos process.
# M(A + λI)Vₖ₊₁ = Vₖ₊₂Tₖ₊₂.ₖ₊₁
# βₖ₊₂vₖ₊₂ = M(A + λI)vₖ₊₁ - αₖ₊₁vₖ₊₁ - βₖ₊₁vₖ
if iter ≤ ℓ-1
    mul(q, A, vₖ₊₁)                    # q ← Avₖ
    kaxpby(n, one(T), q, -βₖ₊₁, vₖ)    # Forms vₖ₊₂ : vₖ ← Avₖ₊₁ - βₖ₊₁vₖ
    if λ ≠ 0
        kaxpy(n, λ, vₖ₊₁, vₖ)          # vₖ ← vₖ + λvₖ₊₁
    end
    αₖ₊₁ = kdotr(n, vₖ, vₖ₊₁)           # αₖ₊₁ = ⟨(A + λI)vₖ₊₁ - βₖ₊₁vₖ , vₖ₊₁⟩
    kaxpy!(n, -αₖ₊₁, vₖ₊₁, vₖ)          # vₖ ← vₖ - αₖ₊₁vₖ₊₁
    βₖ₊₂ = knorm(n, vₖ)                 # βₖ₊₂ = ‖vₖ₊₂‖

    # Detection of early termination
    if βₖ₊₂ ≤ btol
        ℓ = iter + 1
    else
        kdiv!(n, vₖ, βₖ₊₂)
    end
end

XKBlas.sync()   # do this instead

# Apply the Givens reflection Qₖ.ₖ₊₁
f = () -> begin                         # TODO NEW
   if iter ≤ ℓ-2
       ϵₖ      =  sₖ * βₖ₊₂
       γbarₖ₊₁ = -cₖ * βₖ₊₂
   end

   if iter ≤ ℓ-1
       γₖ      = cₖ * γbarₖ + sₖ * αₖ₊₁
       λbarₖ₊₁ = sₖ * γbarₖ - cₖ * αₖ₊₁
   end
end                                     # TODO NEW
XKBlas.host_async(f, reads=[sₖ, βₖ₊₂, cₖ, γbarₖ, αₖ₊₁], writes=[ϵₖ, γbarₖ₊₁, γₖ])   # TODO NEW

XKBlas.sync()   # TODO NEW

####################
# SOLVER ENDS HERE #
####################

XKBlas.deinit() # TODO NEW
