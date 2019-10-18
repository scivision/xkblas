/*
** Copyright 2009-2013,2018,2019 INRIA
**
** Contributors :
**
** thierry.gautier@inrialpes.fr
**
** This software is a computer program whose purpose is to execute
** blas subroutines on multi-GPUs system.
**
** This software is governed by the CeCILL-C license under French law and
** abiding by the rules of distribution of free software.  You can  use,
** modify and/ or redistribute the software under the terms of the CeCILL-C
** license as circulated by CEA, CNRS and INRIA at the following URL
** "http://www.cecill.info".

** As a counterpart to the access to the source code and  rights to copy,
** modify and redistribute granted by the license, users are provided only
** with a limited warranty  and the software's author,  the holder of the
** economic rights,  and the successive licensors  have only  limited
** liability.

** In this respect, the user's attention is drawn to the risks associated
** with loading,  using,  modifying and/or developing or reproducing the
** software by the user in light of its specific status of free software,
** that may mean  that it is complicated to manipulate,  and  that  also
** therefore means  that it is reserved for developers  and  experienced
** professionals having in-depth computer knowledge. Users are therefore
** encouraged to load and test the software's suitability as regards their
** requirements in conditions enabling the security of their systems and/or
** data to be ensured and,  more generally, to use and operate it in the
** same conditions as regards security.

** The fact that you are presently reading this means that you have had
** knowledge of the CeCILL-C license and that you accept its terms.
**/
#ifndef _XKBLAS_WRAPPER_H_
#define _XKBLAS_WRAPPER_H_
#include "flops.h"

#define KAAPI_NO_INCLUDE_BLAS_H
#include "common.h"
#include "kaapi_impl.h"


#ifndef XKBLAS_BLASLIB
#error "XKBLAS_BLASLIB macro should point to the (absolute) path of the libblas to load "
#endif

#define STR_EXPAND2(tok) #tok
#define STR_EXPAND(tok) STR_EXPAND2(tok)

/* Name of kernels for specific thresholds
*/
enum {
  GEN = 0,
  ZGEMM, CGEMM, DGEMM, SGEMM,
  ZGEMMT, CGEMMT, DGEMMT, SGEMMT,
  ZTRSM, CTRSM, DTRSM, STRSM,
  ZTRMM, CTRMM, DTRMM, STRMM,
  ZSYMM, CSYMM, DSYMM, SSYMM,
  ZSYRK, CSYRK, DSYRK, SSYRK,
  ZSYR2K, CSYR2K, DSYR2K, SSYR2K,
  ZHEMM, CHEMM, DHEMM, SHEMM,
  ZHERK, CHERK, DHERK, SHERK,
  ZHER2K, CHER2K, DHER2K, SHER2K,
  LAST
};
extern double threshold_kern[LAST];
extern char* name_kern[LAST];
extern void xkblas_load_sym(void** ptr, const char* name);


#define DATA_MAT(m,n) ((double)(m)*(double)(n))

#define DATA__GEMM(m,n,k) ((DATA_MAT((m),(n))+DATA_MAT((m),(k))+DATA_MAT((k),(n))))
#define DATA_ZGEMM(m,n,k) (1.0*sizeof(double complex)*DATA__GEMM((m),(n),(k)))
#define DATA_CGEMM(m,n,k) (1.0*sizeof(float complex)*DATA__GEMM((m),(n),(k)))
#define DATA_DGEMM(m,n,k) (1.0*sizeof(double)*DATA__GEMM((m),(n),(k)))
#define DATA_SGEMM(m,n,k) (1.0*sizeof(float)*DATA__GEMM((m),(n),(k)))

#define DATA__GEMM(m,n,k) ((DATA_MAT((m),(n))+DATA_MAT((m),(k))+DATA_MAT((k),(n))))
#define DATA_ZGEMM(m,n,k) (1.0*sizeof(double complex)*DATA__GEMM((m),(n),(k)))
#define DATA_CGEMM(m,n,k) (1.0*sizeof(float complex)*DATA__GEMM((m),(n),(k)))
#define DATA_DGEMM(m,n,k) (1.0*sizeof(double)*DATA__GEMM((m),(n),(k)))
#define DATA_SGEMM(m,n,k) (1.0*sizeof(float)*DATA__GEMM((m),(n),(k)))

#define FLOPS_ZGEMMT(n,k) (0.5 * FLOPS_ZGEMM((n), (n), (k)))
#define FLOPS_CGEMMT(n,k) (0.5 * FLOPS_CGEMM((n), (n), (k)))
#define FLOPS_DGEMMT(n,k) (0.5 * FLOPS_DGEMM((n), (n), (k)))
#define FLOPS_SGEMMT(n,k) (0.5 * FLOPS_SGEMM((n), (n), (k)))

