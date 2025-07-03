# XKBlas v0.6 beta RC

XKBlas is a drop in replacement of blas library for multi-GPUs servers similar
to CUBLASXt but with higher performances especially when matrix dimensions 
becomes smaller.

XKBlas was only developped and tested on Linux plateform with CUDA >= 8.0 on machine with up to 8 GPUs (DGX-1).
It was ported on AMD GPU with HIP/ROCM enviroment = 4.5, 5.X., 6.X Note:
 * ROCM version 4.5.0 has a [buggy TRSM.](https://github.com/ROCmSoftwarePlatform/rocBLAS/blob/develop/CHANGELOG.md?plain=1)


XKBlas was successfully port on top of :
* P100, V100, A100, H100 and GraceHopper. Other NVIDIA GPUs where not tested.
* MI50, MI100 GPU and MI250x GPU, MI300A. Other AMD GPUs were not tested. 

XKBlas is built from 2 components:
*  the multi-GPU module of the XKaapi[1,2] runtime that has low overhead in task management.
*  tile algorithms come from PLASMA [6] or CHAMELEON [7] libraries.

This current version of XKBlas only contains BLAS level 3 algorithms, including XGEMMT:
- XGEMM
- XGEMMT: see [MKL GEMMT](https://software.intel.com/en-us/mkl-developer-reference-fortran-gemmt) interface
- XTRSM
- XTRMM
- XSYMM
- XSYRK
- XSYR2K
- XHEMM
- XHERK
- XHER2K

For classical precision Z, C, D, S.

# Source code
You can clone the projet or get the tarball [here](https://gitlab.inria.fr/xkblas/versions/).

# Installation

XKBlas needs: a CPU blas library (*e.g.* MKL or OpenBLAS or CrayBLAS) and an a CUDA toolkit; or ROCM environment (runtime + hipBLAS/rocBlas).
XKBlas was successfully port on CUDA from version 8 until 11 and on HIP/ROCM 4.5.0 to HIP/ROCM 6.2.X

Previous version of XKBlas where based on updating make.inc to local installation. 
Since version 0.4, XKBlas switches to use CMake in order to simplify configuration.

## Selection of the GPU environment
The two exclusive choices are:
* -DKAAPI_USE_CUDA_RT=ON, default value is OFF. Switch to ON if you want to compile for NVidia environment.
    XKBlas requires: Runtime API and cuBLAS library. 
* -DKAAPI_USE_HIP=ON, default value is OFF. 
    XKBlas requires: rocblas, hipblas, and hip environment.   

## Selection of the CPU BLAS library
Exclusive choices are:
* -DKAAPI_USE_MKL=ON
* -DKAAPI_USE_OPENBLAS=ON
* -DKAAPI_USE_CRAYBLAS=ON. Used to link agains libSCI available with Cray PE. 
* -DKAAPI_USE_NVPL=ON
* -DKAAPI_USE_AOCL=ON



## Selection of using Unified Memory
Port on GraceHopper or MI300A let XKBlas/XKaapi to run using "native" unified memory. Preliminary experiments show that on other architectures, the performances are best using internal explicit memory copy.

To enable unified memory support the flags -DENABLE_KAAPI_UNIFIED=ON should be set on the CMake command line option.

Once compiled, at runtime and conservatively, the XKaapi execution runtime continues to exploit memory copy in place of true unified memory except if the environnement variable XKBLAS_UNIFIED is set to 1.

## Add support for generating execution trace
In case the user want to access to execution trace of its applications; XKBlas is able to generate Gantt chart with BLAS calls.
* -DKAAPI_USE_TRACE=ON
* -DKAAPI_USE_PERFCTR=ON
With performance counter on, one can run applications linked with XKBlas with XKBLAS_VERBOSE=1 on the command line in order
to have some performance counters about the execution. With XKBLAS_VERBOSE=2 more detailed counters are print.

## Compilation of testing executables
This option is to active the compilation of the files in testing subdirectory.
* -DKAAPI_BUILD_TESTING=ON

## Compilation and installation
Then enter
```
> make -j 
> make install 
```

## Examples of command line arguments
Following commands assume to be in a 'build' directory inside the xkblas source code.

On Linux with NVidia CUDA environment and MKL BLAS library:
```
cmake .. -DKAAPI_USE_CUDA_RT=ON -DKAAPI_USE_MKL=ON -DCMAKE_INSTALL_PREFIX=/tmp/xkblas -DKAAPI_USE_TRACE=OFF -DKAAPI_USE_PERFCTR=OFF -DCMAKE_BUILD_TYPE=Release
```

On Linux with HIP and MKL BLAS library:
```
cmake .. -DKAAPI_USE_CUDA_RT=ON -DKAAPI_USE_MKL=ON -DCMAKE_INSTALL_PREFIX=/tmp/xkblas -DKAAPI_USE_TRACE=OFF -DKAAPI_USE_PERFCTR=OFF -DCMAKE_BUILD_TYPE=Release
```

The blas may be difficult to found, so its can be specified on the cmake commande line. For instance with CRAY PE environment:
```
cmake .. -DKAAPI_USE_HIP=ON -DKAAPI_USE_CRAYBLAS=ON -DCMAKE_INSTALL_PREFIX=/tmp/xkblas -DKAAPI_USE_TRACE=OFF -DKAAPI_USE_PERFCTR=OFF -DBLAS_LIBRARIES=${CRAY_LIBSCI_DIR}/CRAY/9.0/x86_64/lib/libsci_cray.so -DBLAS_INCLUDE_DIRS=${CRAY_LIBSCI_DIR}/CRAY/9.0/x86_64/include
```

where ${CRAY_LIBSCI_DIR} is defined when Cray PE module is loaded.


# Testing the installation
XKBlas has testing programs ported from PLASMA/Chameleon.
If configuration option -DKAAPI_BUILD_TESTING=ON was set, the files have to be compiled. Thus, enter
```
> ./testing/run_test
```
All BLAS routines, for each precision and all possible values of trans/side/uplo/diag parameters,
are tested against 3 matrix sizes (1024, 2048, 8192) using all the numbers of GPUs
the host have.
The Failure/Passed status are reported to stdout while all output traces are
added to a file called `log`.

# Compilation of XKBlas program
This step is optional if you want to use the drop-in replacement of the BLAS library,
in the same way NVBLAS traps redirect BLAS calls to equivalent CUBLASXT, then you only need to
run your program with preloading `libXKBlas_blaswrapper.so` (see below).

The installation generates 3 libraries in  `<install directory>/lib`:
* `libkaapi.so`: the low level XKaapi runtime
* `libXKBlas.so`: the XKBlas library which is linked against libkaapi.so
* `libXKBlas_blaswrapper.so`: the BLAS drop-in replacement library **NOT YET AVAILABLE IN V0.4-rc7**

The XKBlas API is defined in `<install directory>/include` repository and the user that want to 
compile XKBlas program with its API should includes `XKBlas.h`.

# Running with XKBlas
Before running any programs, you need to export `<install directory>/lib` in your LD_LIBRARY_PATH variable.

XKaapi manages the computation on GPUs. There is  several environment variables you can use to control execution of your program:
* `XKBLAS_NGPUS`, an integer: the number of GPUs to use for execution. By default it is the number of plugged GPU on the local server.
* `XKBLAS_GPUSET`, an integer representing the bit set of GPUs to use. The i-th bit is equal to 1 in the number iff the GPU `i` would be used.
for instance XKBLAS_GPUSET=5 (=0b101) selects GPU id 0 and GPU id 2.
* `XKBLAS_CACHE_LIMIT`, an integer (default 95) >0 and less or equal to 100 representing the percent of GPU memory that XKBlas may use for caching tiles.
* `XKBLAS_NSTREAMS`, an integer (default 4) that gives the number of CUDA streams for launching kernel to GPU
* `XKBLAS_NKERNELS`, an integer (default 2) that gives the maximum of CUDA kernels pending into each CUDA stream for launching kernel.


For instance, if you want to run with XKBlas as a drop in replacement BLAS library:
```
> LD_PRELOAD=<install directory>/lib/libXKBlas_blaswrapper.so XKBLAS_NGPUS=2 ./prog arg
```


# Any support ?
Please contact us or fill an [issue here](https://gitlab.inria.fr/xkblas/versions/-/issues/new).


# References
* [1] Thierry Gautier, João V. F. Lima: XKBlas: a High Performance Implementation of BLAS-3 Kernels on Multi-GPU Server. PDP 2020, Västerås, Sweden, March 11-13, 2020. IEEE 2020, 
* [2] Thierry Gautier, João V. F. Lima: XKBlas: a High Performance Implementation of BLAS-3 Kernels on Multi-GPU Server. PDP 2020, Västerås, Sweden, March 11-13, 2020. IEEE 2020, 
* [3] João V. F. Lima, Thierry Gautier, Vincent Danjean, Bruno Raffin, Nicolas Maillard. Design and analysis of scheduling strategies for multi-CPU and multi-GPU architectures. Parallel Computing 44: 37-52 (2015)
* [4] Thierry Gautier, Joao Vicente Ferreira Lima, Nicolas Maillard, Bruno Raffin. XKaapi: A Runtime System for Data-Flow Task Programming on Heterogeneous Architectures. In Proc. of the 27-th IEEE International Parallel and Distributed Processing Symposium (IPDPS), Boston, USA, jun 2013.
* [5] João V. F. Lima, Thierry Gautier, Nicolas Maillard, Vincent Danjean. Exploiting Concurrent GPU Operations for Efficient Work Stealing on Multi-GPUs. 24rd International Symposium on Computer Architecture and High Performance Computing (SBAC-PAD), Columbia University, New York, USA, oct 2012.
* [6] http://icl.cs.utk.edu/plasma/software/
* [7] https://solverstack.gitlabpages.inria.fr/chameleon/doxygen/index.html


