all: build install

build:
	mkdir -p XKBlas
	julia generate.jl -i ${XKBLAS_BUILD_PATH}/api/c/include/xkblas
	sed -i 's/function xkblas_/function /g' XKBlas/src/bindings.jl

install:
	julia install.jl
