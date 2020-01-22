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
#include "flops.h"


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

#endif
