#!/bin/bash

git_hash_file=$1
if [ -z $git_hash_file ]; then
  echo "Usage: $0 <git_hash_file>"
  echo "  <git_hash_file> is generated into the subdirectory kaapi of the build directory" 
  exit 1
fi
if [ ! -e $git_hash_file ]; then
  echo "Invalid git_hash_file"
  exit 1
fi
suffix=`git describe --tags --long --abbrev=8`
dirname=xkblas-$suffix
mkdir $dirname
cp -r AUTHORS bin blas cmake CMakeLists.txt config COPYING gen_internal_register.sh gen_precision_one.sh gen_precision.sh kaapi LICENCE Makefile make.inc README.md testing $dirname
cp $git_hash_file  $dirname/kaapi
tar cfvz $dirname.tgz $dirname
\rm -rf $dirname
