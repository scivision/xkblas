# Example build
```
rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ CMAKE_PREFIX_PATH=$ONEAPI_ROOT:$CUDA_PATH:$CMAKE_PREFIX_PATH cmake -DUSE_CUDA=on -DUSE_CUBLAS=on -DUSE_SYCL=on -DUSE_MKL=on -DUSE_CLBLAST=on -DCMAKE_BUILD_TYPE=Debug ../
```
