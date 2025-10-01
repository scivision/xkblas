#!/bin/bash

# Requires
#   LLVM  >= 20.x
#   cmake >= 3.17
#   hwloc

##########################################################################
# Configuration (modify as you want, see CMakeLists.txt for all options) #
##########################################################################

CMAKE_BUILD_TYPE="Debug"
CMAKE_XKAAPI_OPTS="-DUSE_CUDA=on"
CMAKE_XKBLAS_OPTS="-DUSE_CUDA=on -DUSE_CUBLAS=on -DUSE_CUSPARSE=on "
CMAKE_XKBLAS_OPTS+=" -DUSE_MKL=on -DUSE_CBLAS=on -DUSE_TESTS=on" # enable to build tests
WORK_DIRECTORY="$(pwd)"

export CC=clang
export CXX=clang++

###########
# Install #
###########

INSTALL_DIRECTORY="$(pwd)/install"
REPO_DIRECTORY="$(pwd)/repo"
MODULES_DIRECTORY="$(pwd)/modules"

mkdir -p $INSTALL_DIRECTORY
mkdir -p $REPO_DIRECTORY
mkdir -p $MODULES_DIRECTORY

##################
# Install XKAAPI #
##################
git clone https://gitlab.inria.fr/xkaapi/dev-v2.git $REPO_DIRECTORY/xkaapi
mkdir $REPO_DIRECTORY/xkaapi/build
cd $REPO_DIRECTORY/xkaapi/build
CMAKE_PREFIX_PATH=$CUDA_PATH:$CMAKE_PREFIX_PATH cmake $CMAKE_XKAAPI_OPTS -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DCMAKE_INSTALL_PREFIX="$INSTALL_DIRECTORY/xkaapi" $REPO_DIRECTORY/xkaapi
make install -j

##################
# Install XKBlas #
##################
git clone https://gitlab.inria.fr/xkblas/dev $REPO_DIRECTORY/xkblas
mkdir $REPO_DIRECTORY/xkblas/build
cd $REPO_DIRECTORY/xkblas/build
git checkout v2.0
CMAKE_PREFIX_PATH=$CUDA_PATH:$INSTALL_DIRECTORY/xkaapi:$CMAKE_PREFIX_PATH cmake $CMAKE_XKBLAS_OPTS -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DCMAKE_INSTALL_PREFIX="$INSTALL_DIRECTORY/xkblas" $REPO_DIRECTORY/xkblas
make install -j

#######################
# Create module files #
#######################

cd $REPO_DIRECTORY/xkaapi/build
XKAAPI_HASH=$(git rev-parse HEAD | cut -c 1-12)

cd $REPO_DIRECTORY/xkblas/build
git checkout v2.0
XKBLAS_HASH=$(git rev-parse HEAD | cut -c 1-12)

mkdir -p $MODULES_DIRECTORY/xkaapi
mkdir -p $MODULES_DIRECTORY/xkblas

XKAAPI_MODULE_FILE=$MODULES_DIRECTORY/xkaapi/$XKAAPI_HASH-$CMAKE_BUILD_TYPE
XKBLAS_MODULE_FILE=$MODULES_DIRECTORY/xkblas/$XKBLAS_HASH-$CMAKE_BUILD_TYPE

cp $REPO_DIRECTORY/xkaapi/modulefile $XKAAPI_MODULE_FILE
cp $REPO_DIRECTORY/xkaapi/modulefile $XKBLAS_MODULE_FILE

sed -i "s,MY_MODULE_HOME,XKAAPI_HOME,g"                $XKAAPI_MODULE_FILE
sed -i "s,MY_WHATIS,xkaapi,g"                          $XKAAPI_MODULE_FILE
sed -i "s,MY_PREFIX_PATH,$INSTALL_DIRECTORY/xkaapi,g"  $XKAAPI_MODULE_FILE

sed -i "s,MY_MODULE_HOME,XKBLAS_HOME,g"                $XKBLAS_MODULE_FILE
sed -i "s,MY_WHATIS,xkblas,g"                          $XKBLAS_MODULE_FILE
sed -i "s,MY_PREFIX_PATH,$INSTALL_DIRECTORY/xkblas,g"  $XKBLAS_MODULE_FILE

echo "-------------------------------------------------"
echo "Success. You may type
    module use $MODULES_DIRECTORY
    module load xkaapi/$XKAAPI_HASH-$CMAKE_BUILD_TYPE
    module load xkblas/$XKBLAS_HASH-$CMAKE_BUILD_TYPE

Then build the following program with
    clang++ main.cc -lxkblas
"
echo "-------------------------------------------------"

echo "
# include <xkblas/xkblas.h>

int main(void)
{
    xkblas_init();
    xkblas_deinit();
    return 0;
}
"
