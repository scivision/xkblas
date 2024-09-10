#ifndef __BLAS_VERSION_S_H__
# define __BLAS_VERSION_S_H__

# define PRECISION_s 1
# define TYPE float
# define BLAS_INT long long int
# define native_gemm(...) cblas_sgemm(CblasRowMajor, __VA_ARGS__)

int IONE       = 1;
int PAD[2048]  = {0,0,0,0,0,0,0}; /* pad : Romain, why ? */
lapack_int ISEED[5] = { 7, 1, 2, 1, 1 };
int PAD2[2048] = {0,0,0,0,0,0,0}; /* pad : Romain, why ? */

# define FILL(PTR, N) LAPACKE_slarnv_work(IONE, ISEED, N, PTR)

#endif
