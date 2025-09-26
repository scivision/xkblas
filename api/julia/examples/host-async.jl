using XKBlas

XKBlas.init()

x = 0
f = () -> begin
    global x = 42
end
XKBlas.host_async(f)
XKBlas.sync()
println(string("X is ", x))

XKBlas.deinit()
