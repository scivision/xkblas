#!/bin/bash

##
## Copyright 2009-2013,2018,2019 INRIA
##
## Contributors :
##
## thierry.gautier@inrialpes.fr
##
## This software is a computer program whose purpose is to execute
## blas subroutines on multi-GPUs system.
##
## This software is governed by the CeCILL-C license under French law and
## abiding by the rules of distribution of free software.  You can  use,
## modify and/ or redistribute the software under the terms of the CeCILL-C
## license as circulated by CEA, CNRS and INRIA at the following URL
## "http://www.cecill.info".

## As a counterpart to the access to the source code and  rights to copy,
## modify and redistribute granted by the license, users are provided only
## with a limited warranty  and the software's author,  the holder of the
## economic rights,  and the successive licensors  have only  limited
## liability.

## In this respect, the user's attention is drawn to the risks associated
## with loading,  using,  modifying and/or developing or reproducing the
## software by the user in light of its specific status of free software,
## that may mean  that it is complicated to manipulate,  and  that  also
## therefore means  that it is reserved for developers  and  experienced
## professionals having in-depth computer knowledge. Users are therefore
## encouraged to load and test the software's suitability as regards their
## requirements in conditions enabling the security of their systems and/or
## data to be ensured and,  more generally, to use and operate it in the
## same conditions as regards security.

## The fact that you are presently reading this means that you have had
## knowledge of the CeCILL-C license and that you accept its terms.
##



#PRECISION = c d s
p=$1
#File with _z inside
f=$2
echo "generate $f for precision $p"

    if [ ! -e "$f" ]; then
echo "File '$f' do not exit"
      exit;
    fi
    if [ "$f" = "zhemm.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "task_zhemm.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "zher2k.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "task_zher2k.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "zherk.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "task_zherk.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "zplghe.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "zplghe.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "testing_zhemm.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "testing_zherk.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$f" = "testing_zher2k.c" ] && [ "$p" != "c" ]; then
      exit
    fi
    if [ "$p" = "z" ]; then
      pm="Z";
      pf="d";
      type=Complex64_t;
      norefvalue=0;
      ctype_format="kaapi_dcplx_format";
      zfloattype="double";
      cutype="cuDoubleComplex";
      ADDR="\&";
    fi
    if [ "$p" = "c" ]; then
      pm="C";
      pf="s";
      type=Complex32_t;
      norefvalue=0;
      ctype_format="kaapi_scplx_format";
      zfloattype="float";
      cutype="cuComplex";
      ADDR="\&";
    fi
    if [ "$p" = "d" ]; then
      pm="D";
      pf="d";
      type=double;
      norefvalue=1;
      ctype_format="kaapi_dbl_format";
      zfloattype="double";
      cutype="double";
      ADDR="";
    fi
    if [ "$p" = "s" ]; then
      pm="S";
      pf="s";
      type=float; 
      norefvalue=1;
      ctype_format="kaapi_flt_format";
      zfloattype="float";
      cutype="float";
      ADDR="";
    fi

    filename=`echo $f | sed -e "s+_z+_${p}+g"`
    echo $filename


    cat $f |grep -v "#define CBLAS_SADDR" > .tmp
    sed -e "s+Complex64_t+$type+g" \
        -e "s+CFloat64_t+$zfloattype+g" \
        -e "s+NOREFZVALUE+$norefvalue+g" \
        -e "s+task_z+task_${p}+g" \
        -e "s+CBLAS_SADDR\((.*)\)+$ADDR\1+g" \
        -e "s+zauxiliary+${p}auxiliary+g" \
        -e "s+ZGEMM+${pm}GEMM+g" \
        -e "s+zgemm+${p}gemm+g" \
        -e "s+Zgemm+${pm}gemm+g" \
        -e "s+ZTRSM+${pm}TRSM+g" \
        -e "s+ztrsm+${p}trsm+g" \
        -e "s+Ztrsm+${pm}trsm+g" \
        -e "s+ZTRMM+${pm}TRMM+g" \
        -e "s+ztrmm+${p}trmm+g" \
        -e "s+Ztrmm+${pm}trmm+g" \
        -e "s+ZSYRK+${pm}SYRK+g" \
        -e "s+zsyrk+${p}syrk+g" \
        -e "s+Zsyrk+${pm}syrk+g" \
        -e "s+ZSYR2K+${pm}SYR2K+g" \
        -e "s+zsyr2k+${p}syr2k+g" \
        -e "s+Zsyr2k+${pm}syr2k+g" \
        -e "s+ZSYMM+${pm}SYMM+g" \
        -e "s+zsymm+${p}symm+g" \
        -e "s+Zsymm+${pm}symm+g" \
        -e "s+ZHEMM+${pm}HEMM+g" \
        -e "s+zhemm+${p}hemm+g" \
        -e "s+Zhemm+${pm}hemm+g" \
        -e "s+ZHERK+${pm}HERK+g" \
        -e "s+zherk+${p}herk+g" \
        -e "s+Zherk+${pm}herk+g" \
        -e "s+ZHER2K+${pm}HER2K+g" \
        -e "s+zher2k+${p}her2k+g" \
        -e "s+Zher2k+${pm}her2k+g" \
        -e "s+ZPOTRF+${pm}POTRF+g" \
        -e "s+zpotrf+${p}potrf+g" \
        -e "s+Zpotrf+${pm}potrf+g" \
        -e "s+cuDoubleComplex+${cutype}+g" \
        -e "s+kaapi_dcplx_format+${ctype_format}+g" .tmp > .tmp2
    mv .tmp2 .tmp
    sed -e "s/zlarn/${p}larn/g" \
        -e "s/zlange/${p}lange/g" \
        -e "s/zaxpy/${p}axpy/g" \
        -e "s/dlamch/${pf}lamch/g" \
        -e "s/dlamch/${pf}lamch/g" .tmp > .tmp2
    mv .tmp2 .tmp
    sed -e "s+PRECISION_z+PRECISION_toto+g" .tmp > .tmp2 
    mv .tmp2 .tmp
    sed -e "s+_z+_${p}+g" .tmp \
        -e "s+PRECISION_toto+PRECISION_z+g" > $filename
    echo "/* this file is automatically generated */"  > .tmp
    cat .tmp $filename > .tmp2
    mv .tmp2 $filename
    rm .tmp
