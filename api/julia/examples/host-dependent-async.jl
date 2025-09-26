using XKBlas

XKBlas.init()

x = 0
y = 1
z = 2
w = 42
f = (x, y, z, w) -> begin
    global w = x + z * y
end

XKBlas.host_async(f, reads=[x, y, z], writes=[w])
XKBlas.sync()

println(string("w is ", w))

XKBlas.deinit()
