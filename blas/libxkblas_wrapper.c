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

#define _GNU_SOURCE
#include <dlfcn.h>
//#include <stdlib.h>
//#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define LIBNAME "[xkblas]"
#define TRACE_MSG 1

#define KAAPI_NO_INCLUDE_BLAS_H
#include "common.h"
#include "kaapi_impl.h"


#ifndef XKBLAS_BLASLIB
#error "XKBLAS_BLASLIB macro should point to the (absolute) path of the libblas to load "
#endif


/* */
void Usage(void)
{
  printf("XKBLAS_NGPUS:\n");
  printf("XKBLAS_TILE_SIZE:\n");
  printf("XKBLAS_THRESHOLD:\n");
  printf("\n");
  printf("KAAPI_CUDA_KERNEL_STREAM_NUMS:\n");
  abort();
}

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
static void* handle_blas = 0;
static long threshold = 1600;
double threshold_kern[LAST];
char* name_kern[LAST] = {
  "GEN",
  "ZGEMM", "CGEMM", "DGEMM", "SGEMM",
  "ZGEMMT", "CGEMMT", "DGEMMT", "SGEMMT",
  "ZTRSM", "CTRSM", "DTRSM", "STRSM",
  "ZTRMM", "CTRMM", "DTRMM", "STRMM",
  "ZSYMM", "CSYMM", "DSYMM", "SSYMM",
  "ZSYRK", "CSYRK", "DSYRK", "SSYRK",
  "ZSYR2K", "CSYR2K", "DSYR2K", "SSYR2K",
  "ZHEMM", "CHEMM", "DHEMM", "SHEMM",
  "ZHERK", "CHERK", "DHERK", "SHERK",
  "ZHER2K", "CHER2K", "DHER2K", "SHER2K"
};


/*
*/
__attribute__((constructor)) void toto_constructor(void) 
{
  printf(LIBNAME ": library loaded. Blas library '%s'\n",XKBLAS_BLASLIB);

  if (getenv("XKBLAS_HELP"))
    Usage();
  if (getenv("XKBLAS_NGPUS"))
    setenv("KAAPI_NUM_GPUS",getenv("XKBLAS_NGPUS"),1);

  /* */
  for (int i=0; i<LAST; ++i)
    threshold_kern[i] = threshold;

  if (0 != xkblas_init())
  {
    printf(LIBNAME ": cannot initialize\n");
    abort();
  }
  extern const char* get_kaapi_version(void);
  printf(LIBNAME ": version :%s\n", get_kaapi_version());
  printf(LIBNAME ": ngpus :%i\n", kaapi_default_param.ngpus);

  if ((kaapi_default_param.ngpus>0) && (getenv("XKBLAS_THRESHOLD")))
  {
    threshold=atol(getenv("XKBLAS_THRESHOLD"));
    if (threshold <0) threshold = 0;
    for (int i=0; i<LAST; ++i)
      threshold_kern[i] = threshold;
  }
  printf(LIBNAME ": threshold :%i\n", threshold);

  if ((kaapi_default_param.ngpus>0) && (getenv("XKBLAS_THRESHOLD")))
  {
    threshold=atol(getenv("XKBLAS_THRESHOLD"));
    if (threshold <0) threshold = 0;
    threshold_kern[GEN] = threshold;
  }
  printf(LIBNAME ": threshold :%i\n", threshold);
  if (kaapi_default_param.ngpus>0)
  {
    char* name = "XKBLAS_THRESHOLD_PXXYYZZ";
    for (int i=0; i<LAST; ++i)
    {
      strcpy( &name[17], name_kern[i] );
      name[17+strlen(name_kern[i])]=0;
      if (getenv(name))
      {
        long th=atol(getenv(name));
        if (th <0) th = 0;
        threshold_kern[i] = th;
      }
      else
        threshold_kern[i] = threshold;
      printf(LIBNAME ": %i =%i\n", name, threshold_kern[i] );
    }
  }

  handle_blas = dlopen(XKBLAS_BLASLIB,RTLD_LAZY);
  if (handle_blas ==0)
  {
    printf(LIBNAME ": cannot load liblas '%s'\n", XKBLAS_BLASLIB);
    abort();
  }
}

extern void xkblas_load_sym(void** ptr, const char* name)
{
  *ptr = dlsym( handle_blas, name );
  if (*ptr ==0)
  {
    fprintf(stderr,"*** Error: " LIBNAME " cannot load symbol '%s' from '%s'\n", name, XKBLAS_BLASLIB);
    abort();
  }
}

/*
*/
__attribute__((destructor)) void toto_destructor(void) 
{
  if (handle_blas !=0) dlclose(handle_blas);
  handle_blas = 0;
  xkblas_finalize();
}

