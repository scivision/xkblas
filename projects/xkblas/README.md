# Example build
```
rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ CMAKE_PREFIX_PATH=$ONEAPI_ROOT:$CUDA_PATH:$CMAKE_PREFIX_PATH cmake -DUSE_CUDA=on -DUSE_CUBLAS=on -DUSE_SYCL=on -DUSE_MKL=on -DUSE_CLBLAST=on -DUSE_ZE=on -DCMAKE_BUILD_TYPE=Debug ../
```

# Problems on Intel GPUs
- Deadlock / pagefaults of what seems valid transfers
- It is unclear if that code is correct
```
440 void func(ze_event_handle_t * eventPtr)
441 {
442     sycl::event event = oneapi::mkl::blas::column_major::gemm(
443         queue,
444         transa, transb,
445         m, n, k,
446         alpha,
447         a, lda,
448         b, ldb,
449         beta,
450         c, ldc,
451         mode,
452         dependencies
453     );
454     *eventPtr = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(event);
455 }
```
