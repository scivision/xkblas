module XKBlas

using Libdl

# --- Load the library handle ---
libpath = Libdl.find_library(["libxkblas"], String[])
if libpath === nothing
    error("libxkblas not found. Set update LD_LIBRARY_PATH.")
end
const libxkblas = Libdl.LazyLibrary(libpath)

# --- Include generated bindings ---
const size_t = Csize_t
include("bindings.jl")

# --- Optional high-level wrappers ---
include("wrappers.jl")

end # module
