using XKBlas

XKBlas.init()

f = function(args)
    println("Hello from Julia!")
end
cf = @cfunction(f, Cvoid, (Ptr{Cvoid},))
XKBlas.host_async(cf, C_NULL)

XKBlas.sync()
XKBlas.deinit()
