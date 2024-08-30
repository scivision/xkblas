#ifndef __S_H__
# define __S_H__

# define TYPE float
# define native_gemm(...) cblas_sgemm(CblasRowMajor, __VA_ARGS__)

#endif