#define DATA__GEMMT(n,k) ((0.5*DATA_MAT((n),(n))+DATA_MAT((n),(k))+DATA_MAT((k),(n))))
#define DATA_ZGEMMT(n,k) (1.0*sizeof(double complex)*DATA__GEMMT((n),(k)))
#define DATA_CGEMMT(n,k) (1.0*sizeof(float complex)*DATA__GEMMT((n),(k)))
#define DATA_DGEMMT(n,k) (1.0*sizeof(double)*DATA__GEMMT((n),(k)))
#define DATA_SGEMMT(n,k) (1.0*sizeof(float)*DATA__GEMMT((n),(k)))

#define DATA__TRSM(s,m,n) ((s) == CblasLeft ? (0.5*DATA_MAT((m),(m))+2*DATA_MAT((m),(n))) : (0.5*DATA_MAT((n),(n))+2*DATA_MAT((n),(m))))
#define DATA_ZTRSM(s,m,n) (1.0*sizeof(double complex)*DATA__TRSM((s),(m),(n)))
#define DATA_CTRSM(s,m,n) (1.0*sizeof(float complex)*DATA__TRSM((s),(m),(n)))
#define DATA_DTRSM(s,m,n) (1.0*sizeof(double)*DATA__TRSM((s),(m),(n)))
#define DATA_STRSM(s,m,n) (1.0*sizeof(float)*DATA__TRSM((s),(m),(n)))

#define DATA_ZTRMM DATA_ZTRSM
#define DATA_CTRMM DATA_CTRSM
#define DATA_DTRMM DATA_DTRSM
#define DATA_STRMM DATA_STRSM


#define DATA_ZSYMM(s,m,n) ((s) == CblasLeft ? DATA_ZGEMM(m,m,n) : DATA_ZGEMM(m,n,n))
#define DATA_CSYMM(s,m,n) ((s) == CblasLeft ? DATA_CGEMM(m,m,n) : DATA_CGEMM(m,n,n))
#define DATA_DSYMM(s,m,n) ((s) == CblasLeft ? DATA_DGEMM(m,m,n) : DATA_DGEMM(m,n,n))
#define DATA_SSYMM(s,m,n) ((s) == CblasLeft ? DATA_SGEMM(m,m,n) : DATA_SGEMM(m,n,n))


#define DATA__SYRK(n,k) (0.5*DATA_MAT((n),(n))+DATA_MAT((n),(k)))
#define DATA_ZSYRK(n,k) (1.0*sizeof(double complex)*DATA__SYRK((n),(k)))
#define DATA_CSYRK(n,k) (1.0*sizeof(float complex)*DATA__SYRK((n),(k)))
#define DATA_DSYRK(n,k) (1.0*sizeof(double)*DATA__SYRK((n),(k)))
#define DATA_SSYRK(n,k) (1.0*sizeof(float)*DATA__SYRK((n),(k)))

#define DATA__SYR2K(n,k) (0.5*DATA_MAT((n),(n))+2*DATA_MAT((n),(k)))
#define DATA_ZSYR2K(n,k) (1.0*sizeof(double complex)*DATA__SYR2K((n),(k)))
#define DATA_CSYR2K(n,k) (1.0*sizeof(float complex)*DATA__SYR2K((n),(k)))
#define DATA_DSYR2K(n,k) (1.0*sizeof(double)*DATA__SYR2K((n),(k)))
#define DATA_SSYR2K(n,k) (1.0*sizeof(float)*DATA__SYR2K((n),(k)))


#define DATA_ZHEMM(s,m,n) ((s) == CblasLeft ? DATA_ZGEMM(m,m,n) : DATA_ZGEMM(m,n,n))
#define DATA_CHEMM(s,m,n) ((s) == CblasLeft ? DATA_CGEMM(m,m,n) : DATA_CGEMM(m,n,n))
#define DATA_DHEMM(s,m,n) ((s) == CblasLeft ? DATA_DGEMM(m,m,n) : DATA_DGEMM(m,n,n))
#define DATA_SHEMM(s,m,n) ((s) == CblasLeft ? DATA_SGEMM(m,m,n) : DATA_SGEMM(m,n,n))

#define DATA_ZHERK(n,k) DATA_ZSYRK(n,k)
#define DATA_CHERK(n,k) DATA_CSYRK(n,k)

#define DATA_ZHER2K(n,k) DATA_ZSYR2K(n,k)
#define DATA_CHER2K(n,k) DATA_CSYR2K(n,k)

#endif
