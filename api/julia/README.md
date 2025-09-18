# How to test
```
make XKBLAS_BUILD_PATH=/home/rpereira/repo/xktrucs/xkblas/build-debug-mi300x-hip
julia example/gemm.jl
```

# TODO
- Make a proper julia package. This thing only needs a `libxkblas.so` installed
- Add support for host-lambdas. Maybe binding to xkrt first
- Add more examples
