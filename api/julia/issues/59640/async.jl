using Libdl

libpath = Libdl.find_library(["libasync"], String[])
if libpath === nothing
    error("libasync not found. Set LD_LIBRARY_PATH")
end
const libasync = Libdl.LazyLibrary(libpath)

# convert julia func to C pointer
f = function()
    println("Hello from Julia!")
end
cf = @cfunction(f, Cvoid, ())
@ccall libasync.async(cf::Ptr{Cvoid})::Cvoid

# wait for completion
@ccall libasync.sync()::Cvoid
