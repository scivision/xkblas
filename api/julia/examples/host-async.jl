using XKBlas

XKBlas.init()

f = function(args)
    print("Hello from Julia callback!")
end
cf = @cfunction(f, Cvoid, (Ptr{Cvoid},))
XKBlas.host_async(cf, C_NULL)

XKBlas.sync()
XKBlas.deinit()
